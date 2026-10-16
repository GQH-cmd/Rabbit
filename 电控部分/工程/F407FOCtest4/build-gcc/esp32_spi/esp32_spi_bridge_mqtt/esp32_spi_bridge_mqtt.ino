#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "FocSpiProtocol.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select ESP32S3 Dev Module. This wiring is for ESP32-S3, not classic ESP32/C3."
#endif

// ESP32-S3 N16R8 -> STM32 SPI3 (M1 connector), NOT the 2x3 BOOT header.
// GPIO15 -> PA15/CS; GPIO13 -> PC10/SCK; GPIO14 <- PC11/MISO;
// GPIO12 -> PC12/MOSI; GND <-> DGND. Leave both power rails UNCONNECTED.
// COM USB: USB CDC On Boot=Disabled. Native USB: CDC On Boot=Enabled.
static constexpr uint8_t PIN_SPI_CS = 15;
static constexpr uint8_t PIN_SPI_SCK = 13;
static constexpr uint8_t PIN_SPI_MISO = 14;
static constexpr uint8_t PIN_SPI_MOSI = 12;
static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t SPI_CLOCK_HZ = 100000; // slow first for the STM32 IT receiver
static constexpr uint32_t MIN_FRAME_GAP_MS = 100;
static constexpr uint32_t LINE_TIMEOUT_MS = 1500;
static constexpr size_t LINE_CAPACITY = 96;
static constexpr size_t SERIAL_BYTE_BUDGET = 64;

// MQTT first version: edit these five values before a real network test.
// The placeholders deliberately keep the firmware offline until configured.
static constexpr char WIFI_SSID[] = "CHANGE_ME_WIFI_SSID";
static constexpr char WIFI_PASSWORD[] = "CHANGE_ME_WIFI_PASSWORD";
static constexpr char MQTT_HOST[] = "CHANGE_ME_MQTT_HOST";
static constexpr uint16_t MQTT_PORT = 1883;
static constexpr char MQTT_USERNAME[] = "";
static constexpr char MQTT_PASSWORD[] = "";
static constexpr char DEVICE_ID[] = "rabbit-foc-001";
static constexpr uint32_t NETWORK_RETRY_MS = 10000;
static constexpr uint32_t STATUS_PUBLISH_MS = 10000;

SPIClass focSpi(FSPI);
WiFiClient mqttTransport;
PubSubClient mqttClient(mqttTransport);
static bool spiReady = false;
static bool armed = false; // local input guard ONLY; not physical motor state
static bool sentAnyFrame = false;
static uint8_t sequenceNumber = 0;
static uint32_t txCount = 0;
static uint32_t rejectedCount = 0;
static uint32_t lastFrameMs = 0;
static char lineBuffer[LINE_CAPACITY] = {};
static size_t lineLength = 0;
static bool droppingLine = false;
static uint32_t lastByteMs = 0;
static uint32_t nextNetworkAttemptMs = 0;
static uint32_t lastStatusPublishMs = 0;
static bool mqttWasConnected = false;
static char topicCommand[96] = {};
static char topicAck[96] = {};
static char topicStatus[96] = {};

static void processLine(char* text);

static bool networkConfigured() {
  return WIFI_SSID[0] != '\0' && WIFI_PASSWORD[0] != '\0' &&
         MQTT_HOST[0] != '\0' && strcmp(WIFI_SSID, "CHANGE_ME_WIFI_SSID") != 0 &&
         strcmp(MQTT_HOST, "CHANGE_ME_MQTT_HOST") != 0;
}

static void buildMqttTopics() {
  snprintf(topicCommand, sizeof(topicCommand), "rabbit/%s/cmd", DEVICE_ID);
  snprintf(topicAck, sizeof(topicAck), "rabbit/%s/ack", DEVICE_ID);
  snprintf(topicStatus, sizeof(topicStatus), "rabbit/%s/status", DEVICE_ID);
}

