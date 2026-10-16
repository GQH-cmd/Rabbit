#ifndef FOC_M1_ONLY
#define FOC_M1_ONLY 0 // 此文件原双轴回归；单轴入口另行用真实生产函数运行。
#endif
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include "Arduino.h"
#include "SPI.h"
#include "GatewayValidation.h"
#include "FocLinkClient.h"
#include "RelayControl.h"
// 保留真实编译默认值；仅把配置读取边界替换为可调整的主机测试夹具。
#define feedconfig compiledfeedconfig
#include "FeedStageConfig.h"
#undef feedconfig
namespace feedconfig {
inline feed::Settings current = compiledfeedconfig::settings();
inline feed::Settings settings(){return current;}
}
constexpr uint8_t CS=15;
SPIClass spi(FSPI);
gateway::Lease lease{};
bool spiReady=true,sentFrame=false,polling=false,linkVerified=false,linkReset=false;
uint32_t txCount=0,lastFrame=0,nextId=0,rejectedCount=0;
bool configured=false;
foclink::Client link;
relay::Controller relays;
diagnostic::ProbeBatch probes;
feed::Controller feedStage;
struct FeedSource {char request[48]={};uint32_t epoch=0;bool remote=false;} feedSource;
feed::State reportedFeedState=feed::State::Idle;
bool relayPinsReady=false;
int appliedRelay=-1;
bool saveOk=true;
bool persistRelaySettings(const relay::Settings& settings){return saveOk&&relay::valid(settings);}
struct Pending {char request[48];uint32_t epoch;bool remote;} pending{};
struct Ack {char request[48];char result[40];uint32_t epoch,tx;bool armed;};
std::vector<Ack> captured;
int acks=1,commands=2,resets=0;
constexpr int pdTRUE=1;
int xQueueSend(int,const Ack* a,int){captured.push_back(*a);return pdTRUE;}
void xQueueReset(int){++resets;}
std::atomic<bool> portal{false},netOnline{false},localOnly{false};
std::atomic<uint32_t> netEpoch{0};
uint32_t restartAt=0;
// 仅替换无线/AP和HTTP服务器边界，模式切换逻辑仍执行生产execute()。
struct PortalServer {
  bool running=false;
  void begin(){running=true;}
  void stop(){running=false;}
} web;
struct PortalWifi {
  bool apActive=false,stopOk=true;
  int status()const{return 0;}
  bool softAPdisconnect(bool turnOff){if(!stopOk)return false;if(turnOff)apActive=false;return true;}
} WiFi;
constexpr int WL_CONNECTED=3;
// 配网/NVS属于本次不执行的硬件边界；LOCAL不得进入这些流程。
static void startPortal(){std::abort();}
static bool clearConfig(){std::abort();}
// 直接提取生产函数，覆盖发帧、回执关联、锁定和超时。
#include "gateway_runtime_extracted.h"
static int count=0;
#define CHECK(x) do {++count;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static FlpStatus status{};
static void reply(uint32_t id,uint8_t result){
  uint8_t rx[FLP_SIZE];status.flags=FLP_READY|FLP_ACK|(FOC_M1_ONLY?(0x80|FLP_ENCODER1):0);status.id=id;status.result=result;
  ++status.sample;status.uptime=millis();flp_reply(rx,&status);
  fake::replies.emplace_back(rx,rx+FLP_SIZE);
}
static void pollReply(uint32_t id,uint8_t result){fake::now+=101;reply(id,result);serviceLink();}
static uint32_t feedSample=0;
static void feedReply(int32_t rpm,bool contact,uint32_t id=0,uint8_t result=FLP_ACCEPTED,uint16_t faults=0){
  FlpStatus s{};s.sample=++feedSample;s.uptime=fake::now;
  s.flags=FLP_READY|FLP_ENCODER1|(FOC_M1_ONLY?0x80:FLP_ENCODER0);
  s.rpm1=rpm;s.mode1=rpm?2:0;s.faults=faults;
  if(contact)s.flags|=FLP_CONTACT1;
  if(id){s.flags|=FLP_ACK;s.id=id;s.result=result;}
  uint8_t rx[FLP_SIZE];flp_reply(rx,&s);fake::replies.emplace_back(rx,rx+FLP_SIZE);
}
static void feedFrame(int32_t rpm,bool contact,uint32_t id=0,uint8_t result=FLP_ACCEPTED){
  fake::now+=100;feedReply(rpm,contact,id,result);
  serviceLink();serviceRelays();serviceFeed();serviceRelays();
}
static int feedRig(bool calibrated=true,int8_t downSign=-1){
  fake::millisStep=0;fake::now=10000;fake::replies.clear();fake::frames.clear();fake::times.clear();fake::writes.clear();fake::relayAtTransfer.clear();
  link=foclink::Client{};lease=gateway::Lease{};feedStage=feed::Controller{};
  feedSource=FeedSource{};reportedFeedState=feed::State::Idle;
  probes=diagnostic::ProbeBatch{};relays=relay::Controller{};relays.configure(relay::defaultSettings());
  spiReady=true;sentFrame=polling=linkVerified=linkReset=relayPinsReady=false;
  appliedRelay=-1;txCount=lastFrame=0;nextId=100;feedSample=0;pending=Pending{};
  portal=false;netOnline=false;localOnly=false;netEpoch=0;restartAt=0;captured.clear();
  web.running=WiFi.apActive=false;
  WiFi.stopOk=true;
  configured=false;Serial.input.clear();Serial.output.clear();
  feedconfig::current=compiledfeedconfig::settings();
  // 显式保留旧分段链路的回归夹具；一键生产流程由m1_auto_feed_cycle_test恢复autoCycle测试。
  feedconfig::current.autoCycle=false;
  feedconfig::current.milkMs=3000;feedconfig::current.downSign=calibrated?downSign:0;
  applyRelayOutputs();feedReply(0,false,101,FLP_PONG);
  CHECK(!strcmp(execute("PING"),"spi_pending"));CHECK(linkVerified&&!link.pending);
  return 0;
}
static int feedToMilk(bool advancingClock=false,int8_t downSign=-1){
  CHECK(feedRig(true,downSign)==0);
  CHECK(!strcmp(execute("FEED"),"accepted_feed_wait_motion"));
  CHECK(!strcmp(execute(downSign<0?"o0,v-90":"o0,v90"),"spi_pending"));
  feedFrame(9000*downSign,false,link.commandId);feedFrame(9000*downSign,false);
  CHECK(feedStage.state==feed::State::WaitContact);
  feedFrame(0,true);CHECK(feedStage.state==feed::State::WaitStopAck);
  FlpRequest q{};CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"o0,o0"));
  CHECK(feedStage.stopId==q.id&&link.pending);
  feedFrame(0,true,q.id);CHECK(feedStage.state==feed::State::ConfirmStopped);
  feedFrame(0,true);CHECK(!relays.running());
  fake::millisStep=advancingClock?1:0;feedFrame(0,true);fake::millisStep=0;
  CHECK(feedStage.state==feed::State::MilkRunning&&relays.active==0);
  CHECK(relays.output(0)==HIGH&&relays.output(1)==LOW);
  CHECK(fake::gpioLevel(4)==HIGH&&fake::gpioLevel(5)==LOW);
  return 0;
}
static int feedGatewayCases(){
  CHECK(feedRig(false)==0);
  CHECK(!strcmp(execute("FEED"),"rejected_feed_unconfigured"));CHECK(!relays.running());
  CHECK(feedToMilk(true)==0);
  // 两个真实生产控制器必须共用时钟起点；不同起点会在到期时误报milk_output_lost。
  CHECK(relays.startedAt==feedStage.pumpAt);
  auto frames=fake::frames.size();
  CHECK(!strcmp(execute("PING"),"rejected_feed_busy"));
  CHECK(!strcmp(execute("SPITEST 10"),"rejected_feed_busy"));
  CHECK(!strcmp(execute("o0,v-90"),"rejected_feed_busy"));CHECK(fake::frames.size()==frames);
  const auto deadline=relays.startedAt+3000;
  while(fake::now+100<deadline)feedFrame(0,true);
  fake::now=deadline-1;feedReply(0,true);serviceLink();serviceRelays();serviceFeed();
  CHECK(relays.running());
  fake::now=deadline;serviceRelays();serviceFeed();
  CHECK(!relays.running()&&feedStage.state==feed::State::Complete&&!lease.armed);
  CHECK(relays.output(0)==LOW&&relays.output(1)==LOW);
  CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  CHECK(feedToMilk()==0);
  CHECK(!strcmp(execute("STOP"),"spi_pending"));
  CHECK(!relays.running()&&feedStage.state==feed::State::Aborted&&!lease.armed);
  CHECK(relays.output(0)==LOW&&relays.output(1)==LOW);
  CHECK(fake::relayAtTransfer.back()==std::make_pair(LOW,LOW));
  CHECK(feedToMilk()==0);fake::now+=501;serviceFeed();
  CHECK(!relays.running()&&feedStage.state==feed::State::Aborted);
  CHECK(!strcmp(feedStage.reason,"feedback_stale"));
  CHECK(feedToMilk()==0);linkVerified=false;serviceFeed();
  CHECK(!relays.running()&&feedStage.state==feed::State::Aborted&&!lease.armed);
  // 正向下降完整链路也执行真实生产网关函数；这里的命令只是主机夹具。
  CHECK(feedToMilk(true,1)==0);CHECK(relays.startedAt==feedStage.pumpAt);
  const auto positiveDeadline=relays.startedAt+3000;
  while(fake::now+100<positiveDeadline)feedFrame(0,true);
  fake::now=positiveDeadline;serviceRelays();serviceFeed();
  CHECK(!relays.running()&&feedStage.state==feed::State::Complete&&!lease.armed);
  CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  return 0;
}
static void queueFeedSnapshot(const FlpStatus& s){
  uint8_t rx[FLP_SIZE];flp_reply(rx,&s);fake::replies.emplace_back(rx,rx+FLP_SIZE);
}
static int feedAbortSnapshotCases(){
  // STOP在同次交换中回传正常状态：诊断必须保留此前触发保护的帧。
  for(int modeCase=0;modeCase<2;++modeCase){
    CHECK(feedRig(true,1)==0);
    CHECK(!strcmp(execute("FEED"),"accepted_feed_wait_motion"));
    fake::now=10100;
    FlpStatus trigger{};trigger.flags=FLP_READY|FLP_ENCODER0|FLP_ENCODER1;
    trigger.sample=2;trigger.uptime=10100;
    trigger.mode0=modeCase?2:0;trigger.rpm0=modeCase?0:8500;
    queueFeedSnapshot(trigger);
    FlpStatus stopped{};stopped.flags=FLP_READY|FLP_ENCODER0|FLP_ENCODER1|FLP_ACK;
    stopped.id=102;stopped.result=FLP_ACCEPTED;stopped.sample=3;stopped.uptime=10200;
    queueFeedSnapshot(stopped);
    serviceLink();serviceFeed();
    CHECK(feedStage.state==feed::State::Aborted&&!strcmp(feedStage.reason,"x_not_stopped"));
    CHECK(!lease.armed&&!relays.running()&&link.status.mode0==0&&link.status.rpm0==0);
    CHECK(link.status.sample==3&&!link.pending); // 确实已被STOP回执覆盖。
    CHECK(Serial.output.find("FEED_ABORT reason=x_not_stopped")!=std::string::npos);
    CHECK(Serial.output.find("phase_before=wait_motion")!=std::string::npos);
    CHECK(Serial.output.find("checked_ms=10100 sample=2 uptime_ms=10100 age_ms=0")!=std::string::npos);
    CHECK(Serial.output.find(modeCase?"mode0=2 rpm0_x100=0":"mode0=0 rpm0_x100=8500")!=std::string::npos);
    CHECK(Serial.output.find("STOP_REQUEST reason=x_not_stopped")<Serial.output.find("FEED_ABORT reason=x_not_stopped"));
    FlpRequest q{};CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"o0,o0"));
    CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  }
  // 奶泵已运行时也先关闭GPIO并发STOP，然后打印快照，不能为日志延后停机。
  CHECK(feedToMilk(true,1)==0);
  CHECK(fake::gpioLevel(4)==HIGH);
  fake::now+=100;
  FlpStatus trigger{};trigger.flags=FLP_READY|FLP_ENCODER0|FLP_ENCODER1|FLP_CONTACT1;
  trigger.sample=++feedSample;trigger.uptime=fake::now;trigger.rpm0=-7000;
  queueFeedSnapshot(trigger);
  FlpStatus stopped=trigger;stopped.sample=++feedSample;stopped.uptime=fake::now+100;
  stopped.flags|=FLP_ACK;stopped.id=nextId+1;stopped.result=FLP_ACCEPTED;stopped.rpm0=0;
  queueFeedSnapshot(stopped);
  Serial.output.clear();serviceLink();serviceFeed();
  CHECK(feedStage.state==feed::State::Aborted&&!relays.running()&&!lease.armed);
  CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  CHECK(fake::relayAtTransfer.back()==std::make_pair(LOW,LOW));
  CHECK(Serial.output.find("phase_before=milk_running")!=std::string::npos);
  CHECK(Serial.output.find("mode0=0 rpm0_x100=-7000")!=std::string::npos);
  CHECK(Serial.output.find("STOP_REQUEST reason=x_not_stopped")<Serial.output.find("FEED_ABORT reason=x_not_stopped"));
  return 0;
}
static int localModeGatewayCases(){
  CHECK(feedRig(true,1)==0);
  portal=true;web.running=WiFi.apActive=true;
  const auto before=fake::frames.size();
  CHECK(!strcmp(execute("FEED"),"rejected_config_mode"));
  CHECK(!strcmp(execute("ARM"),"rejected_config_mode"));
  CHECK(!strcmp(execute("LOCAL",true),"rejected_local_only"));
  CHECK(portal&&web.running&&WiFi.apActive);
  CHECK(!strcmp(execute("LOCAL"),"accepted_local_mode"));
  CHECK(localOnly&&!portal&&!web.running&&!WiFi.apActive);
  CHECK(!lease.armed&&!relays.running()&&!linkVerified&&!polling);
  CHECK(fake::frames.size()==before); // 切换模式不发运动/开泵命令。
  CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  CHECK(!strcmp(execute("FEED"),"rejected_foc_unverified"));
  // 即使已有网络连接，LOCAL也不能被远程ARM/FEED/速度接管；STOP例外。
  netOnline=true;netEpoch=7;
  CHECK(!strcmp(execute("ARM",true,7),"rejected_local_mode"));
  CHECK(!strcmp(execute("FEED",true,7),"rejected_local_mode"));
  CHECK(!strcmp(execute("o0,v90",true,7),"rejected_local_mode"));
  CHECK(!strcmp(execute("KEEPALIVE",true,7),"rejected_local_mode"));
  netOnline=false;
  feedReply(0,false,nextId+1,FLP_PONG);
  CHECK(!strcmp(execute("PING"),"spi_pending"));CHECK(linkVerified&&!link.pending);
  CHECK(!strcmp(execute("FEED"),"accepted_feed_wait_motion"));
  CHECK(!strcmp(execute("o0,v90"),"spi_pending"));
  feedFrame(9000,false,link.commandId);feedFrame(9000,false);
  CHECK(feedStage.state==feed::State::WaitContact);
  feedFrame(0,true);CHECK(feedStage.state==feed::State::WaitStopAck);
  feedFrame(0,true,feedStage.stopId);feedFrame(0,true);feedFrame(0,true);
  CHECK(feedStage.state==feed::State::MilkRunning&&relays.active==0);
  CHECK(fake::gpioLevel(4)==HIGH&&fake::gpioLevel(5)==LOW);
  const auto deadline=feedStage.pumpAt+3000;
  while(fake::now+100<deadline)feedFrame(0,true);
  fake::now=deadline;serviceRelays();serviceFeed();
  CHECK(feedStage.state==feed::State::Complete&&!lease.armed&&!relays.running());
  CHECK(localOnly&&!netOnline&&fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  CHECK(!strcmp(execute("STOP",true,7),"spi_pending"));
  CHECK(localOnly&&!lease.armed&&!relays.running());
  FlpRequest q{};
  CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"o0,o0"));
  // 运动解锁、泵测试、探测或未决回执期间，不能偷偷切换控制模式。
  for(int busy=0;busy<4;++busy){
    CHECK(feedRig(true,1)==0);
    if(busy==0)CHECK(!strcmp(execute("ARM"),"accepted_local"));
    if(busy==1)CHECK(!strcmp(execute("RELAY MILK 500"),"accepted_relay_pulse"));
    if(busy==2)CHECK(!strcmp(execute("SPITEST 1"),"accepted_spi_test"));
    if(busy==3)CHECK(!strcmp(execute("PING"),"spi_pending"));
    portal=true;web.running=WiFi.apActive=true;
    const auto frames=fake::frames.size();
    CHECK(!strcmp(execute("LOCAL"),"rejected_stop_before_local"));
    CHECK(!localOnly&&portal&&web.running&&WiFi.apActive&&fake::frames.size()==frames);
  }
  CHECK(feedRig(true,1)==0);portal=true;restartAt=millis()+1000;
  CHECK(!strcmp(execute("LOCAL"),"rejected_restart_pending"));
  CHECK(portal&&!localOnly);
  CHECK(feedRig(true,1)==0);portal=true;web.running=WiFi.apActive=true;WiFi.stopOk=false;
  CHECK(!strcmp(execute("LOCAL"),"rejected_ap_stop_failed"));
  CHECK(portal&&!localOnly&&web.running&&WiFi.apActive&&!lease.armed);
  CHECK(!strcmp(execute("FEED"),"rejected_config_mode"));
  return 0;
}
static int localConsoleCases(){
  CHECK(feedRig(true,1)==0);portal=true;web.running=WiFi.apActive=true;
  Serial.feed(" local\r\n");console();
  CHECK(localOnly&&!portal&&!lease.armed&&!relays.running());
  CHECK(Serial.output.find("LOCAL_RESULT accepted_local_mode")!=std::string::npos);
  Serial.feed("STATUS\n");console();
  CHECK(Serial.output.find("config=0")!=std::string::npos);
  CHECK(Serial.output.find("mode=LOCAL")!=std::string::npos);
  Serial.feed("feed\n");console();
  CHECK(Serial.output.find("LOCAL_RESULT rejected_foc_unverified")!=std::string::npos);
  feedReply(0,false,nextId+1,FLP_PONG);
  Serial.feed("ping\r\n");console();CHECK(linkVerified&&!link.pending);
  Serial.feed("FEED\n");console();
  CHECK(feedStage.state==feed::State::WaitMotion&&lease.armed);
  CHECK(Serial.output.find("LOCAL_RESULT accepted_feed_wait_motion")!=std::string::npos);
  CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  Serial.feed("STOP\r\n");console();
  CHECK(feedStage.state==feed::State::Aborted&&!lease.armed&&!relays.running());
  return 0;
}
int main(){
  CHECK(fake::frames.empty());serviceLink();CHECK(fake::frames.empty());
  CHECK(!strcmp(execute("RELAY OFF"),"accepted_relays_off"));
  CHECK(!strcmp(execute("RELAY MILK 100"),"rejected_relay_unconfigured"));
  CHECK(!strcmp(execute("RELAY CONFIG LOW HIGH",true),"rejected_local_only"));
  saveOk=false;CHECK(!strcmp(execute("RELAY CONFIG LOW HIGH"),"rejected_relay_save_failed"));
  CHECK(!relays.configured);saveOk=true;
  CHECK(!strcmp(execute("RELAY CONFIG LOW HIGH"),"accepted_relay_config"));
  CHECK(relays.output(0)==HIGH&&relays.output(1)==LOW&&!relays.running());
  CHECK(!strcmp(execute("RELAY MILK 500"),"accepted_relay_pulse"));
  CHECK(relays.output(0)==LOW&&relays.output(1)==LOW);
  CHECK(!strcmp(execute("ARM"),"rejected_diagnostic_busy"));
  CHECK(!strcmp(execute("RELAY WATER 500"),"rejected_relay_busy"));
  CHECK(!strcmp(execute("RELAY MILK 0"),"rejected_relay_command"));
  fake::now+=499;serviceRelays();CHECK(relays.running());
  ++fake::now;serviceRelays();CHECK(!relays.running());
  CHECK(fake::writes.back()==std::make_pair(5,LOW));
  CHECK(!strcmp(execute("RELAY WATER 500"),"accepted_relay_pulse"));
  CHECK(!strcmp(execute("RELAY OFF"),"accepted_relays_off"));
  CHECK(!relays.running());
  CHECK(!strcmp(execute("v30,o0"),"rejected_not_armed"));
  CHECK(!strcmp(execute("ARM"),"rejected_foc_unverified"));
  CHECK(!strcmp(execute("PING"),"spi_pending"));CHECK(link.pending&&!linkVerified);
  CHECK(fake::frames.back().size()==64&&fake::frames.back()[0]==0xa6);
  CHECK(!strcmp(execute("PING"),"rejected_spi_busy"));
  auto id=link.commandId;pollReply(id+1,FLP_PONG);CHECK(link.pending&&!linkVerified);
  pollReply(id,FLP_PONG);CHECK(!link.pending&&linkVerified);
  CHECK(!strcmp(execute("RELAY WATER 2000"),"accepted_relay_pulse"));
  CHECK(relays.linkBound);
  linkVerified=false;serviceRelays();CHECK(!relays.running());linkVerified=true;
  CHECK(!strcmp(execute("FACTORY_RESET",true,1),"rejected_stale_session"));
  netOnline=true;netEpoch=1;
  CHECK(!strcmp(execute("ARM",true,1),"accepted_local"));
  CHECK(!strcmp(execute("RELAY MILK 500"),"rejected_motor_armed"));
  CHECK(!strcmp(execute("SPITEST 10"),"rejected_motor_armed"));
  CHECK(!strcmp(execute("v30,o0",true,1,"speed-1"),"spi_pending"));
  CHECK(captured.empty()); // 发帧不等于从机接受。
  CHECK(!strcmp(execute("v30,o0"),"rejected_owner"));
  CHECK(!strcmp(execute("v30,o0",true,2),"rejected_stale_session"));
  pollReply(link.commandId,FLP_ACCEPTED);
  CHECK(captured.size()==1&&!strcmp(captured.back().result,"foc_accepted"));
  CHECK(!strcmp(captured.back().request,"speed-1")&&captured.back().epoch==1);
  CHECK(!strcmp(execute("v40,o0",true,1,"speed-2"),"spi_pending"));
  CHECK(!strcmp(execute("STOP",true,1,"stop-1"),"spi_pending"));
  CHECK(!lease.armed&&!strcmp(captured.back().result,"cancelled_by_stop"));
  FlpRequest q{};CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"o0,o0"));
  pollReply(link.commandId,FLP_ACCEPTED);
  CHECK(!strcmp(captured.back().request,"stop-1")&&!strcmp(captured.back().result,"foc_accepted"));
  CHECK(!strcmp(execute("KEEPALIVE",true,1),"rejected_not_armed"));
  portal=true;CHECK(!strcmp(execute("ARM"),"rejected_config_mode"));
  CHECK(!strcmp(execute("ARM",true,1),"rejected_config_mode"));
  CHECK(!strcmp(execute("STOP"),"spi_pending"));pollReply(link.commandId,FLP_ACCEPTED);
  portal=false;netOnline=false;
  CHECK(!strcmp(execute("STOP",true,99,"s2"),"spi_pending"));pollReply(link.commandId,FLP_ACCEPTED);
  CHECK(!strcmp(execute("ARM"),"accepted_local"));
  CHECK(!strcmp(execute("v30,o0"),"spi_pending"));
  fake::now+=1600;serviceLink();CHECK(!lease.armed&&!linkVerified&&resets>0);
  CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"o0,o0"));
  CHECK(Serial.output.find("foc_timeout")!=std::string::npos);
  CHECK(!strcmp(execute("ARM"),"rejected_foc_unverified"));
  fake::now+=1600;serviceLink(); // 停止无回执不能无限重发。
  CHECK(!link.pending);CHECK(!strcmp(execute("PING"),"spi_pending"));
  pollReply(link.commandId,FLP_PONG);CHECK(linkVerified);
  CHECK(!strcmp(execute("ARM"),"accepted_local"));
  CHECK(!strcmp(execute("v30,o0"),"spi_pending"));pollReply(link.commandId,FLP_ACCEPTED);
  // 从机重启：时间倒退，必须锁定而不是自动恢复运动。
  fake::now+=101;status.uptime=0;status.sample=1;status.flags=FLP_READY;status.id=0;status.result=0;
  uint8_t rx[FLP_SIZE];flp_reply(rx,&status);fake::replies.emplace_back(rx,rx+FLP_SIZE);
  serviceLink();CHECK(linkReset);serviceLink();CHECK(!lease.armed&&!linkVerified);
  CHECK(fake::framingOk&&fake::spiHz==100000&&fake::spiMode==0);
  for(size_t i=1;i<fake::times.size();++i)CHECK(fake::times[i]-fake::times[i-1]>=100);
  // 连续探测必须实际启动，不能被每100ms只读轮询饿死。
  stopAndLock("test");pollReply(link.commandId,FLP_ACCEPTED);
  CHECK(!strcmp(execute("SPITEST 3"),"accepted_spi_test"));
  CHECK(!strcmp(execute("ARM"),"rejected_diagnostic_busy"));
  CHECK(!strcmp(execute("PING"),"rejected_spi_test_busy"));
  for(int i=0;i<3;++i){
    fake::now+=101;size_t before=fake::frames.size();serviceLink();serviceDiagnostics();
    CHECK(fake::frames.size()==before+1);CHECK(link.pending);
    CHECK(flp_read_request(fake::frames.back().data(),&q)&&!strcmp(q.text,"PING"));
    pollReply(link.commandId,FLP_PONG);
  }
  serviceDiagnostics();CHECK(!probes.active&&probes.passed==3&&probes.failed==0);
  CHECK(Serial.output.find("SPITEST_DONE total=3 pong=3 failed=0 result=PASS")!=std::string::npos);
  CHECK(!strcmp(execute("SPITEST 1"),"accepted_spi_test"));
  fake::now+=101;serviceDiagnostics();CHECK(link.pending);
  fake::now+=1600;serviceLink();serviceDiagnostics();
  CHECK(!probes.active&&probes.failed==1);
  CHECK(Serial.output.find("SPITEST_DONE total=1 pong=0 failed=1 result=FAIL")!=std::string::npos);
  CHECK(!strcmp(execute("SPITEST 0"),"rejected_spi_test_command"));
  CHECK(!strcmp(execute("SPITEST 10",true),"rejected_local_only"));
  CHECK(!strcmp(execute("SPITEST 5"),"accepted_spi_test"));
  stopAndLock("cancel_probe");CHECK(!probes.active);pollReply(link.commandId,FLP_ACCEPTED);
  CHECK(!strcmp(execute("PING"),"spi_pending"));pollReply(link.commandId,FLP_PONG);
  CHECK(!strcmp(execute("RELAY MILK 1000"),"accepted_relay_pulse"));
  CHECK(!strcmp(execute("STOP"),"spi_pending"));CHECK(!relays.running());
  CHECK(relays.output(0)==HIGH&&relays.output(1)==LOW);pollReply(link.commandId,FLP_ACCEPTED);
  // 用户确认的 HIGH/HIGH 模块：两路关闭为 LOW，只拉高当前测试通道。
  CHECK(!strcmp(execute("RELAY CONFIG HIGH"),"accepted_relay_config"));
  CHECK(relays.output(0)==LOW&&relays.output(1)==LOW);
  CHECK(!strcmp(execute("RELAY MILK 500"),"accepted_relay_pulse"));
  CHECK(relays.output(0)==HIGH&&relays.output(1)==LOW);
  CHECK(!strcmp(execute("RELAY OFF"),"accepted_relays_off"));
  CHECK(relays.output(0)==LOW&&relays.output(1)==LOW);
  stopAndLock("test");spiReady=false;CHECK(!strcmp(execute("ARM"),"rejected_spi"));
  CHECK(feedGatewayCases()==0);
  CHECK(feedAbortSnapshotCases()==0);
  CHECK(localModeGatewayCases()==0);
  CHECK(localConsoleCases()==0);
  printf("PASS %d gateway runtime checks\n",count);return 0;
}
