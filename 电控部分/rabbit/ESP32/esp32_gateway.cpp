#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp_err.h>
#include <esp_system.h>
#include <nvs.h>
#include "FocLinkProtocol.h"
#include "GatewayValidation.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select ESP32S3 Dev Module"
#endif

/* The old gateway's website/MQTT/NVS contract remains intact. The sole
 * downstream change is FOC SPI v2 -> generic STM32 task SPI v3. */
static constexpr uint8_t SPI_CS = 15U, SPI_SCK = 13U, SPI_MOSI = 12U, SPI_MISO = 14U;
static constexpr uint32_t SPI_HZ = 100000U, SPI_INTERVAL_MS = 100U;
SPIClass spi(FSPI);
WiFiClient transport;
PubSubClient mqtt(transport);
WebServer web(80);
gateway::Config config{};
gateway::Lease lease{};
FlpStatus lastStatus{};
char topicCmd[96] = {}, topicAck[96] = {}, topicStatus[96] = {}, sessionId[33] = {};
char csrf[33] = {}, apPassword[20] = {}, recent[16][48] = {};
uint8_t recentIndex = 0U;
uint32_t nextId = 0U, txCount = 0U, rejectedCount = 0U, lastFrameAt = 0U, lastReplyAt = 0U;
uint32_t lastStatusAt = 0U, networkEpoch = 0U, portalSince = 0U, restartAt = 0U;
bool configured = false, portal = false, localOnly = false, localNetworkPaused = false;
bool spiReady = false, spiSeen = false, statusFresh = false, linkVerified = false, netOnline = false;

