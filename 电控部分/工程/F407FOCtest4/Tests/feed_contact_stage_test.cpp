#include <cstdio>
#include <climits>
#include <cstring>
#include "../build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/FeedContactStage.h"
#include "../build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/GatewayValidation.h"
#include "../build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/RelayControl.h"
#include "../build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/FeedStageConfig.h"
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return false;}}while(0)
static feed::Settings configured(){feed::Settings c;c.milkMs=1000;c.downSign=-1;return c;}
static FlpStatus frame(uint32_t sample,uint32_t now,int32_t rpm1=0,bool contact=false){
  FlpStatus s{};s.sample=sample;s.uptime=now;s.flags=FLP_READY|FLP_ENCODER0|FLP_ENCODER1;
  s.rpm1=rpm1;s.mode1=rpm1?2:0;if(contact)s.flags|=FLP_CONTACT1;return s;
}
static bool initial_and_duplicate(){
  feed::Controller c;auto s=frame(10,1000);auto cfg=configured();
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  CHECK(c.running());CHECK(c.permitsMotion());
  CHECK(!std::strcmp(c.begin(cfg,s,1001,0),"rejected_feed_busy"));
  CHECK(c.tick(frame(11,1100),1100,0,false)==feed::Action::None);
  CHECK(c.state==feed::State::WaitMotion);return true;
}
static bool happy_path(){
  feed::Controller c;auto cfg=configured();
  CHECK(!std::strcmp(c.begin(cfg,frame(1,1000),1000,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,1100,-9000),1100,0,false)==feed::Action::None);
  CHECK(c.tick(frame(3,1200,-9000),1200,0,false)==feed::Action::None);
  CHECK(c.state==feed::State::WaitContact);
  CHECK(c.tick(frame(4,1300,0,true),1300,0,false)==feed::Action::SendStop);
  CHECK(c.bindStop(77,1300));
  auto s=frame(5,1400,0,true);s.flags|=FLP_ACK;s.id=76;s.result=FLP_ACCEPTED;
  CHECK(c.tick(s,1400,0,false)==feed::Action::None);CHECK(c.state==feed::State::WaitStopAck);
  s=frame(6,1500,0,true);s.flags|=FLP_ACK;s.id=77;s.result=FLP_ACCEPTED;
  CHECK(c.tick(s,1500,0,false)==feed::Action::None);
  CHECK(c.tick(s,1510,0,false)==feed::Action::None); // 重复帧不算第二次停稳。
  CHECK(c.tick(frame(7,1600,0,true),1600,0,false)==feed::Action::None);
  CHECK(c.tick(frame(8,1700,0,true),1700,0,false)==feed::Action::StartMilk);
  CHECK(c.pumpStarted(1700));
  CHECK(c.tick(frame(9,2000,0,true),2000,0,true)==feed::Action::None);
  CHECK(c.tick(frame(10,2700,0,true),2700,0,false)==feed::Action::StopMilk);
  CHECK(c.state==feed::State::Complete);CHECK(!c.running());
  CHECK(c.tick(frame(11,2800,0,true),2800,0,false)==feed::Action::None);return true;
}
static bool no_false_contact(){
  auto cfg=configured();feed::Controller c;
  CHECK(!std::strcmp(c.begin(cfg,frame(1,1000,0,true),1000,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,1100,0,true),1100,0,false)==feed::Action::None);
  CHECK(c.tick(frame(3,1200,0,true),1200,0,false)==feed::Action::None);
  CHECK(c.state==feed::State::WaitMotion);
  auto s=frame(4,1300);s.flags|=FLP_CONTACT0;
  CHECK(c.tick(s,1300,0,false)==feed::Action::None);CHECK(c.state==feed::State::WaitMotion);
  CHECK(c.tick(frame(5,1400,-10000),1400,0,false)==feed::Action::None);
  CHECK(c.tick(frame(6,1500,0,true),1500,0,false)==feed::Action::None); // 只见一次运动，仍不放行。
  return true;
}
static bool fail_closed(){
  auto cfg=configured();auto s=frame(1,1000);feed::Controller c;
  auto noTime=cfg;noTime.milkMs=0;CHECK(!std::strncmp(c.begin(noTime,s,1000,0),"rejected",8));
  auto noSign=cfg;noSign.downSign=0;CHECK(!std::strncmp(c.begin(noSign,s,1000,0),"rejected",8));
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,1100,9000),1100,0,false)==feed::Action::Abort);CHECK(!c.running());
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,1100),1100,501,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  auto bad=frame(2,1100);bad.flags&=~FLP_ENCODER1;
  CHECK(c.tick(bad,1100,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  bad=frame(2,1100);bad.faults=FLP_FAULT_WATCHDOG;CHECK(c.tick(bad,1100,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  bad=frame(2,900);CHECK(c.tick(bad,1100,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  bad=frame(2,1100);bad.rpm0=8000;CHECK(c.tick(bad,1100,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,s,1000,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,1000+cfg.waitMotionMs),1000+cfg.waitMotionMs,0,false)==feed::Action::Abort);
  c.cancel("user_stop");CHECK(c.tick(frame(3,32000,0,true),32000,0,false)==feed::Action::None);
  return true;
}
static bool protocol_compatibility(){
  gateway::Command c{};const char* noId="{\"command\":\"FEED\",\"session_id\":\"current\"}";
  CHECK(gateway::parseCommand(noId,std::strlen(noId),c));CHECK(!c.request[0]);
  CHECK(gateway::sessionAllowed(c,"current"));CHECK(!gateway::sessionAllowed(c,"other"));
  std::strcpy(c.text,"STOP");CHECK(gateway::sessionAllowed(c,"other"));
  CHECK(feed::stageMotorCommand("o0,o2"));CHECK(feed::stageMotorCommand("o0,v10"));
  CHECK(!feed::stageMotorCommand("o1,o2"));CHECK(!feed::stageMotorCommand("p20,o2"));
  CHECK(!feed::stageMotorCommand("o0,p10"));CHECK(!feed::stageMotorCommand("o0,o0"));
  CHECK(gateway::remoteAllowed("FEED"));CHECK(gateway::remoteAllowed("STOP"));
  CHECK(!gateway::remoteAllowed("CLEAN")); // 尚未实现的完整任务不能假接受。
  const char* bad="{\"command\":\"FEED\",\"command\":\"STOP\"}";
  CHECK(!gateway::parseCommand(bad,std::strlen(bad),c));return true;
}
static bool relay_integration(){
  relay::Controller r;r.configure(relay::defaultSettings());
  CHECK(!std::strcmp(r.startMilk(3000,30000,100),"accepted_milk_stage"));
  CHECK(r.active==0);CHECK(r.output(0)==1);CHECK(r.output(1)==0);
  CHECK(!r.tick(3099,true));CHECK(r.running());
  CHECK(!std::strcmp(r.tick(3100,true),"milk_timer_expired"));CHECK(!r.running());
  CHECK(r.output(0)==0&&r.output(1)==0);
  CHECK(!std::strcmp(r.startMilk(3000,30000,100),"accepted_milk_stage"));
  CHECK(!std::strcmp(r.tick(200,false),"foc_link_lost"));CHECK(!r.running());
  CHECK(!std::strcmp(r.start(0,3000,100,true),"rejected_relay_duration")); // 本机诊断仍限制2秒。
  CHECK(!std::strcmp(r.startMilk(30001,30000,100),"rejected_relay_duration"));
  return true;
}
static bool prepare_milk(feed::Controller& c){
  auto cfg=configured();
  CHECK(!std::strcmp(c.begin(cfg,frame(1,1000),1000,0),"accepted_feed_wait_motion"));
  c.tick(frame(2,1100,-9000),1100,0,false);c.tick(frame(3,1200,-9000),1200,0,false);
  CHECK(c.tick(frame(4,1300,0,true),1300,0,false)==feed::Action::SendStop);
  CHECK(c.bindStop(77,1300));auto s=frame(5,1400,0,true);s.flags|=FLP_ACK;s.id=77;s.result=FLP_ACCEPTED;
  c.tick(s,1400,0,false);c.tick(frame(6,1500,0,true),1500,0,false);
  CHECK(c.tick(frame(7,1600,0,true),1600,0,false)==feed::Action::StartMilk);CHECK(c.pumpStarted(1600));return true;
}
static bool abort_during_milk(){
  feed::Controller c;CHECK(prepare_milk(c));
  CHECK(c.tick(frame(8,1700,0,true),1700,0,false)==feed::Action::Abort);CHECK(!c.running());
  CHECK(prepare_milk(c));CHECK(c.tick(frame(8,1700,-9000,true),1700,0,true)==feed::Action::Abort);
  CHECK(prepare_milk(c));CHECK(c.tick(frame(8,1700,0,true),1700,501,true)==feed::Action::Abort);
  CHECK(prepare_milk(c));auto s=frame(8,1700,0,true);s.faults=FLP_FAULT_FRAME;
  CHECK(c.tick(s,1700,0,true)==feed::Action::Abort);
  CHECK(prepare_milk(c));c.cancel("stop");CHECK(c.tick(frame(8,1700,0,true),1700,0,true)==feed::Action::None);
  return true;
}
static bool ack_failure_and_wrap(){
  feed::Controller c;auto cfg=configured();
  c.begin(cfg,frame(1,1000),1000,0);c.tick(frame(2,1100,-9000),1100,0,false);c.tick(frame(3,1200,-9000),1200,0,false);
  c.tick(frame(4,1300,0,true),1300,0,false);CHECK(c.bindStop(77,1300));
  auto bad=frame(5,1400,0,true);bad.flags|=FLP_ACK;bad.id=77;bad.result=FLP_BAD_COMMAND;
  CHECK(c.tick(bad,1400,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.begin(cfg,frame(0xfffffffdU,0xfffffff0U),0xfffffff0U,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(0xfffffffeU,0x54U,-9000),0x54U,0,false)==feed::Action::None);
  CHECK(c.tick(frame(0xffffffffU,0xb8U,-9000),0xb8U,0,false)==feed::Action::None);
  CHECK(c.tick(frame(0U,0x11cU,0,true),0x11cU,0,false)==feed::Action::SendStop);
  return true;
}
static bool real_wire_and_three_seconds(){
  // 实际生成的一键配置；真实协议往返覆盖下降、3秒供奶、回程及第二次新接触。
  feed::Controller c;relay::Controller r;r.configure(relay::defaultSettings());auto cfg=feedconfig::settings();
  CHECK(cfg.autoCycle);
  CHECK(!std::strcmp(c.begin(cfg,frame(1,1000),1000,0),"accepted_feed_starting"));
  CHECK(c.bindMotion(66,1000));
  uint32_t sample=1;bool started=false,returned=false,completed=false;
  for(uint32_t now=1100;now<=5400;now+=100){
    int32_t rpm=(now<=1200)?9000:((now==4800||now==4900)?-9000:0);
    bool contact=now>=1300&&now!=4800&&now!=4900;
    auto source=frame(++sample,now,rpm,contact);
    uint32_t ack=now<=1200?66:(now<4700?77:(now<5100?88:99));
    source.flags|=FLP_ACK;source.id=ack;source.result=FLP_ACCEPTED;
    uint8_t wire[FLP_SIZE];flp_reply(wire,&source);FlpStatus s{};CHECK(flp_read_reply(wire,&s));
    r.tick(now,true);auto action=c.tick(s,now,0,r.active==0);
    if(action==feed::Action::SendStop)CHECK(c.bindStop(c.returning()?99:77,now));
    if(action==feed::Action::StartMilk){CHECK(!started);started=true;
      CHECK(!std::strcmp(r.startMilk(cfg.milkMs,cfg.maxMilkMs,now),"accepted_milk_stage"));CHECK(c.pumpStarted(now));
      CHECK(now==1600);
    }
    if(now>=1600&&now<4600){CHECK(r.active==0);CHECK(r.output(1)==0);}
    if(action==feed::Action::StartReturn){
      CHECK(now==4600&&!returned);r.stop();returned=true;
      CHECK(!std::strcmp(c.motionCommand(),"o0,v-100"));CHECK(c.bindMotion(88,now));
    }
    if(action==feed::Action::FinishCycle){CHECK(now==5300&&!completed);completed=true;}
    if(now>=4600)CHECK(!r.running());
    CHECK(action!=feed::Action::Abort);
  }
  CHECK(started&&returned&&completed);CHECK(c.state==feed::State::CycleComplete);CHECK(!r.running());return true;
}
static bool production_direction_guard(){
  feed::Controller c;auto cfg=feedconfig::settings();
  CHECK(!std::strcmp(c.begin(cfg,frame(1,1000),1000,0),"accepted_feed_starting"));
  CHECK(c.bindMotion(66,1000));
  // 用户确认下降反馈为正；负反馈不得进入供奶阶段。
  CHECK(c.tick(frame(2,1100,-9000),1100,0,false)==feed::Action::Abort);
  CHECK(!std::strcmp(c.reason,"wrong_z_direction"));CHECK(!c.running());return true;
}
int main(){
  if(!initial_and_duplicate()||!happy_path()||!no_false_contact()||!fail_closed()||!protocol_compatibility()||
     !relay_integration()||!abort_during_milk()||!ack_failure_and_wrap()||!real_wire_and_three_seconds()||!production_direction_guard())return 1;
  auto uncalibrated=feedconfig::settings();if(uncalibrated.downSign==0&&feed::Controller::valid(uncalibrated))return 1;
  std::puts("PASS: contact-feed stage, fresh M1 edge, stop ACK, settled samples, timed OFF, faults, optional request_id");return 0;
}