static bool extractJsonString(const char* json, size_t length, const char* key,
                              char* output, size_t capacity) {
  if (!json || !key || !output || capacity < 2) return false;
  char marker[40] = {};
  const int markerLength = snprintf(marker, sizeof(marker), "\"%s\"", key);
  if (markerLength <= 0 || static_cast<size_t>(markerLength) >= sizeof(marker)) return false;
  const char* end = json + length;
  const char* found = strstr(json, marker);
  if (!found || found >= end) return false;
  const char* cursor = found + markerLength;
  while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) ++cursor;
  if (cursor >= end || *cursor++ != ':') return false;
  while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) ++cursor;
  if (cursor >= end || *cursor++ != '"') return false;
  size_t copied = 0;
  while (cursor < end && *cursor != '"') {
    if (copied + 1 >= capacity) return false;
    if (*cursor == '\\') return false; // strict first version: no escaped strings
    output[copied++] = *cursor++;
  }
  if (cursor >= end) return false;
  output[copied] = '\0';
  return copied > 0;
}

static void publishMqttStatus(const char* eventName) {
  if (!mqttClient.connected()) return;
  char payload[320] = {};
  snprintf(payload, sizeof(payload),
           "{\"device_id\":\"%s\",\"event\":\"%s\",\"wifi\":%u,"
           "\"mqtt\":%u,\"spi_ready\":%u,\"armed\":%u,\"tx\":%lu,"
           "\"rejected\":%lu,\"foc_link\":\"unverified\"}",
           DEVICE_ID, eventName, static_cast<unsigned>(WiFi.status() == WL_CONNECTED),
           static_cast<unsigned>(mqttClient.connected()), static_cast<unsigned>(spiReady),
           static_cast<unsigned>(armed), static_cast<unsigned long>(txCount),
           static_cast<unsigned long>(rejectedCount));
  mqttClient.publish(topicStatus, payload, true);
}

static void publishMqttAck(const char* requestId, const char* result) {
  if (!mqttClient.connected()) return;
  char safeRequest[48] = {};
  if (requestId) {
    size_t n = 0;
    while (requestId[n] && n + 1 < sizeof(safeRequest)) {
      const char c = requestId[n];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_')) break;
      safeRequest[n] = c;
      ++n;
    }
    safeRequest[n] = '\0';
  }
  char payload[240] = {};
  snprintf(payload, sizeof(payload),
           "{\"device_id\":\"%s\",\"request_id\":\"%s\","
           "\"result\":\"%s\",\"armed\":%u,\"tx\":%lu,"
           "\"foc_link\":\"unverified\"}",
           DEVICE_ID, safeRequest, result, static_cast<unsigned>(armed),
           static_cast<unsigned long>(txCount));
  mqttClient.publish(topicAck, payload, false);
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!topic || strcmp(topic, topicCommand) != 0 || length == 0 || length >= 160) return;
  char message[160] = {};
  memcpy(message, payload, length);
  message[length] = '\0';

  char requestId[48] = {};
  char command[96] = {};
  bool parsedJson = message[0] == '{';
  if (parsedJson) {
    if (!extractJsonString(message, length, "command", command, sizeof(command))) {
      publishMqttAck("", "rejected_bad_json");
      return;
    }
    (void)extractJsonString(message, length, "request_id", requestId, sizeof(requestId));
  } else {
    strncpy(command, message, sizeof(command) - 1);
  }

  const uint32_t txBefore = txCount;
  const uint32_t rejectedBefore = rejectedCount;
  processLine(command);
  const char* result = txCount != txBefore ? "spi_sent" :
                       (rejectedCount != rejectedBefore ? "rejected" : "accepted_local");
  publishMqttAck(requestId, result);
}

static void networkBegin() {
  buildMqttTopics();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(30);
  if (!networkConfigured()) {
    Serial.println("MQTT_CONFIG=NOT_SET edit WIFI_SSID/WIFI_PASSWORD/MQTT_HOST");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WIFI_CONNECTING ssid=%s MQTT=%s:%u\n", WIFI_SSID, MQTT_HOST,
                static_cast<unsigned>(MQTT_PORT));
}