static void randomToken(char *out, size_t capacity) {
  snprintf(out, capacity, "%08lx%08lx", static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
}
static bool loadConfig() {
  Preferences preferences;
  if (!preferences.begin("rabbitnet", true)) return false;
  const bool loaded = preferences.getBytesLength("config") == sizeof(config) && preferences.getBytes("config", &config, sizeof(config)) == sizeof(config);
  preferences.end();
  return loaded && gateway::validConfig(config);
}
/* v8.2.2 direct-NVS save diagnostics; same rabbitnet/config blob as old code. */
static bool networkSaveFailure(const char *stage, esp_err_t error, size_t expected = 0U, size_t actual = 0U) {
  Serial.printf("NET_SAVE_FAIL stage=%s err=0x%08lX (%s) expected=%lu actual=%lu\n", stage, static_cast<unsigned long>(error), esp_err_to_name(error), static_cast<unsigned long>(expected), static_cast<unsigned long>(actual));
  nvs_stats_t stats{}; const esp_err_t statsError = nvs_get_stats(nullptr, &stats);
  if (statsError == ESP_OK) Serial.printf("NET_SAVE_NVS used=%lu free=%lu available=%lu total=%lu namespaces=%lu\n", static_cast<unsigned long>(stats.used_entries), static_cast<unsigned long>(stats.free_entries), static_cast<unsigned long>(stats.available_entries), static_cast<unsigned long>(stats.total_entries), static_cast<unsigned long>(stats.namespace_count));
  else Serial.printf("NET_SAVE_NVS stats_error=0x%08lX (%s)\n", static_cast<unsigned long>(statsError), esp_err_to_name(statsError));
  return false;
}
static bool persistConfig(const gateway::Config &next) {
  nvs_handle_t handle = 0; esp_err_t error = nvs_open("rabbitnet", NVS_READWRITE, &handle);
  if (error != ESP_OK) return networkSaveFailure("open", error);
  error = nvs_set_blob(handle, "config", &next, sizeof(next));
  if (error != ESP_OK) { nvs_close(handle); return networkSaveFailure("write", error, sizeof(next)); }
  error = nvs_commit(handle);
  if (error != ESP_OK) { nvs_close(handle); return networkSaveFailure("commit", error, sizeof(next)); }
  gateway::Config verify{}; size_t actual = sizeof(verify); error = nvs_get_blob(handle, "config", &verify, &actual); nvs_close(handle);
  if (error != ESP_OK) return networkSaveFailure("readback", error, sizeof(next), actual);
  if (actual != sizeof(next)) return networkSaveFailure("verify_length", ESP_ERR_INVALID_SIZE, sizeof(next), actual);
  if (memcmp(&next, &verify, sizeof(next)) != 0) return networkSaveFailure("verify_mismatch", ESP_FAIL, sizeof(next), actual);
  Serial.printf("NET_SAVE_OK bytes=%lu\n", static_cast<unsigned long>(actual)); return true;
}
static bool clearConfig() {
  Preferences preferences; if (!preferences.begin("rabbitnet", false)) return false;
  const bool result = preferences.clear(); preferences.end(); return result;
}
static void buildTopics() {
  snprintf(topicCmd, sizeof(topicCmd), "rabbit/%s/cmd", config.device);
  snprintf(topicAck, sizeof(topicAck), "rabbit/%s/ack", config.device);
  snprintf(topicStatus, sizeof(topicStatus), "rabbit/%s/status", config.device);
}
static void publishAck(const char *request, const char *result) {
  char body[320];
  snprintf(body, sizeof(body), "{\"device_id\":\"%s\",\"request_id\":\"%s\",\"result\":\"%s\",\"tx\":%lu,\"armed\":%u}", config.device, request, result, static_cast<unsigned long>(txCount), lease.armed ? 1U : 0U);
  mqtt.publish(topicAck, body, false);
}

static bool exchange(const uint8_t *request, uint8_t *response) {
  if (!spiReady) return false;
  const uint32_t elapsed = millis() - lastFrameAt;
  if (txCount && elapsed < SPI_INTERVAL_MS) delay(SPI_INTERVAL_MS - elapsed);
  spi.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS, LOW); delayMicroseconds(4U); spi.transferBytes(request, response, FLP_SIZE); delayMicroseconds(4U); digitalWrite(SPI_CS, HIGH); spi.endTransaction();
  lastFrameAt = millis(); ++txCount; spiSeen = true; return true;
}
static bool readReply(const uint8_t *request, FlpStatus &status) {
  uint8_t response[FLP_SIZE] = {};
  if (!exchange(request, response) || !flp_read_reply(response, &status)) return false;
  lastStatus = status; lastReplyAt = millis(); statusFresh = true;
  if (status.result == FLP_RESULT_PONG) linkVerified = true;
  return true;
}
static bool sendSpiTask(FlpTask task, FlpStatus &status) {
  if (++nextId == 0U) ++nextId;
  uint8_t command[FLP_SIZE] = {}, poll[FLP_SIZE] = {}, ignored[FLP_SIZE] = {};
  if (!flp_request(command, FLP_COMMAND, nextId, task, 0U, 0U, false) || !flp_request(poll, FLP_POLL, 0U, FLP_TASK_NONE, 0U, 0U, false) || !exchange(command, ignored)) return false;
  return readReply(poll, status) && status.id == nextId;
}
static void pollSpi(bool keepalive) {
  uint8_t poll[FLP_SIZE] = {}; FlpStatus status{};
  if (flp_request(poll, FLP_POLL, 0U, FLP_TASK_NONE, 0U, 0U, keepalive)) (void)readReply(poll, status);
}
static bool taskFor(const char *command, FlpTask &task) {
  if (!strcmp(command, "PING")) task = FLP_TASK_PING;
  else if (!strcmp(command, "ARM")) task = FLP_TASK_ARM;
  else if (!strcmp(command, "FEED")) task = FLP_TASK_FEED;
  else if (!strcmp(command, "CLEAN")) task = FLP_TASK_CLEAN;
  else if (!strcmp(command, "RESET_FAULT")) task = FLP_TASK_RESET_FAULT;
  else if (gateway::isStop(command)) task = FLP_TASK_STOP;
  else return false;
  return true;
}
/* Historic reply names are retained for the website.  New generic state is
 * exposed additively in status, rather than faking old RPM/encoder fields. */
