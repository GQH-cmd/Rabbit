#include <cstdio>
#include <cstring>
#include "FeedContactStage.h"
static int checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("FAIL %d: %s\n",__LINE__,#x);}}while(0)
template<class T> auto single(T& cfg,int)->decltype(cfg.singleM1=true,void()){cfg.singleM1=true;}
template<class T> void single(T&,long){} // 旧实现仍编译，必须以行为失败而不是缺API作为RED。
static feed::Settings settings(){feed::Settings cfg;cfg.milkMs=3000;cfg.downSign=1;single(cfg,0);return cfg;}
static FlpStatus frame(unsigned n,int rpm=0,bool contact=false){
  FlpStatus s{};s.flags=FLP_READY|FLP_ENCODER1|0x80;s.sample=n;s.uptime=n*100;
  s.rpm1=rpm;s.mode1=rpm?2:0;if(contact)s.flags|=FLP_CONTACT1;return s;
}
int main(){
  uint8_t raw[FLP_SIZE];auto s=frame(1);FlpStatus decoded{};flp_reply(raw,&s);
  CHECK(flp_read_reply(raw,&decoded));CHECK(!(decoded.flags&FLP_ENCODER0));
  auto cfg=settings();feed::Controller c;
  CHECK(!std::strcmp(c.begin(cfg,s,100,0),"accepted_feed_wait_motion"));
  CHECK(c.tick(frame(2,10000),200,0,false)==feed::Action::None);
  CHECK(c.tick(frame(3,10000),300,0,false)==feed::Action::None);CHECK(c.state==feed::State::WaitContact);
  CHECK(c.tick(frame(4,0,true),400,0,false)==feed::Action::SendStop);CHECK(c.bindStop(99,400));
  s=frame(5,0,true);s.flags|=FLP_ACK;s.id=99;s.result=FLP_ACCEPTED;
  CHECK(c.tick(s,500,0,false)==feed::Action::None);
  CHECK(c.tick(frame(6,0,true),600,0,false)==feed::Action::None);
  CHECK(c.tick(frame(7,0,true),700,0,false)==feed::Action::StartMilk);CHECK(c.pumpStarted(700));
  CHECK(c.tick(frame(8,0,true),3699,0,true)==feed::Action::None);
  CHECK(c.tick(frame(9,0,true),3700,0,false)==feed::Action::StopMilk);
  auto rejected=[&](FlpStatus bad){feed::Controller d;CHECK(std::strncmp(d.begin(cfg,bad,100,0),"accepted",8)!=0);};
  s=frame(1);s.flags&=~0x80;rejected(s); // 旧双轴从机即使M0故障，也不能冒充单轴固件。
  s=frame(1);s.flags|=FLP_ENCODER0;rejected(s);
  s=frame(1);s.flags&=~FLP_ENCODER1;rejected(s);
  s=frame(1);s.faults=FLP_FAULT_TRANSPORT;rejected(s);
  s=frame(1);s.rpm0=100;rejected(s);
  s=frame(1);s.mode0=2;rejected(s);
  s=frame(1);s.iq0=1;flp_reply(raw,&s);CHECK(!flp_read_reply(raw,&decoded));
  c=feed::Controller{};CHECK(!std::strcmp(c.begin(cfg,frame(1),100,0),"accepted_feed_wait_motion"));
  s=frame(2);s.flags&=~FLP_ENCODER1;CHECK(c.tick(s,200,0,false)==feed::Action::Abort);
  c=feed::Controller{};c.begin(cfg,frame(1),100,0);CHECK(c.tick(frame(2),200,501,false)==feed::Action::Abort);
  c=feed::Controller{};c.begin(cfg,frame(1),100,0);CHECK(c.tick(frame(2,-10000),200,0,false)==feed::Action::Abort);
  std::printf("M1-only feed: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
