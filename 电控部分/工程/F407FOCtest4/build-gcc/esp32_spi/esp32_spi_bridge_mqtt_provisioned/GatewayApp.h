#pragma once
#include <atomic>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "GatewayValidation.h"
#include "FocLinkClient.h"
#include "RelayControl.h"
#include "FeedStageConfig.h"

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select ESP32S3 Dev Module"
#endif

namespace app {
// 接线不变：GPIO15→PA15/CS，13→PC10/SCK，14←PC11/MISO，12→PC12/MOSI，共地不接电源线。
constexpr uint8_t CS=15,SCK=13,MISO=14,MOSI=12;
SPIClass spi(FSPI);
WebServer web(80);
gateway::Config config{};
gateway::Lease lease{};
bool configured=false,spiReady=false,sentFrame=false;
uint32_t txCount=0,rejectedCount=0,lastFrame=0;
uint32_t nextId=0;
foclink::Client link;
bool polling=false,linkVerified=false,linkReset=false;
relay::Controller relays;
diagnostic::ProbeBatch probes;
feed::Controller feedStage;
struct FeedSource {char request[48]={};uint32_t epoch=0;bool remote=false;} feedSource;
feed::State reportedFeedState=feed::State::Idle;
bool relayPinsReady=false;
int appliedRelay=-1;
struct Pending {char request[48];uint32_t epoch;bool remote;} pending{};
std::atomic<bool> netOnline{false},portal{false},networkTaskReady{false},localOnly{false};
std::atomic<uint32_t> netEpoch{0};
QueueHandle_t commands=nullptr,stops=nullptr,acks=nullptr,snapshots=nullptr;
struct QueuedCommand { gateway::Command value; uint32_t epoch; };
struct Ack { char request[48]; char result[40]; uint32_t epoch,tx; bool armed; };
struct Snapshot { uint32_t tx,rejected,age,capturedAt; bool armed,spi,online,verified; FlpStatus foc; bool relayConfigured; int relayActive; feed::State feedState; bool feedConfigured; };
char topicCmd[96]={},topicAck[96]={},topicStatus[96]={};
char csrf[33]={},apPassword[20]={};
uint32_t portalSince=0,restartAt=0;

static void randomToken(char* out,size_t capacity) {
  snprintf(out,capacity,"%08lx%08lx",static_cast<unsigned long>(esp_random()),static_cast<unsigned long>(esp_random()));
}
static bool loadConfig() {
  Preferences p;
  if(!p.begin("rabbitnet",true)) return false;
  bool ok=p.getBytesLength("config")==sizeof(config) && p.getBytes("config",&config,sizeof(config))==sizeof(config);
  p.end();
  return ok && gateway::validConfig(config);
}
static bool persistConfig(const gateway::Config& next) {
  Preferences p;
  if(!p.begin("rabbitnet",false)) return false;
  bool ok=p.putBytes("config",&next,sizeof(next))==sizeof(next);
  gateway::Config verify{};
  ok=ok && p.getBytes("config",&verify,sizeof(verify))==sizeof(verify) && memcmp(&next,&verify,sizeof(next))==0;
  p.end(); return ok;
}
static bool clearConfig() {
  Preferences p;
  if(!p.begin("rabbitnet",false)) return false;
  bool ok=p.clear(); p.end(); return ok;
}
// 继电器极性单独保存，不影响已有 Wi-Fi/MQTT 配置；FACTORY_RESET 仍只清网络。
static bool persistRelaySettings(const relay::Settings& next) {
  if(!relay::valid(next))return false;
  Preferences p;
  if(!p.begin("rabbitrelay",false))return false;
  bool ok=p.putBytes("config",&next,sizeof(next))==sizeof(next);
  relay::Settings verify{};
  ok=ok&&p.getBytes("config",&verify,sizeof(verify))==sizeof(verify)&&memcmp(&next,&verify,sizeof(next))==0;
  p.end();return ok;
}
static void loadRelaySettings() {
  Preferences p;relay::Settings saved{};
  if(!p.begin("rabbitrelay",true))return;
  bool ok=p.getBytesLength("config")==sizeof(saved)&&p.getBytes("config",&saved,sizeof(saved))==sizeof(saved);
  p.end();if(ok)relays.configure(saved);
}
// 先写关闭电平，再启用输出；切换通道先全关。复位早期仍需模块的硬件偏置。
static void applyRelayOutputs() {
  if(!relays.configured)return;
  if(relayPinsReady&&appliedRelay==relays.active)return;
  digitalWrite(relay::MILK_PIN,1-relays.settings.milkActive);
  digitalWrite(relay::WATER_PIN,1-relays.settings.waterActive);
  if(!relayPinsReady) {
    pinMode(relay::MILK_PIN,OUTPUT);pinMode(relay::WATER_PIN,OUTPUT);relayPinsReady=true;
  }
  if(relays.active==0)digitalWrite(relay::MILK_PIN,relays.settings.milkActive);
  if(relays.active==1)digitalWrite(relay::WATER_PIN,relays.settings.waterActive);
  appliedRelay=relays.active;
}
static void stopRelays(const char* reason) {
  bool wasOn=relays.running();relays.stop();applyRelayOutputs();
  if(wasOn)Serial.printf("RELAY_OFF reason=%s OUTPUT_ONLY\n",reason);
}
static void serviceRelays() {
  const char* reason=relays.tick(millis(),linkVerified&&link.online(millis())&&!linkReset);
  if(reason){applyRelayOutputs();Serial.printf("RELAY_OFF reason=%s OUTPUT_ONLY\n",reason);}
}
static void reportRelays() {
  Serial.printf("RELAY configured=%u milk_gpio=4 water_gpio=5 milk_active=%d water_active=%d milk_output=%u water_output=%u max_test_ms=%lu OUTPUT_ONLY\n",
    relays.configured,relays.configured?relays.settings.milkActive:-1,relays.configured?relays.settings.waterActive:-1,
    relays.active==0,relays.active==1,static_cast<unsigned long>(relay::MAX_TEST_MS));
  if(!relays.configured)Serial.println("RELAY_UNCONFIGURED GPIO4/5 INPUT; disconnect pump power, then RELAY CONFIG LOW or HIGH; hardware default-off NOT VERIFIED");
}
// 主循环独占链路状态。回执必须等 FOC 返回相同编号；发完 SPI 不算成功。
static void finishPending(const char* result) {
  if(probes.result(link.commandId,!strcmp(result,"foc_pong"))&&strcmp(result,"foc_pong")) {
    linkVerified=false;stopRelays("spi_test_failed");
  }
  Serial.printf("FOC_RESULT id=%lu result=%s\n",static_cast<unsigned long>(link.commandId),result);
  if(pending.remote) {
    Ack a{};strcpy(a.request,pending.request);strncpy(a.result,result,sizeof(a.result)-1);
    a.epoch=pending.epoch;a.tx=txCount;a.armed=lease.armed;
    if(xQueueSend(acks,&a,0)!=pdTRUE)Serial.println("WARN ACK_QUEUE_FULL");
  }
  pending=Pending{};
}
static void exchangeFrame(const uint8_t* tx) {
  uint8_t rx[FLP_SIZE]={};
  uint32_t elapsed=millis()-lastFrame;
  if(sentFrame && elapsed<100)delay(100-elapsed);
  spi.beginTransaction(SPISettings(100000,MSBFIRST,SPI_MODE0));
  digitalWrite(CS,LOW);delayMicroseconds(10);
  spi.transferBytes(tx,rx,FLP_SIZE);
  delayMicroseconds(10);digitalWrite(CS,HIGH);spi.endTransaction();
  lastFrame=millis();sentFrame=true;++txCount;
  if(link.observe(rx,millis())&&link.restarted) {linkReset=true;linkVerified=false;}
  int result=link.takeResult();
  if(result>=0) {
    // PING 的匹配回执才建立双向验证；遥测 CRC 合法不等于新命令已执行。
    if(result==FLP_PONG)linkVerified=true;
    finishPending(result==FLP_PONG?"foc_pong":result==FLP_ACCEPTED?"foc_accepted":"foc_rejected");
  }
}
static bool transmit(const char* command,bool remote=false,uint32_t epoch=0,const char* request="") {
  if(!spiReady||link.pending)return false;
  uint8_t tx[FLP_SIZE];
  if(++nextId==0)++nextId;
  if(!flp_request(tx,FLP_COMMAND,nextId,command,0))return false;
  // 先等待帧间隔再启动回执计时，避免把等待时间误算为从机超时。
  if(sentFrame&&millis()-lastFrame<100)delay(100-(millis()-lastFrame));
  if(!link.begin(nextId,millis()))return false;
  pending=Pending{};pending.remote=remote;pending.epoch=epoch;
  strncpy(pending.request,request,sizeof(pending.request)-1);
  polling=true;exchangeFrame(tx);
  Serial.printf("SPI TX id=%lu cmd=%s WAIT_FOC_ACK\n",static_cast<unsigned long>(nextId),command);
  return true;
}
static void stopAndLock(const char* reason,bool remote=false,uint32_t epoch=0,const char* request="") {
  if(feedStage.running())feedStage.cancel(reason);
  // 先本地关两路输出，不等待 SPI 或网络；同时取消连续探测。
  stopRelays(reason);
  if(probes.active)Serial.printf("SPITEST_CANCEL reason=%s\n",reason);
  probes.cancel();
  lease.lock();
  if(link.pending){link.cancel();finishPending("cancelled_by_stop");}
  bool sent=transmit("o0,o0",remote,epoch,request);
  Serial.printf("STOP_REQUEST reason=%s sent=%u\n",reason,sent);
}
static void reportFeed() {
  auto cfg=feedconfig::settings();
  cfg.singleM1=FOC_M1_ONLY;
  Serial.printf("FEED phase=%s busy=%u configured=%u milk_ms=%lu down_rpm_sign=%d reason=%s %s axis=%s\n",
    feed::name(feedStage.state),feedStage.running(),feed::Controller::valid(cfg),
    static_cast<unsigned long>(cfg.milkMs),cfg.downSign,feedStage.reason,
    cfg.autoCycle?"AUTO_FEED_RETURN":"CONTACT_STAGE_ONLY",FOC_M1_ONLY?"M1_ONLY":"DUAL");
}
static void feedNotice() {
  if(feedStage.state==reportedFeedState)return;
  reportedFeedState=feedStage.state;reportFeed();
  if(feedSource.remote) {
    Ack a{};strcpy(a.request,feedSource.request);
    snprintf(a.result,sizeof(a.result),"feed_%s",feed::name(feedStage.state));
    a.epoch=feedSource.epoch;a.tx=txCount;a.armed=lease.armed;
    if(xQueueSend(acks,&a,0)!=pdTRUE)Serial.println("WARN FEED_ACK_QUEUE_FULL");
    if(!feedStage.running())feedSource=FeedSource{};
  }
}
// 仅在明确接受FEED或正常喂奶计时结束后调用；先绑定编号，再发一次，永不重放。
static bool sendFeedMotion() {
  const char* command=feedStage.motionCommand();
  uint32_t id=nextId+1;if(!id)id=1;
  if(!command||link.pending||!feedStage.bindMotion(id,millis()))return false;
  return transmit(command);
}
static void serviceFeed() {
  if(!feedStage.running()){feedNotice();return;}
  const uint32_t checkedAt=millis();
  const uint32_t age=link.seen?checkedAt-link.receivedAt:UINT32_MAX;
  // STOP交换可能立即覆盖link.status；保存实际参与此次保护判断的原始帧。
  const FlpStatus checked=link.status;
  const feed::State phaseBefore=feedStage.state;
  feed::Action action;
  if(!linkVerified||linkReset||!lease.armed){feedStage.cancel("foc_unverified_or_locked");action=feed::Action::Abort;}
  else action=feedStage.tick(checked,checkedAt,age,relays.active==0);
  if(action==feed::Action::SendStop) {
    // 此编号只在ESP32与FOC之间匹配停止回执，不需要网页传任务编号。
    if(link.pending){link.cancel();finishPending("cancelled_by_feed_stop");}
    uint32_t id=nextId+1;if(!id)id=1;
    if(!feedStage.bindStop(id,millis())||!transmit("o0,o0")) {
      feedStage.cancel("stop_send_failed");stopAndLock("feed_stop_failed");
    }
  } else if(action==feed::Action::StartReturn) {
    // 反转前先硬写GPIO关闭奶泵；不能等SPI的帧间隔或回执才关泵。
    stopRelays("feed_milk_complete");
    if(!sendFeedMotion())stopAndLock("return_motion_send_failed");
  } else if(action==feed::Action::FinishCycle) {
    stopRelays("feed_cycle_complete");lease.lock();
    Serial.println("FEED_CYCLE_COMPLETE; M1 returned to a new contact and stopped; no X movement");
  } else if(action==feed::Action::StartMilk) {
    // 两个控制器共享起点。分别读取millis可能跨过毫秒边界，使继电器先到期
    // 而阶段尚差1ms，从而把正常定时关泵误判为milk_output_lost。
    const uint32_t pumpNow=millis();
    const char* result=relays.startMilk(feedStage.settings.milkMs,feedStage.settings.maxMilkMs,pumpNow);
    if(strncmp(result,"accepted",8)||!feedStage.pumpStarted(pumpNow)) {
      feedStage.cancel("milk_start_rejected");stopAndLock("feed_relay_failed");
    } else {
      applyRelayOutputs();Serial.printf("FEED_MILK_ON duration_ms=%lu OUTPUT_ONLY\n",static_cast<unsigned long>(feedStage.settings.milkMs));
    }
  } else if(action==feed::Action::StopMilk) {
    stopRelays("feed_timer_complete");lease.lock();
    Serial.println("FEED_CONTACT_STAGE_COMPLETE; no X movement or automatic lift");
  } else if(action==feed::Action::Abort) {
    const char* reason=feedStage.reason;
    // 安全动作先行：先关泵并请求双轴STOP，再打印保存的帧，不延后停机。
    stopAndLock(reason);
    Serial.printf("FEED_ABORT reason=%s phase_before=%s checked_ms=%lu sample=%lu uptime_ms=%lu age_ms=%lu flags=0x%02X encoder0_enabled=%u encoder1_enabled=%u mode0=%u rpm0_x100=%ld mode1=%u rpm1_x100=%ld contact1=%u link_faults=%u bad_frames=%lu\n",
      reason,feed::name(phaseBefore),static_cast<unsigned long>(checkedAt),
      static_cast<unsigned long>(checked.sample),static_cast<unsigned long>(checked.uptime),
      static_cast<unsigned long>(age),static_cast<unsigned int>(checked.flags),
      static_cast<unsigned int>(!!(checked.flags&FLP_ENCODER0)),static_cast<unsigned int>(!!(checked.flags&FLP_ENCODER1)),
      static_cast<unsigned int>(checked.mode0),static_cast<long>(checked.rpm0),
      static_cast<unsigned int>(checked.mode1),static_cast<long>(checked.rpm1),
      static_cast<unsigned int>(!!(checked.flags&FLP_CONTACT1)),static_cast<unsigned int>(checked.faults),
      static_cast<unsigned long>(checked.badFrames));
  }
  feedNotice();
}
static void serviceLink() {
  if(link.timeout(millis())) {
    finishPending("foc_timeout");linkVerified=false;
    stopRelays("ack_timeout");
    bool wasArmed=lease.armed;lease.lock();xQueueReset(commands);
    // 不重放原运动命令；若正在运动，只尝试一次停止。FOC 自身另有两秒看门狗。
    if(wasArmed)stopAndLock("ack_timeout");
  }
  if(linkReset || (lease.armed&&(!linkVerified||!link.online(millis())))) {
    stopRelays("foc_link_lost");
    linkReset=false;linkVerified=false;stopAndLock("foc_link_lost");xQueueReset(commands);
  }
  if(polling&&spiReady&&millis()-lastFrame>=100&&!(probes.due()&&!link.pending)) {
    uint8_t tx[FLP_SIZE];flp_request(tx,FLP_POLL,0,"",lease.armed?1:0);exchangeFrame(tx);
  }
}
static void serviceDiagnostics() {
  if(probes.completed) {
    Serial.printf("SPITEST_DONE total=%lu pong=%lu failed=%lu result=%s\n",
      static_cast<unsigned long>(probes.target),static_cast<unsigned long>(probes.passed),
      static_cast<unsigned long>(probes.failed),probes.failed?"FAIL":"PASS");
    probes.completed=false;
  }
  if(probes.due()&&!link.pending&&spiReady&&(!sentFrame||millis()-lastFrame>=100)) {
    // begin() 保持旧编号，sent() 必须在交换帧前记录，防止同帧回执竞态。
    uint32_t id=nextId+1;if(!id)id=1;
    probes.sent(id);
    if(!transmit("PING")) {probes.cancel();Serial.println("SPITEST_ABORT transmit_failed");stopRelays("spi_test_failed");}
  }
}
static void reportStatus() {
  Serial.printf("LOCAL spi_ready=%u armed=%u tx=%lu rejected=%lu wifi=%u mqtt=%u config=%u heap=%lu mode=%s\n",
    spiReady,lease.armed,static_cast<unsigned long>(txCount),static_cast<unsigned long>(rejectedCount),
    WiFi.status()==WL_CONNECTED,netOnline.load(),configured,static_cast<unsigned long>(ESP.getFreeHeap()),
    portal.load()?"CONFIG":localOnly.load()?"LOCAL":"NETWORK");
  Serial.printf("SPI3 CS=15 SCK=13 MISO=14 MOSI=12 mode=0 hz=100000 frame=64 version=2 FOC_LINK=%s pending=%u\n",
    link.online(millis())&&linkVerified?"VERIFIED":"NOT_VERIFIED",link.pending);
  if(link.seen)Serial.printf("FOC sample=%lu age_ms=%lu rpm0=%.2f rpm1=%.2f iq0=%.3f iq1=%.3f contact0=%u contact1=%u link_faults=%u\n",
    static_cast<unsigned long>(link.status.sample),static_cast<unsigned long>(millis()-link.receivedAt),
    link.status.rpm0/100.0,link.status.rpm1/100.0,link.status.iq0/1000.0,link.status.iq1/1000.0,
    !!(link.status.flags&FLP_CONTACT0),!!(link.status.flags&FLP_CONTACT1),link.status.faults);
  reportRelays();
  reportFeed();
  Serial.printf("SPITEST running=%u total=%lu pong=%lu failed=%lu\n",probes.active,
    static_cast<unsigned long>(probes.target),static_cast<unsigned long>(probes.passed),static_cast<unsigned long>(probes.failed));
}
static bool fromAp() { return portal.load() && web.client().localIP()==WiFi.softAPIP(); }
static bool protectPost() {
  if(!fromAp() || web.arg("token")!=csrf) { web.send(403,"text/plain; charset=utf-8","请连接配置热点并重新打开页面"); return false; }
  if(lease.armed||relays.running()||probes.active||feedStage.running()) { web.send(409,"text/plain; charset=utf-8","请先停止并锁定设备"); return false; }
  return true;
}
static String escapeHtml(const char* text) {
  String s=text; s.replace("&","&amp;");s.replace("\"","&quot;");s.replace("<","&lt;");s.replace(">","&gt;");return s;
}
static void page() {
  if(!fromAp()){web.send(403,"text/plain","AP only");return;}
  web.sendHeader("Cache-Control","no-store");
  String html=R"HTML(<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Rabbit 网络配置</title><style>body{font-family:system-ui;max-width:540px;margin:20px auto;padding:16px}label{display:block;margin:12px 0}input:not([type=checkbox]){width:100%;box-sizing:border-box;padding:8px}button{padding:10px}</style>
<h2>ESP32 WiFi / MQTT 配置</h2><p>仅配置网络，不控制电机。使用 2.4GHz WiFi。当前版本仅支持局域网明文 MQTT，不支持 TLS。</p>
<form method="post" action="/save"><input type="hidden" name="token" value=")HTML";
  html+=csrf;html+="\"><label>WiFi 名称<input name=ssid maxlength=32 required value=\"";
  html+=configured?escapeHtml(config.ssid):"";html+="\"></label><label>WiFi 密码（留空保留原密码）<input name=wpass type=password maxlength=63></label><label><input type=checkbox name=open value=1>此 WiFi 无密码</label>";
  html+="<label>MQTT 域名或 IPv4（不要填 http://）<input name=host maxlength=127 required value=\"";
  html+=configured?escapeHtml(config.host):"";html+="\"></label><label>端口<input name=port type=number min=1 max=65535 required value=\"";
  html+=String(configured?config.port:1883);html+="\"></label><label>MQTT 用户名<input name=user maxlength=64 value=\"";
  html+=configured?escapeHtml(config.username):"";html+="\"></label><label>MQTT 密码（留空保留原密码）<input name=pass type=password maxlength=128></label><label><input type=checkbox name=clearpass value=1>清空 MQTT 密码</label>";
  html+="<label>设备 ID<input name=device maxlength=39 required value=\"";html+=configured?escapeHtml(config.device):"rabbit-foc-001";
  html+="\"></label><button>保存并重启</button></form><hr><form method=post action=/clear><input type=hidden name=token value=\"";
  html+=csrf;html+="\"><label><input type=checkbox name=confirm value=YES required>确认清除网络配置</label><button>清除并重启</button></form></html>";
  web.send(200,"text/html; charset=utf-8",html);
}
template<size_t N> static bool copyField(const String& s,char (&dst)[N]) {
  if(s.length()>=N) return false;
  for(size_t i=0;i<s.length();++i) if(s[i]=='\0'||static_cast<uint8_t>(s[i])<32) return false;
  memcpy(dst,s.c_str(),s.length()+1);return true;
}
static void savePage() {
  if(!protectPost())return;
  gateway::Config next{};
  String host=web.arg("host"),device=web.arg("device");host.trim();device.trim();
  String wp=web.arg("wpass"),mp=web.arg("pass");
  if(web.arg("open")=="1")wp="";else if(wp.isEmpty()&&configured)wp=config.wifiPassword;
  if(web.arg("clearpass")=="1")mp="";else if(mp.isEmpty()&&configured)mp=config.password;
  bool ok=copyField(web.arg("ssid"),next.ssid)&&copyField(wp,next.wifiPassword)&&copyField(host,next.host)&&
    copyField(web.arg("user"),next.username)&&copyField(mp,next.password)&&copyField(device,next.device)&&gateway::parsePort(web.arg("port").c_str(),next.port);
  gateway::seal(next);
  if(!ok||!gateway::validConfig(next)){web.send(400,"text/plain; charset=utf-8","参数无效。WiFi 密码须为 8–63 字符或选择无密码，端口须为整数，设备 ID 只允许字母数字下划线和短横线。");return;}
  if(!persistConfig(next)){web.send(500,"text/plain; charset=utf-8","保存失败，未安排重启，请重试并检查串口。");return;}
  web.send(200,"text/plain; charset=utf-8","保存成功，即将重启。连接失败可在串口发送 CONFIG，或运行后长按 BOOT 五秒重新配置。");restartAt=millis()+1000;
}
static void clearPage() {
  if(!protectPost())return;
  if(web.arg("confirm")!="YES"){web.send(400,"text/plain","Confirmation required");return;}
  if(!clearConfig()){web.send(500,"text/plain","NVS clear failed");return;}
  web.send(200,"text/plain; charset=utf-8","配置已清除，即将重启");restartAt=millis()+1000;
}
static void startPortal() {
  if(lease.armed||relays.running()||feedStage.running()){Serial.println("ERR STOP_BEFORE_CONFIG");return;}
  stopRelays("network_config");probes.cancel();
  if(portal.load())return;
  // 配置期间禁止 ARM 和运动命令；网页仅接受热点接口上的请求。
  portal=true;netOnline=false;xQueueReset(commands);
  randomToken(csrf,sizeof(csrf));
  snprintf(apPassword,sizeof(apPassword),"Rbt-%08lx",static_cast<unsigned long>(esp_random()));
  char name[40];snprintf(name,sizeof(name),"Rabbit-Setup-%06lx",static_cast<unsigned long>(ESP.getEfuseMac()&0xFFFFFF));
  WiFi.mode(WIFI_AP_STA);
  if(!WiFi.softAP(name,apPassword)){portal=false;Serial.println("ERR AP_START_FAILED");return;}
  localOnly=false;
  web.on("/",HTTP_GET,page);web.on("/save",HTTP_POST,savePage);web.on("/clear",HTTP_POST,clearPage);
  web.onNotFound([](){web.send(404,"text/plain","Not found");});web.begin();portalSince=millis();
  Serial.printf("PROVISION_AP ssid=%s password=%s url=http://%s/\n",name,apPassword,WiFi.softAPIP().toString().c_str());
}
static const char* execute(const char* cmd,bool remote=false,uint32_t epoch=0,const char* request="") {
  if(gateway::isStop(cmd)){stopAndLock("command",remote,epoch,request);return spiReady?"spi_pending":"rejected_spi";}
  // 显式、仅本机的离线入口：不改NVS，不自动运动/供奶；重启后恢复原启动流程。
  if(!strcmp(cmd,"LOCAL")) {
    if(remote)return "rejected_local_only";
    if(restartAt)return "rejected_restart_pending";
    if(lease.armed||relays.running()||feedStage.running()||probes.active||link.pending)
      return "rejected_stop_before_local";
    if(portal.load()) {
      web.stop();
      if(!WiFi.softAPdisconnect(true)){web.begin();return "rejected_ap_stop_failed";}
    }
    stopRelays("local_mode");lease.lock();xQueueReset(commands);
    localOnly=true;portal=false;
    // 不沿用切换前的链路验证；必须重新PING，之后才能FEED/ARM。
    linkVerified=false;polling=false;
    return "accepted_local_mode";
  }
  // 已保存网络仍可保留连接，但不能接管本地调试；STOP始终优先。
  if(remote&&localOnly.load())return "rejected_local_mode";
  if(!strcmp(cmd,"FEED")) {
    if(feedStage.running())return "rejected_feed_busy";
    if(portal.load())return "rejected_config_mode";
    if(remote&&(!netOnline.load()||epoch!=netEpoch.load()))return "rejected_stale_session";
    if(relays.running()||probes.active||link.pending)return "rejected_diagnostic_busy";
    if(!spiReady||!linkVerified||!link.online(millis())||linkReset)return "rejected_foc_unverified";
    if(lease.armed&&lease.remote!=remote)return "rejected_owner";
    auto cfg=feedconfig::settings();cfg.singleM1=FOC_M1_ONLY;
    const char* result=feedStage.begin(cfg,link.status,millis(),millis()-link.receivedAt);
    if(!strncmp(result,"accepted",8)) {
      lease.arm(remote,epoch,millis());feedSource=FeedSource{};
      feedSource.remote=remote;feedSource.epoch=epoch;
      strncpy(feedSource.request,request,sizeof(feedSource.request)-1);
      if(cfg.autoCycle&&!sendFeedMotion()) {
        stopAndLock("down_motion_send_failed");feedNotice();return "rejected_feed_motion_send_failed";
      }
      feedNotice();
    }
    return result;
  }
  if(feedStage.running()&&!strcmp(cmd,"RELAY OFF")){stopAndLock("relay_off");return spiReady?"spi_pending":"rejected_spi";}
  if(feedStage.running()&&strcmp(cmd,"KEEPALIVE")&&
    !(feedStage.permitsMotion()&&feed::stageMotorCommand(cmd)))return "rejected_feed_busy";
  // 联调入口只允许本机串口；不增加前端业务按钮或裸开泵 MQTT 接口。
  if(!strncmp(cmd,"RELAY",5)) {
    if(remote)return "rejected_local_only";
    relay::Command c=relay::parse(cmd);
    if(c.kind==relay::Kind::Invalid)return "rejected_relay_command";
    if(c.kind==relay::Kind::Status){reportRelays();return "accepted_local";}
    if(c.kind==relay::Kind::Off){stopRelays("relay_command");return "accepted_relays_off";}
    if(lease.armed)return "rejected_motor_armed";
    if(c.kind==relay::Kind::Pulse&&(portal.load()||link.pending))return "rejected_config_or_stop_pending";
    if(c.kind==relay::Kind::Configure) {
      if(relays.running()||link.pending||probes.active)return "rejected_relay_busy";
      if(!persistRelaySettings(c.settings))return "rejected_relay_save_failed";
      relays.configure(c.settings);relayPinsReady=false;applyRelayOutputs();reportRelays();return "accepted_relay_config";
    }
    if(polling&&(!linkVerified||!link.online(millis())||linkReset))return "rejected_foc_unverified";
    const char* result=relays.start(c.channel,c.duration,millis(),polling);
    if(!strncmp(result,"accepted",8)) {
      applyRelayOutputs();Serial.printf("RELAY_PULSE channel=%s duration_ms=%lu link_bound=%u OUTPUT_ONLY\n",
        c.channel==0?"MILK":"WATER",static_cast<unsigned long>(c.duration),polling);
    }
    return result;
  }
  if(!strncmp(cmd,"SPITEST",7)) {
    if(remote)return "rejected_local_only";
    uint32_t count=0;if(!diagnostic::parseCount(cmd,count))return "rejected_spi_test_command";
    if(lease.armed)return "rejected_motor_armed";
    if(!spiReady)return "rejected_spi";
    if(link.pending||probes.active)return "rejected_spi_busy";
    if(!probes.begin(count))return "rejected_spi_test_command";
    polling=true;return "accepted_spi_test";
  }
#if FOC_M1_ONLY
  if(!strcmp(cmd,"RECOVER")) {
    if(remote)return "rejected_local_only";
    if(lease.armed||feedStage.running()||relays.running()||probes.active)
      return "rejected_stop_first";
    if(!spiReady)return "rejected_spi";
    if(link.pending)return "rejected_spi_busy";
    if(!linkVerified||!link.online(millis())||linkReset)return "rejected_foc_unverified";
    if(link.status.mode0||link.status.mode1)return "rejected_stop_first";
    lease.lock();xQueueReset(commands);
    return transmit("RECOVER",false,0,"")?"spi_pending":"rejected_spi_busy";
  }
#endif
  if(!strcmp(cmd,"PING")) {
    if(probes.active)return "rejected_spi_test_busy";
    return transmit("PING",remote,epoch,request)?"spi_pending":"rejected_spi_busy";
  }
  if(portal.load())return "rejected_config_mode";
  if(remote&&(!netOnline.load()||epoch!=netEpoch.load()))return "rejected_stale_session";
  if(!strcmp(cmd,"ARM")) {
    if(relays.running()||probes.active)return "rejected_diagnostic_busy";
    if(!spiReady)return "rejected_spi";
    if(!linkVerified||!link.online(millis())||linkReset)return "rejected_foc_unverified";
#if FOC_M1_ONLY
    if(!feed::Controller::feedbackValid(link.status,true))return "rejected_foc_feedback";
#endif
    if(lease.armed && lease.remote!=remote)return "rejected_owner";
    lease.arm(remote,epoch,millis());return "accepted_local";
  }
  if(!strcmp(cmd,"KEEPALIVE")) {
    if(!remote||!lease.armed||!lease.remote)return "rejected_not_armed";
    lease.renew(millis());return "accepted_local";
  }
  bool zero=false;
  if(!foc::validMotorCommand(cmd,zero))return "rejected_bad_command";
  if(zero){stopAndLock("zero",remote,epoch,request);return spiReady?"spi_pending":"rejected_spi";}
#if FOC_M1_ONLY
  FlpMotor x{},z{};
  if(!flp_motor_pair(cmd,strlen(cmd),&x,&z)||x.mode!='o'||x.value!=0)return "rejected_m0_disabled";
#endif
  if(!lease.armed)return "rejected_not_armed";
  if(lease.remote!=remote)return "rejected_owner";
  if(!linkVerified||!link.online(millis())||linkReset)return "rejected_foc_unverified";
#if FOC_M1_ONLY
  if(!feed::Controller::feedbackValid(link.status,true))return "rejected_foc_feedback";
#endif
  bool ok=transmit(cmd,remote,epoch,request);if(ok&&remote)lease.renew(millis());
  return ok?"spi_pending":"rejected_spi_busy";
}

// 以下对象只由网络任务访问；主循环通过队列传命令和状态，不调用阻塞式 MQTT API。
WiFiClient transport;
PubSubClient mqtt(transport);
char session[33]={};
char recent[16][48]={};uint8_t recentIndex=0;
static void publishAck(const char* req,const char* result,uint32_t tx,bool armed) {
  char body[320];
  snprintf(body,sizeof(body),"{\"device_id\":\"%s\",\"request_id\":\"%s\",\"result\":\"%s\",\"tx\":%lu,\"armed\":%u}",config.device,req,result,static_cast<unsigned long>(tx),armed);
  mqtt.publish(topicAck,body,false);
}
static void receive(char*,byte* payload,unsigned int length) {
  gateway::Command c{};
  // 仅无害探测和停止保留纯文本兼容；ARM 和运动命令必须使用当前会话 JSON。
  if(length==4&&!memcmp(payload,"PING",4))strcpy(c.text,"PING");
  else if(length==4&&!memcmp(payload,"STOP",4))strcpy(c.text,"STOP");
  else if(!gateway::parseCommand(reinterpret_cast<char*>(payload),length,c)){publishAck("","rejected_bad_json",0,false);return;}
  if(!gateway::remoteAllowed(c.text)){publishAck(c.request,"rejected_local_only",0,false);return;}
  bool stop=gateway::isStop(c.text);
  if(!gateway::sessionAllowed(c,session)){publishAck(c.request,"rejected_stale_session",0,false);return;}
  if(portal.load()){publishAck(c.request,"rejected_config_mode",0,false);return;}
  // 同一会话最近十六个请求去重；停止命令不去重，允许再次尝试停止。
  if(!stop&&c.request[0])for(auto& id:recent)if(!strcmp(c.request,id)){publishAck(c.request,"duplicate_ignored",0,false);return;}
  QueuedCommand q{c,netEpoch.load()};
  if(stop) {xQueueOverwrite(stops,&q);xQueueReset(commands);}
  else if(xQueueSend(commands,&q,0)!=pdTRUE){publishAck(c.request,"rejected_queue_full",0,false);return;}
  if(!stop&&c.request[0]){strcpy(recent[recentIndex],c.request);recentIndex=(recentIndex+1)%16;}
}
static void networkTask(void*) {
  transport.setConnectionTimeout(1500);
  mqtt.setServer(config.host,config.port);mqtt.setCallback(receive);
  if(!mqtt.setBufferSize(1536)){Serial.println("ERR MQTT_NO_MEMORY");vTaskDelete(nullptr);return;}
  mqtt.setSocketTimeout(2);mqtt.setKeepAlive(10);networkTaskReady=true;
  uint32_t retry=0,lastPublish=0,lastWifiRetry=0;
  Snapshot snapshot{};
  for(;;) {
    if(portal.load()||WiFi.status()!=WL_CONNECTED) {
      netOnline=false;
      if(mqtt.connected())mqtt.disconnect();
      transport.stop();
      if(!portal.load()&&millis()-lastWifiRetry>=10000){lastWifiRetry=millis();WiFi.reconnect();}
      vTaskDelay(pdMS_TO_TICKS(50));continue;
    }
    if(!mqtt.connected()) {
      netOnline=false;
      if(millis()-retry<3000){vTaskDelay(pdMS_TO_TICKS(20));continue;}
      retry=millis();randomToken(session,sizeof(session));++netEpoch;
      xQueueReset(commands);xQueueReset(acks);memset(recent,0,sizeof(recent));recentIndex=0;
      char clientId[64],will[200];
      snprintf(clientId,sizeof(clientId),"%s-%06lx",config.device,static_cast<unsigned long>(ESP.getEfuseMac()&0xFFFFFF));
      snprintf(will,sizeof(will),"{\"device_id\":\"%s\",\"mqtt\":0,\"event\":\"offline\"}",config.device);
      bool ok=mqtt.connect(clientId,config.username[0]?config.username:nullptr,config.username[0]?config.password:nullptr,topicStatus,1,true,will);
      if(!ok){Serial.printf("MQTT_CONNECT_FAILED state=%d\n",mqtt.state());continue;}
      if(portal.load()||!mqtt.subscribe(topicCmd,1)){mqtt.disconnect();continue;}
      netOnline=true;lastPublish=millis()-1000;
      Serial.printf("MQTT_CONNECTED session_id=%s\n",session);
    }
    if(!mqtt.loop()){netOnline=false;continue;}
    Ack ack{};
    // 有界处理，避免遥测或 ACK 堆积长期占用网络任务。
    for(int i=0;i<4 && xQueueReceive(acks,&ack,0)==pdTRUE;++i)
      if(ack.epoch==netEpoch.load())publishAck(ack.request,ack.result,ack.tx,ack.armed);
    xQueuePeek(snapshots,&snapshot,0);
    if(millis()-lastPublish>=1000) {
      lastPublish=millis();char body[1400];
      uint32_t queueAge=millis()-snapshot.capturedAt;
      uint32_t age=snapshot.age==UINT32_MAX?UINT32_MAX:snapshot.age+queueAge;
      bool fresh=snapshot.online&&queueAge<1000&&age<1000;
      // 只有真实从机快照来自 SPI；失联保留最后值但明确标记 stale，不补造测量。
      snprintf(body,sizeof(body),"{\"device_id\":\"%s\",\"event\":\"heartbeat\",\"mqtt\":1,\"wifi\":1,\"spi_ready\":%u,\"armed\":%u,\"tx\":%lu,\"rejected\":%lu,\"session_id\":\"%s\",\"lease_ms\":5000,\"protocol\":2,\"foc_link\":\"%s\",\"telemetry_valid\":%u,\"foc_age_ms\":%lu,\"sample\":%lu,\"foc_uptime_ms\":%lu,\"m0_rpm\":%.2f,\"m1_rpm\":%.2f,\"m0_iq_a\":%.3f,\"m1_iq_a\":%.3f,\"m0_mode\":%u,\"m1_mode\":%u,\"contact0\":%u,\"contact1\":%u,\"encoder0_enabled\":%u,\"encoder1_enabled\":%u,\"spi_owner\":%u,\"link_faults_latched\":%u,\"bad_frames\":%lu,\"relay_configured\":%u,\"milk_output\":%u,\"water_output\":%u,\"feed_phase\":\"%s\",\"feed_configured\":%u,\"feed_busy\":%u}",
        config.device,snapshot.spi,snapshot.armed,static_cast<unsigned long>(snapshot.tx),static_cast<unsigned long>(snapshot.rejected),session,
        fresh&&snapshot.verified?"verified":"unverified",fresh,static_cast<unsigned long>(age),
        static_cast<unsigned long>(snapshot.foc.sample),static_cast<unsigned long>(snapshot.foc.uptime),
        snapshot.foc.rpm0/100.0,snapshot.foc.rpm1/100.0,snapshot.foc.iq0/1000.0,snapshot.foc.iq1/1000.0,
        snapshot.foc.mode0,snapshot.foc.mode1,!!(snapshot.foc.flags&FLP_CONTACT0),!!(snapshot.foc.flags&FLP_CONTACT1),
        !!(snapshot.foc.flags&FLP_ENCODER0),!!(snapshot.foc.flags&FLP_ENCODER1),!!(snapshot.foc.flags&FLP_SPI_OWNER),
        snapshot.foc.faults,static_cast<unsigned long>(snapshot.foc.badFrames),snapshot.relayConfigured,
        snapshot.relayActive==0,snapshot.relayActive==1,feed::name(snapshot.feedState),snapshot.feedConfigured,
        feed::Controller::active(snapshot.feedState));
      mqtt.publish(topicStatus,body,true);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
static void handleLocal(char* line) {
  String s=line;s.trim();if(s.isEmpty())return;
  String upper=s;upper.toUpperCase();
  if(upper=="CONFIG"){startPortal();return;}
  if(upper=="FACTORY_RESET") {
    if(lease.armed||relays.running()||probes.active||feedStage.running()){Serial.println("ERR STOP_BEFORE_RESET");return;}
    startPortal();
    if(!portal.load()){Serial.println("ERR CONFIG_MODE_REQUIRED");return;}
    if(!clearConfig()){Serial.println("ERR NVS_CLEAR_FAILED");return;}
    restartAt=millis()+1000;Serial.println("NETWORK_CONFIG_CLEARED");return;
  }
  if(upper=="STATUS"){reportStatus();return;}
  if(upper=="HELP"||upper=="?") {
    Serial.println("LOCAL / CONFIG / FACTORY_RESET / STATUS / PING / RECOVER / ARM / FEED / STOP; MQTT motion requires current session_id; request_id OPTIONAL; KEEPALIVE every 1s");
    Serial.println("RECOVER: local only, STOP and wait ACK first; 3 fresh stationary encoder reads; no automatic ARM, calibration or motion replay");
    Serial.println("LOCAL: exit setup while stopped; no WiFi/MQTT needed; re-PING before FEED; remote STOP remains available; reboot resets mode");
    Serial.println("FEED: automatic M1 o0,v100 down, contact-stop, timed milk, o0,v-100 return to a NEW contact and stop; no X movement or CLEAN");
    Serial.println("FEED_ABORT: original checked frame, printed after STOP; rpm*_x100 units = 0.01 RPM");
    Serial.println("LOCAL DIAGNOSTICS: RELAY CONFIG LOW|HIGH [LOW|HIGH]; RELAY MILK|WATER 1..2000; RELAY OFF; RELAY STATUS; SPITEST 1..50");return;
  }
  if(upper=="LOCAL"||upper=="ARM"||upper=="FEED"||upper=="STOP"||upper=="DISARM"||upper=="PING"||upper=="RECOVER"||upper.startsWith("RELAY")||upper.startsWith("SPITEST"))s=upper;
  const char* result=execute(s.c_str());
  if(!strncmp(result,"rejected",8))++rejectedCount;
  Serial.printf("LOCAL_RESULT %s\n",result);
}
static void console() {
  static char line[96]={};static size_t n=0;static bool drop=false;static uint32_t last=0;
  if(n&&millis()-last>=1500){n=0;drop=true;Serial.println("ERR LINE_TIMEOUT");}
  for(int i=0;i<64&&Serial.available();++i) {
    int c=Serial.read();last=millis();
    if(c=='\r'||c=='\n'){if(!drop&&n){line[n]=0;handleLocal(line);}n=0;drop=false;continue;}
    if(drop)continue;
    if(c<32||c>126||n>=sizeof(line)-1){n=0;drop=true;Serial.println("ERR BAD_LINE");continue;}
    line[n++]=static_cast<char>(c);
  }
}
static void begin() {
  // 默认使用用户确认的高电平模块；保存的极性优先，加载后立即请求两路关闭。
  pinMode(relay::MILK_PIN,INPUT);pinMode(relay::WATER_PIN,INPUT);
  relays.configure(relay::defaultSettings());
  loadRelaySettings();applyRelayOutputs();
  Serial.begin(115200);delay(300);setCpuFrequencyMhz(80);
  pinMode(CS,OUTPUT);digitalWrite(CS,HIGH);pinMode(0,INPUT_PULLUP);
  spiReady=spi.begin(SCK,MISO,MOSI,CS);
  commands=xQueueCreate(8,sizeof(QueuedCommand));stops=xQueueCreate(1,sizeof(QueuedCommand));acks=xQueueCreate(12,sizeof(Ack));snapshots=xQueueCreate(1,sizeof(Snapshot));
  if(!commands||!stops||!acks||!snapshots){Serial.println("FATAL QUEUE_NO_MEMORY");return;}
  nextId=esp_random();
  configured=loadConfig();
    Serial.println("Rabbit ESP32-S3 gateway v8.2 M1_ONLY AUTO_FEED_RETURN RECOVER; paired FOC_M1_ONLY required; boot sends NO SPI command");reportStatus();
    Serial.println("USB-only: LOCAL, PING, wait foc_pong, then FEED starts M1 automatically and returns after milk; STOP cancels the cycle");
  if(!configured){config=gateway::Config{};startPortal();return;}
  snprintf(topicCmd,sizeof(topicCmd),"rabbit/%s/cmd",config.device);snprintf(topicAck,sizeof(topicAck),"rabbit/%s/ack",config.device);snprintf(topicStatus,sizeof(topicStatus),"rabbit/%s/status",config.device);
  WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.begin(config.ssid,config.wifiPassword);
  if(xTaskCreate(networkTask,"rabbit-mqtt",8192,nullptr,1,nullptr)!=pdPASS)Serial.println("ERR NETWORK_TASK_FAILED use serial CONFIG");
}
static void tick() {
  serviceRelays();
  if(!commands||!stops||!acks||!snapshots){delay(100);return;}
  // 主循环独占 SPI 和电机状态；网络任务阻塞不会拖住串口停止和租约检查。
  if(lease.expired(millis(),netOnline.load()&&WiFi.status()==WL_CONNECTED,netEpoch.load())){stopAndLock("network_lease_expired");xQueueReset(commands);}
  serviceLink();
  serviceRelays();
  console();
  QueuedCommand q{};
  bool haveStop=xQueueReceive(stops,&q,0)==pdTRUE;
  if(haveStop||xQueueReceive(commands,&q,0)==pdTRUE) {
    const char* result=execute(q.value.text,true,q.epoch,q.value.request);
    if(!strncmp(result,"rejected",8))++rejectedCount;
    Ack a{};strcpy(a.request,q.value.request);strncpy(a.result,result,sizeof(a.result)-1);a.epoch=q.epoch;a.tx=txCount;a.armed=lease.armed;
    if(xQueueSend(acks,&a,0)!=pdTRUE)Serial.println("WARN ACK_QUEUE_FULL");
  }
  // STOP队列和串口命令已先处理，再允许业务阶段判断是否开奶泵。
  serviceDiagnostics();serviceRelays();serviceFeed();serviceRelays();
  Snapshot s{txCount,rejectedCount,link.seen?millis()-link.receivedAt:UINT32_MAX,millis(),
    lease.armed,spiReady,link.online(millis()),linkVerified,link.status,relays.configured,relays.active,
    feedStage.state,feed::Controller::valid(feedconfig::settings())};xQueueOverwrite(snapshots,&s);
  // BOOT 仅在程序运行且已锁定时可进入配网；上电时不要按住，以免进入下载模式。
  static uint32_t pressed=0;
  if(digitalRead(0)==LOW&&!lease.armed){if(!pressed)pressed=millis();if(millis()-pressed>=5000)startPortal();}else pressed=0;
  if(portal.load()) {
    web.handleClient();
    if(configured && millis()-portalSince>=600000 && !restartAt){web.stop();WiFi.softAPdisconnect(true);portal=false;Serial.println("CONFIG_TIMEOUT");}
  }
  if(restartAt&&static_cast<int32_t>(millis()-restartAt)>=0){stopRelays("restart");probes.cancel();ESP.restart();}
  delay(2);
}
} // app 命名空间