static const char *compatibilityResult(bool ok, const FlpStatus &status) {
  if (!ok) return "foc_timeout";
  if (status.result == FLP_RESULT_PONG) return "foc_pong";
  if (status.result == FLP_RESULT_ACCEPTED) return "foc_accepted";
  return "foc_rejected";
}
static const char *execute(const gateway::Command &command, bool remote) {
  const char *text = command.text;
  if (remote && localOnly && !gateway::isStop(text)) return "rejected_local_mode";
  if (!strcmp(text, "KEEPALIVE")) {
    if (!remote || !lease.armed || !lease.remote) return "rejected_not_armed";
    lease.renew(millis()); return "accepted_local";
  }
  if (!strcmp(text, "ARM") && !linkVerified) return "rejected_foc_unverified";
  if (!lease.armed && !gateway::isStop(text) && strcmp(text, "PING") && strcmp(text, "ARM") && strcmp(text, "RESET_FAULT")) return "rejected_not_armed";
  if (lease.armed && lease.remote != remote && !gateway::isStop(text)) return "rejected_owner";
  FlpTask task{}; if (!taskFor(text, task)) return "rejected_bad_command";
  FlpStatus status{}; const bool received = sendSpiTask(task, status);
  if (!strcmp(text, "ARM") && received && status.result == FLP_RESULT_ACCEPTED) lease.arm(remote, networkEpoch, millis());
  if (gateway::isStop(text)) lease.lock();
  if (received && remote && lease.armed) lease.renew(millis());
  return compatibilityResult(received, status);
}

static String escapeHtml(const char *text) { String value(text); value.replace("&", "&amp;"); value.replace("\"", "&quot;"); value.replace("<", "&lt;"); value.replace(">", "&gt;"); return value; }
static bool fromAp() { return portal && web.client().localIP() == WiFi.softAPIP(); }
static bool protectPost() {
  if (!fromAp() || web.arg("token") != csrf) { web.send(403, "text/plain; charset=utf-8", "请连接配置热点并重新打开页面"); return false; }
  if (lease.armed) { web.send(409, "text/plain; charset=utf-8", "请先停止并锁定设备"); return false; }
  return true;
}
template <size_t Size> static bool copyField(const String &source, char (&destination)[Size]) {
  if (source.length() >= Size) return false;
  for (size_t i = 0U; i < source.length(); ++i) if (source[i] == '\0' || static_cast<uint8_t>(source[i]) < 32U) return false;
  memcpy(destination, source.c_str(), source.length() + 1U); return true;
}
static void page() {
  if (!fromAp()) { web.send(403, "text/plain", "AP only"); return; }
  web.sendHeader("Cache-Control", "no-store");
  String html = R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Rabbit 网络配置</title><style>body{font-family:system-ui;max-width:540px;margin:20px auto;padding:16px}label{display:block;margin:12px 0}input:not([type=checkbox]){width:100%;box-sizing:border-box;padding:8px}button{padding:10px}</style><h2>ESP32 WiFi / MQTT 配置</h2><p>仅配置网络，不控制 STM32。使用 2.4GHz WiFi；当前仅支持局域网明文 MQTT。</p><form method="post" action="/save"><input type="hidden" name="token" value=")HTML";
  html += csrf; html += "\"><label>WiFi 名称<input name=ssid maxlength=32 required value=\""; html += configured ? escapeHtml(config.ssid) : "";
  html += "\"></label><label>WiFi 密码（留空保留原密码）<input name=wpass type=password maxlength=63></label><label><input type=checkbox name=open value=1>此 WiFi 无密码</label><label>MQTT 域名或 IPv4（不要填 http://）<input name=host maxlength=127 required value=\""; html += configured ? escapeHtml(config.host) : "";
  html += "\"></label><label>端口<input name=port type=number min=1 max=65535 required value=\""; html += String(configured ? config.port : 1883U);
  html += "\"></label><label>MQTT 用户名<input name=user maxlength=64 value=\""; html += configured ? escapeHtml(config.username) : "";
  html += "\"></label><label>MQTT 密码（留空保留原密码）<input name=pass type=password maxlength=128></label><label><input type=checkbox name=clearpass value=1>清空 MQTT 密码</label><label>设备 ID<input name=device maxlength=39 required value=\""; html += configured ? escapeHtml(config.device) : "rabbit-foc-001";
  html += "\"></label><button>保存并重启</button></form><hr><form method=post action=/clear><input type=hidden name=token value=\""; html += csrf;
  html += "\"><label><input type=checkbox name=confirm value=YES required>确认清除网络配置</label><button>清除并重启</button></form></html>";
  web.send(200, "text/html; charset=utf-8", html);
}
static void savePage() {
  if (!protectPost()) return;
  gateway::Config next{}; String host = web.arg("host"), device = web.arg("device"), wifiPassword = web.arg("wpass"), mqttPassword = web.arg("pass"); host.trim(); device.trim();
  if (web.arg("open") == "1") wifiPassword = ""; else if (wifiPassword.isEmpty() && configured) wifiPassword = config.wifiPassword;
  if (web.arg("clearpass") == "1") mqttPassword = ""; else if (mqttPassword.isEmpty() && configured) mqttPassword = config.password;
  const bool copied = copyField(web.arg("ssid"), next.ssid) && copyField(wifiPassword, next.wifiPassword) && copyField(host, next.host) && copyField(web.arg("user"), next.username) && copyField(mqttPassword, next.password) && copyField(device, next.device) && gateway::parsePort(web.arg("port").c_str(), next.port);
  gateway::seal(next);
  if (!copied || !gateway::validConfig(next)) { web.send(400, "text/plain; charset=utf-8", "参数无效，请检查 WiFi 密码、端口与设备 ID。"); return; }
  if (!persistConfig(next)) { web.send(500, "text/plain; charset=utf-8", "保存失败，未安排重启，请重试并检查串口。"); return; }
  web.send(200, "text/plain; charset=utf-8", "保存成功，即将重启。"); restartAt = millis() + 1000U;
}
static void clearPage() {
  if (!protectPost()) return;
  if (web.arg("confirm") != "YES") { web.send(400, "text/plain", "Confirmation required"); return; }
  if (!clearConfig()) { web.send(500, "text/plain", "NVS clear failed"); return; }
  web.send(200, "text/plain; charset=utf-8", "配置已清除，即将重启"); restartAt = millis() + 1000U;
}
static void startPortal() {
  if (lease.armed) { Serial.println("ERR STOP_BEFORE_CONFIG"); return; }
  if (portal) return;
  portal = true; localOnly = false; netOnline = false; randomToken(csrf, sizeof(csrf));
  snprintf(apPassword, sizeof(apPassword), "Rbt-%08lx", static_cast<unsigned long>(esp_random()));
  char name[40]; snprintf(name, sizeof(name), "Rabbit-Setup-%06lx", static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFU));
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(name, apPassword)) { portal = false; Serial.println("ERR AP_START_FAILED"); return; }
  web.on("/", HTTP_GET, page); web.on("/save", HTTP_POST, savePage); web.on("/clear", HTTP_POST, clearPage); web.onNotFound([]() { web.send(404, "text/plain", "Not found"); }); web.begin(); portalSince = millis();
  Serial.printf("PROVISION_AP ssid=%s password=%s url=http://%s/\n", name, apPassword, WiFi.softAPIP().toString().c_str());
}

