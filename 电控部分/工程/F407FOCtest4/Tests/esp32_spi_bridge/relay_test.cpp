#include <cstdio>
#include <cstring>
#include "RelayControl.h"
#include "GatewayValidation.h"
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
  auto defaults=relay::defaultSettings();
  CHECK(relay::valid(defaults)&&defaults.milkActive==1&&defaults.waterActive==1);
  relay::Controller c;
  CHECK(!c.configured&&!c.running()&&c.output(0)==-1);
  CHECK(!strcmp(c.start(0,500,0,false),"rejected_relay_unconfigured"));
  const char* invalid[]={"RELAY","RELAY ON","RELAY MILK","RELAY MILK -1","RELAY MILK 0",
    "RELAY MILK 2001","RELAY MILK 99999999999999999999","RELAY MILK 12.5",
    "RELAY MILK 100 junk","RELAY CONFIG UNKNOWN","RELAY CONFIG LOW UNKNOWN",
    "RELAY CONFIG LOW HIGH EXTRA","RELAY STATUS EXTRA","RELAY OFF EXTRA","RELAY\tOFF"};
  for(auto s:invalid)CHECK(relay::parse(s).kind==relay::Kind::Invalid);
  auto a=relay::parse("RELAY CONFIG LOW HIGH");CHECK(a.kind==relay::Kind::Configure);
  CHECK(a.settings.milkActive==0&&a.settings.waterActive==1&&relay::valid(a.settings));
  CHECK(c.configure(a.settings));CHECK(c.output(0)==1&&c.output(1)==0);
  CHECK(!strcmp(c.start(0,500,100,true),"accepted_relay_pulse"));
  CHECK(c.output(0)==0&&c.output(1)==0);
  CHECK(!strcmp(c.start(1,500,150,true),"rejected_relay_busy"));
  CHECK(c.startedAt==100&&c.duration==500);
  CHECK(c.tick(599,true)==nullptr&&c.running());
  CHECK(!strcmp(c.tick(600,true),"test_timer_expired")&&!c.running());
  CHECK(!strcmp(c.start(1,2000,1000,true),"accepted_relay_pulse"));
  CHECK(c.output(0)==1&&c.output(1)==1);
  CHECK(!strcmp(c.tick(1001,false),"foc_link_lost")&&!c.running());
  CHECK(!strcmp(c.start(0,2000,0xffffff00U,false),"accepted_relay_pulse"));
  CHECK(c.tick(0x000006cfU,false)==nullptr);
  CHECK(!strcmp(c.tick(0x000006d0U,false),"test_timer_expired"));
  auto high=relay::parse("RELAY CONFIG HIGH");CHECK(c.configure(high.settings));
  CHECK(c.output(0)==0&&c.output(1)==0);
  auto bad=high.settings;++bad.version;CHECK(!c.configure(bad));
  CHECK(c.settings.version==1);
  auto one=relay::parse("  RELAY   WATER  1 ");CHECK(one.kind==relay::Kind::Pulse&&one.duration==1&&one.channel==1);
  CHECK(relay::parse("RELAY OFF").kind==relay::Kind::Off);
  CHECK(relay::parse("RELAY STATUS").kind==relay::Kind::Status);
  CHECK(!gateway::remoteAllowed("RELAY MILK 500"));
  CHECK(!gateway::remoteAllowed("RELAY CONFIG LOW"));
  CHECK(!gateway::remoteAllowed("SPITEST 10"));
  diagnostic::ProbeBatch probe;uint32_t n=0;
  CHECK(diagnostic::parseCount("SPITEST",n)&&n==10);
  CHECK(diagnostic::parseCount("SPITEST 50",n)&&n==50);
  CHECK(!diagnostic::parseCount("SPITEST 0",n));
  CHECK(!diagnostic::parseCount("SPITEST 51",n));
  CHECK(!diagnostic::parseCount("SPITEST 10 extra",n));
  CHECK(probe.begin(2)&&probe.due());CHECK(!probe.begin(1));
  probe.sent(100);CHECK(!probe.due());CHECK(!probe.result(101,true));
  CHECK(probe.result(100,true)&&probe.passed==1&&probe.due());
  CHECK(!probe.result(100,true)&&probe.passed==1);
  probe.sent(101);CHECK(probe.result(101,false));
  CHECK(probe.completed&&!probe.active&&probe.passed==1&&probe.failed==1);
  CHECK(probe.begin(3));probe.sent(50);probe.cancel();CHECK(!probe.active&&!probe.awaiting);
  CHECK(!probe.result(50,true));
  std::printf("PASS %d relay and probe checks\n",checks);
}