static void networkTick() {
  if (!networkConfigured()) return;
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    mqttWasConnected = false;
    if (static_cast<uint32_t>(now - nextNetworkAttemptMs) >= NETWORK_RETRY_MS) {
      nextNetworkAttemptMs = now;
      WiFi.reconnect();
      Serial.println("WIFI_RECONNECT");
    }
    return;
  }
  if (!mqttClient.connected()) {
    mqttWasConnected = false;
    if (static_cast<uint32_t>(now - nextNetworkAttemptMs) < NETWORK_RETRY_MS) return;
    nextNetworkAttemptMs = now;
    char clientId[64] = {};
    snprintf(clientId, sizeof(clientId), "%s-%08lx", DEVICE_ID,
             static_cast<unsigned long>(ESP.getEfuseMac() & 0xFFFFFFFFULL));
    const bool connected = (MQTT_USERNAME[0] == '\0')
      ? mqttClient.connect(clientId)
      : mqttClient.connect(clientId, MQTT_USERNAME, MQTT_PASSWORD);
    if (connected) {
      mqttClient.subscribe(topicCommand);
      mqttWasConnected = true;
      lastStatusPublishMs = now;
      Serial.printf("MQTT_CONNECTED cmd=%s\n", topicCommand);
      publishMqttStatus("connected");
    } else {
      Serial.printf("MQTT_CONNECT_FAILED state=%d\n", mqttClient.state());
    }
    return;
  }
  mqttClient.loop();
  if (!mqttWasConnected) {
    mqttWasConnected = true;
    publishMqttStatus("reconnected");
  }
  if (static_cast<uint32_t>(now - lastStatusPublishMs) >= STATUS_PUBLISH_MS) {
    lastStatusPublishMs = now;
    publishMqttStatus("heartbeat");
  }
}