static void publishStatus() {
  const unsigned long age = statusFresh ? static_cast<unsigned long>(millis() - lastReplyAt) : 0xffffffffUL;
  char body[900];
  snprintf(body, sizeof(body), "{\"device_id\":\"%s\",\"event\":\"heartbeat\",\"mqtt\":%u,\"wifi\":%u,\"spi_ready\":%u,\"armed\":%u,\"tx\":%lu,\"rejected\":%lu,\"session_id\":\"%s\",\"lease_ms\":5000,\"protocol\":3,\"foc_link\":\"%s\",\"stm32_link\":\"%s\",\"status_valid\":%u,\"status_age_ms\":%lu,\"sample\":%lu,\"stm32_uptime_ms\":%lu,\"state\":%u,\"active_task\":%u,\"last_task\":%u,\"fault_code\":%lu,\"self_test_passed\":%lu,\"self_test_failed\":%lu,\"can_dropped\":%lu,\"transport_error\":%lu,\"bad_frames\":%lu,\"simulation\":%u}", config.device, mqtt.connected() ? 1U : 0U, WiFi.status() == WL_CONNECTED ? 1U : 0U, spiSeen ? 1U : 0U, lease.armed ? 1U : 0U, static_cast<unsigned long>(txCount), static_cast<unsigned long>(rejectedCount), sessionId, linkVerified ? "verified" : "unverified", linkVerified ? "verified" : "unverified", statusFresh ? 1U : 0U, age, static_cast<unsigned long>(lastStatus.sample), static_cast<unsigned long>(lastStatus.uptime), lastStatus.state, lastStatus.activeTask, lastStatus.lastTask, static_cast<unsigned long>(lastStatus.faultCode), static_cast<unsigned long>(lastStatus.selfTestPassed), static_cast<unsigned long>(lastStatus.selfTestFailed), static_cast<unsigned long>(lastStatus.canDropped), static_cast<unsigned long>(lastStatus.transportError), static_cast<unsigned long>(lastStatus.badFrames), (lastStatus.linkFlags & 1U) ? 1U : 0U);
  mqtt.publish(topicStatus, body, true); lastStatusAt = millis();
}
static void handleMqtt(char *, uint8_t *payload, unsigned int length) {
  gateway::Command command{};
  if (length == 4U && !memcmp(payload, "PING", 4U)) strcpy(command.text, "PING");
  else if (length == 4U && !memcmp(payload, "STOP", 4U)) strcpy(command.text, "STOP");
  else if (!gateway::parseCommand(payload, length, command)) { ++rejectedCount; publishAck("", "rejected_bad_json"); return; }
  if (!gateway::remoteAllowed(command.text)) { ++rejectedCount; publishAck(command.request, "rejected_local_only"); return; }
  if (!gateway::sessionAllowed(command, sessionId)) { ++rejectedCount; publishAck(command.request, "rejected_stale_session"); return; }
  if (portal) { ++rejectedCount; publishAck(command.request, "rejected_config_mode"); return; }
  const bool stop = gateway::isStop(command.text);
  if (!stop && command.request[0]) {
    for (const auto &request : recent) if (!strcmp(command.request, request)) { publishAck(command.request, "duplicate_ignored"); return; }
    strcpy(recent[recentIndex], command.request); recentIndex = static_cast<uint8_t>((recentIndex + 1U) % 16U);
  }
  const char *result = execute(command, true); if (!strncmp(result, "rejected", 8U)) ++rejectedCount; publishAck(command.request, result);
}
static void reconnectMqtt() {
  static uint32_t retryAt = 0U;
  if (portal || localOnly || WiFi.status() != WL_CONNECTED || mqtt.connected() || millis() - retryAt < 3000U) return;
  retryAt = millis(); randomToken(sessionId, sizeof(sessionId)); ++networkEpoch; lease.lock(); linkVerified = false; memset(recent, 0, sizeof(recent)); recentIndex = 0U;
  char clientId[64], will[200]; snprintf(clientId, sizeof(clientId), "%s-%06lx", config.device, static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFU)); snprintf(will, sizeof(will), "{\"device_id\":\"%s\",\"mqtt\":0,\"event\":\"offline\"}", config.device);
  if (mqtt.connect(clientId, config.username[0] ? config.username : nullptr, config.username[0] ? config.password : nullptr, topicStatus, 1, true, will) && mqtt.subscribe(topicCmd, 1)) { netOnline = true; Serial.printf("MQTT_CONNECTED session_id=%s\n", sessionId); }
  else { mqtt.disconnect(); Serial.printf("MQTT_CONNECT_FAILED state=%d\n", mqtt.state()); }
}
static void serviceConsole() {
  static char line[320] = {}; static size_t length = 0U;
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value == '\r' || value == '\n') {
      if (!length) continue;
      line[length] = 0; length = 0; String input(line); input.trim(); if (input.isEmpty()) continue;
      if (input.equalsIgnoreCase("CONFIG")) { startPortal(); continue; }
      if (input.equalsIgnoreCase("FACTORY_RESET")) { if (lease.armed) Serial.println("ERR STOP_BEFORE_RESET"); else if (clearConfig()) { restartAt = millis() + 1000U; Serial.println("NETWORK_CONFIG_CLEARED"); } else Serial.println("ERR NVS_CLEAR_FAILED"); continue; }
      if (input.equalsIgnoreCase("LOCAL")) { if (lease.armed) { Serial.println("LOCAL_RESULT rejected_stop_before_local"); continue; } if (portal) { web.stop(); WiFi.softAPdisconnect(true); portal = false; } localOnly = true; lease.lock(); linkVerified = false; Serial.println("LOCAL_RESULT accepted_local_mode"); continue; }
      if (input.equalsIgnoreCase("STATUS")) { Serial.printf("STATUS config=%u mode=%s wifi=%u mqtt=%u armed=%u link=%s state=%u task=%u fault=0x%08lx tx=%lu\n", configured ? 1U : 0U, portal ? "CONFIG" : localOnly ? "LOCAL" : "NETWORK", WiFi.status() == WL_CONNECTED ? 1U : 0U, netOnline ? 1U : 0U, lease.armed ? 1U : 0U, linkVerified ? "verified" : "unverified", lastStatus.state, lastStatus.activeTask, static_cast<unsigned long>(lastStatus.faultCode), static_cast<unsigned long>(txCount)); continue; }
      if (input.equalsIgnoreCase("HELP") || input == "?") { Serial.println("LOCAL / CONFIG / FACTORY_RESET / STATUS / PING / ARM / FEED / CLEAN / STOP / RESET_FAULT"); Serial.println("WEB {\"command\":\"PING\",\"request_id\":\"t1\",\"session_id\":\"...\"}"); continue; }
      if (input.startsWith("WEB ")) { gateway::Command command{}; const String json = input.substring(4); if (!gateway::parseCommand(reinterpret_cast<const uint8_t *>(json.c_str()), json.length(), command)) Serial.println("WEB_ACK rejected_bad_json"); else Serial.printf("WEB_ACK request_id=%s result=%s\n", command.request, execute(command, true)); continue; }
      input.toUpperCase(); gateway::Command command{}; strncpy(command.text, input.c_str(), sizeof(command.text) - 1U); const char *result = execute(command, false); if (!strncmp(result, "rejected", 8U)) ++rejectedCount; Serial.printf("LOCAL_RESULT command=%s result=%s\n", command.text, result);
      continue;
    }
    if (value < 32 || value > 126 || length >= sizeof(line) - 1U) { length = 0; Serial.println("LOCAL_ACK rejected_bad_line"); continue; }
    line[length++] = static_cast<char>(value);
  }
}