static void reject(const char* reason) {
  ++rejectedCount;
  Serial.printf("ERR %s\n", reason);
}
static bool sameWord(const char* text, const char* upper) {
  while (*text && *upper) {
    char c = *text++;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c != *upper++) return false;
  }
  return *text == '\0' && *upper == '\0';
}
static void printHelp() {
  Serial.println("ESP32-S3 FOC SPI bridge v2 (2026-09-19)");
  Serial.println("HELP / STATUS : local only; PING : harmless link test");
  Serial.println("ARM : allow motor commands; STOP / DISARM : send o0,o0 and lock");
  Serial.println("Motor format: <mode><number>,<mode><number>, e.g. v30,o0");
  Serial.println("Modes: o=open-loop, c=current, v=speed RPM, p=position. Both modes REQUIRED.");
  Serial.println("Max payload 28 ASCII bytes. End each line with CR, LF or CRLF.");
  Serial.println("TX != received/executed. ACK=UNAVAILABLE on legacy FOC SPI firmware.");
  Serial.println("Check FOC USB1/XCOM 115200 for SPI3 PONG or SPI3 OK with matching seq.");
  Serial.println("STOP is a transmitted request, NOT a hardware emergency stop.");
}
static void printStatus() {
  Serial.printf("LOCAL spi_ready=%u armed=%u tx=%lu rejected=%lu cpu_mhz=%lu heap=%lu\n",
                static_cast<unsigned>(spiReady), static_cast<unsigned>(armed),
                static_cast<unsigned long>(txCount), static_cast<unsigned long>(rejectedCount),
                static_cast<unsigned long>(getCpuFrequencyMhz()),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.printf("SPI3 CS=%u SCK=%u MISO=%u MOSI=%u mode=0 hz=%lu frame=32\n",
                PIN_SPI_CS, PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI,
                static_cast<unsigned long>(SPI_CLOCK_HZ));
  Serial.println("FOC_LINK=NOT_VERIFIED ACK=UNAVAILABLE (use FOC USB1 log)");
}
static bool transmit(const char* command) {
  if (!spiReady) { reject("SPI_NOT_READY"); return false; }
  uint8_t tx[foc::kFrameSize];
  uint8_t rx[foc::kFrameSize] = {};
  if (!foc::buildFrame(command, sequenceNumber, tx)) {
    reject("PAYLOAD_LENGTH_1_TO_28"); return false;
  }
  // STM32 has one RX buffer/ready flag. Pacing reduces risk, but is NOT an ACK.
  if (sentAnyFrame) {
    const uint32_t elapsed = static_cast<uint32_t>(millis() - lastFrameMs);
    if (elapsed < MIN_FRAME_GAP_MS) delay(MIN_FRAME_GAP_MS - elapsed);
  }
  focSpi.beginTransaction(SPISettings(SPI_CLOCK_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_SPI_CS, LOW);
  delayMicroseconds(10);
  focSpi.transferBytes(tx, rx, foc::kFrameSize);
  delayMicroseconds(10);
  digitalWrite(PIN_SPI_CS, HIGH);
  focSpi.endTransaction();
  lastFrameMs = millis();
  sentAnyFrame = true;
  ++sequenceNumber;
  ++txCount;
  // rx is intentionally NOT interpreted: existing FOC has no SPI TX protocol.
  Serial.printf("SPI TX seq=%u len=%u cmd=%s ACK=UNAVAILABLE\n",
                static_cast<unsigned>(tx[1]), static_cast<unsigned>(tx[2]), command);
  return true;
}
static void processLine(char* text) {
  while (*text == ' ' || *text == '\t') ++text;
  size_t n = strlen(text);
  while (n && (text[n-1] == ' ' || text[n-1] == '\t')) text[--n] = '\0';
  if (!n) return;
  if (sameWord(text, "HELP") || sameWord(text, "?")) { printHelp(); return; }
  if (sameWord(text, "STATUS")) { printStatus(); return; }
  if (sameWord(text, "PING")) { transmit("PING"); return; }
  if (sameWord(text, "ARM")) {
    if (!spiReady) { reject("SPI_NOT_READY"); return; }
    armed = true;
    Serial.println("LOCAL ARMED (not a FOC command; no motor started)");
    return;
  }
  if (sameWord(text, "STOP") || sameWord(text, "DISARM")) {
    armed = false;
    transmit("o0,o0");
    Serial.println("LOCAL LOCKED; confirm actual stop on FOC; no automatic retries");
    return;
  }
  bool zeroOpenLoop = false;
  if (!foc::validMotorCommand(text, zeroOpenLoop)) {
    reject("BAD_COMMAND use both modes, e.g. v30,o0; reject NaN/Inf/junk/len>28");
    return;
  }
  if (zeroOpenLoop) armed = false;
  else if (!armed) { reject("NOT_ARMED send ARM first"); return; }
  transmit(text);
}
static void pollConsole() {
  // Never execute partial input on timeout: discard through the NEXT newline.
  if (lineLength && static_cast<uint32_t>(millis() - lastByteMs) >= LINE_TIMEOUT_MS) {
    lineLength = 0; droppingLine = true;
    reject("LINE_TIMEOUT discarded; send a newline then retry the whole command");
  }
  size_t budget = SERIAL_BYTE_BUDGET;
  while (budget-- && Serial.available()) {
    const int value = Serial.read();
    if (value < 0) break;
    const char c = static_cast<char>(value);
    lastByteMs = millis();
    if (c == '\r' || c == '\n') {
      if (!droppingLine && lineLength) {
        lineBuffer[lineLength] = '\0';
        processLine(lineBuffer);
      }
      lineLength = 0; droppingLine = false;
      continue;
    }
    if (droppingLine) continue;
    if ((value < 32 && c != '\t') || value > 126 || lineLength >= LINE_CAPACITY - 1) {
      reject("BAD_OR_LONG_LINE discarded through newline");
      lineLength = 0; droppingLine = true;
      continue;
    }
    lineBuffer[lineLength++] = c;
  }
}
void setup() {
  // No Wi-Fi/Bluetooth, motor start, auto-PING, or replay of saved commands.
  armed = false; spiReady = false; sentAnyFrame = false;
  sequenceNumber = 0; txCount = 0; rejectedCount = 0; lastFrameMs = 0;
  lineLength = 0; droppingLine = false; lastByteMs = 0;
  Serial.begin(SERIAL_BAUD);
  delay(300); // no infinite wait for a USB serial monitor
  if (!setCpuFrequencyMhz(80)) Serial.println("WARN CPU clock unchanged");
  pinMode(PIN_SPI_CS, OUTPUT);
  digitalWrite(PIN_SPI_CS, HIGH);
  spiReady = focSpi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
  if (!spiReady) reject("SPI_INIT_FAILED");
  printHelp();
  printStatus();
  networkBegin();
  Serial.println("READY LOCAL LOCKED. Send PING first; boot sent NO SPI frames.");
}
void loop() {
  pollConsole();
  networkTick();
  delay(2); // yield; no busy-spin and no periodic motor commands
}