void setup() {
  Serial.begin(115200); delay(300U); pinMode(SPI_CS, OUTPUT); digitalWrite(SPI_CS, HIGH); pinMode(0, INPUT_PULLUP);
  spiReady = spi.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS); mqtt.setCallback(handleMqtt); mqtt.setBufferSize(1536); mqtt.setKeepAlive(10); mqtt.setSocketTimeout(2);
  nextId = esp_random(); configured = loadConfig(); randomToken(sessionId, sizeof(sessionId));
  Serial.println("Rabbit ESP32-S3 gateway v8.2.2 NET_SAVE_DIAG LOCAL_WIFI_PAUSE; STM32 task SPI v3");
  Serial.println("GPIO15/13/12/14 -> STM32 PB12/PB13/PB15/PB14; no ESP32 relay output");
  if (!configured) { config = gateway::Config{}; startPortal(); return; }
  buildTopics(); WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin(config.ssid, config.wifiPassword);
}
void loop() {
  if (localOnly && !localNetworkPaused) { WiFi.setAutoReconnect(false); if (WiFi.status() != WL_CONNECTED && !WiFi.disconnect(false, false)) Serial.println("NETWORK_LOCAL_CANCEL_FAILED; reconnect remains paused"); localNetworkPaused = true; Serial.println("NETWORK_LOCAL_PAUSED; existing online MQTT STOP link retained"); }
  else if (!localOnly && localNetworkPaused) { WiFi.setAutoReconnect(true); localNetworkPaused = false; }
  if (!portal && configured && !localOnly && WiFi.status() != WL_CONNECTED && millis() - lastFrameAt > 10000U) WiFi.reconnect();
  reconnectMqtt(); if (mqtt.connected()) { netOnline = true; mqtt.loop(); } else netOnline = false;
  serviceConsole();
  if (lease.expired(millis(), netOnline && WiFi.status() == WL_CONNECTED, networkEpoch)) { gateway::Command stop{}; strcpy(stop.text, "STOP"); lease.lock(); (void)execute(stop, false); publishAck("", "lease_expired"); }
  if (millis() - lastFrameAt >= SPI_INTERVAL_MS) pollSpi(false);
  if (mqtt.connected() && millis() - lastStatusAt >= 1000U) publishStatus();
  static uint32_t pressedAt = 0U; if (digitalRead(0) == LOW && !lease.armed) { if (!pressedAt) pressedAt = millis(); if (millis() - pressedAt >= 5000U) startPortal(); } else pressedAt = 0U;
  if (portal) { web.handleClient(); if (configured && millis() - portalSince >= 600000U && !restartAt) { web.stop(); WiFi.softAPdisconnect(true); portal = false; Serial.println("CONFIG_TIMEOUT"); } }
  if (restartAt && static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
  delay(2U);
}
