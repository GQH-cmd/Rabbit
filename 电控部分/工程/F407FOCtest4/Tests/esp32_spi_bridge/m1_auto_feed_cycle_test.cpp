#define FOC_M1_ONLY 1
#define main legacy_gateway_suite_not_run
#include "gateway_runtime_test.cpp"
#undef main
#include "gateway_feed_busy_extracted.h"

// Run real gateway/state-machine/relay code. Only time, SPI and GPIO are simulated.
// The fallback lets the old implementation compile and fail on missing behavior.
template<class T> auto enableCycle(T& c,int)->decltype(c.autoCycle=true,void()){c.autoCycle=true;}
template<class T> void enableCycle(T&,long){}
template<class T> auto cycleEnabled(const T& c,int)->decltype(c.autoCycle){return c.autoCycle;}
template<class T> bool cycleEnabled(const T&,long){return false;}
static bool phase(const char* value){return !strcmp(feed::name(feedStage.state),value);}
static unsigned commandCount(const char* value){
  unsigned n=0;
  for(const auto& bytes:fake::frames){FlpRequest q{};
    if(flp_read_request(bytes.data(),&q)&&q.kind==FLP_COMMAND&&!strcmp(q.text,value))++n;}
  return n;
}
static FlpRequest lastRequest(){FlpRequest q{};flp_read_request(fake::frames.back().data(),&q);return q;}
static int beginCycle(){
  CHECK(feedRig(true,1)==0);enableCycle(feedconfig::current,0);
  auto before=fake::frames.size();
  CHECK(!strncmp(execute("FEED"),"accepted",8));
  CHECK(fake::frames.size()==before+1); // RED: old FEED sends no motion command.
  auto q=lastRequest();CHECK(q.kind==FLP_COMMAND&&!strcmp(q.text,"o0,v100")&&q.id);
  CHECK(link.pending&&link.commandId==q.id&&phase("wait_down_ack"));
  CHECK(lease.armed&&!relays.running());
  CHECK(commandCount("o0,v100")==1&&commandCount("o0,v-100")==0);
  auto sent=fake::frames.size();
  CHECK(!strcmp(execute("FEED"),"rejected_feed_busy"));
  CHECK(!strcmp(execute("o0,v100"),"rejected_feed_busy"));
  CHECK(fake::frames.size()==sent);return 0;
}
static int reachDownContact(){
  CHECK(beginCycle()==0);auto id=link.commandId;
  feedFrame(10000,false,id);CHECK(phase("wait_motion")&&!relays.running());
  feedFrame(10000,false);CHECK(phase("wait_contact"));return 0;
}
static int reachMilk(){
  CHECK(reachDownContact()==0);
  feedFrame(0,true);CHECK(phase("wait_stop_ack"));
  auto q=lastRequest();CHECK(!strcmp(q.text,"o0,o0")&&q.id==feedStage.stopId);
  feedFrame(0,true,q.id);CHECK(phase("confirm_stopped"));
  feedFrame(0,true);CHECK(!relays.running());
  feedFrame(0,true);CHECK(phase("milk_running")&&relays.active==0);
  CHECK(fake::gpioLevel(4)==HIGH&&fake::gpioLevel(5)==LOW);
  CHECK(commandCount("o0,v-100")==0);return 0;
}
static int reachReturn(){
  CHECK(reachMilk()==0);auto deadline=relays.startedAt+3000;
  while(fake::now+100<deadline)feedFrame(0,true);
  CHECK(relays.running());
  fake::now=deadline;feedReply(0,true);serviceLink();serviceRelays();serviceFeed();serviceRelays();
  CHECK(!relays.running()&&phase("wait_return_ack")&&lease.armed);
  auto q=lastRequest();CHECK(q.kind==FLP_COMMAND&&!strcmp(q.text,"o0,v-100"));
  CHECK(q.id==link.commandId&&link.pending);
  CHECK(fake::relayAtTransfer.back()==std::make_pair(LOW,LOW)); // OFF precedes return SPI.
  CHECK(commandCount("o0,v100")==1&&commandCount("o0,v-100")==1);
  CHECK(Serial.output.find("FEED_CONTACT_STAGE_COMPLETE")==std::string::npos);
  return 0;
}
static int completeCycle(){
  CHECK(reachReturn()==0);auto id=link.commandId;
  // The downward contact stays latched initially. It MUST NOT finish the return leg.
  feedFrame(0,true,id);CHECK(phase("wait_return_motion"));
  for(int i=0;i<5;++i){feedFrame(0,true);CHECK(phase("wait_return_motion")&&!relays.running());}
  feedFrame(-10000,false);CHECK(phase("wait_return_motion"));
  feedFrame(-10000,false);CHECK(phase("wait_return_contact"));
  auto sent=fake::frames.size();CHECK(!strcmp(execute("o0,v-100"),"rejected_feed_busy"));
  CHECK(fake::frames.size()==sent);
  feedFrame(0,true);CHECK(phase("wait_return_stop_ack"));
  auto q=lastRequest();CHECK(!strcmp(q.text,"o0,o0"));
  feedFrame(0,true,q.id);CHECK(phase("confirm_return_stopped"));
  feedFrame(0,true);CHECK(feedStage.running());
  feedFrame(0,true);CHECK(phase("cycle_complete")&&!feedStage.running());
  CHECK(!lease.armed&&!relays.running());CHECK(fake::gpioLevel(4)==LOW&&fake::gpioLevel(5)==LOW);
  CHECK(commandCount("o0,v100")==1&&commandCount("o0,v-100")==1&&commandCount("o0,o0")==2);
  auto motion=commandCount("o0,v100")+commandCount("o0,v-100");
  for(int i=0;i<5;++i)feedFrame(0,true);
  CHECK(commandCount("o0,v100")+commandCount("o0,v-100")==motion);
  // Start a fresh user-requested cycle without resetting either MCU.
  CHECK(!strncmp(execute("FEED"),"accepted",8));
  CHECK(phase("wait_down_ack")&&commandCount("o0,v100")==2&&!relays.running());
  return 0;
}
static int acknowledgements(){
  CHECK(beginCycle()==0);auto id=link.commandId;
  feedFrame(10000,false,id+42);CHECK(phase("wait_down_ack")&&link.pending);
  feedFrame(10000,false,id,FLP_BAD_COMMAND);
  CHECK(!feedStage.running()&&!lease.armed&&!relays.running());
  CHECK(commandCount("o0,v100")==1&&commandCount("o0,v-100")==0&&commandCount("o0,o0")==1);
  CHECK(beginCycle()==0);
  for(int i=0;i<16&&feedStage.running();++i)feedFrame(0,false);
  CHECK(!feedStage.running()&&!lease.armed&&!relays.running());
  CHECK(commandCount("o0,v100")==1&&commandCount("o0,v-100")==0);
  CHECK(reachReturn()==0);id=link.commandId;
  feedFrame(0,true,id+1);CHECK(phase("wait_return_ack"));
  feedFrame(0,true,id,FLP_BAD_COMMAND);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(commandCount("o0,v-100")==1);
  CHECK(reachReturn()==0);
  for(int i=0;i<16&&feedStage.running();++i)feedFrame(0,true);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(commandCount("o0,v-100")==1);return 0;
}
static int timeoutAndDirection(){
  CHECK(reachDownContact()==0);
  for(int i=0;i<151&&feedStage.running();++i)feedFrame(10000,false);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(!strcmp(feedStage.reason,"contact_wait_timeout"));CHECK(commandCount("o0,v-100")==0);
  CHECK(reachReturn()==0);feedFrame(0,true,link.commandId);
  feedFrame(10000,false);CHECK(!feedStage.running()&&!relays.running());
  CHECK(!strcmp(feedStage.reason,"wrong_return_direction"));
  CHECK(reachReturn()==0);feedFrame(0,true,link.commandId);
  for(int i=0;i<301&&feedStage.running();++i)feedFrame(0,true);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(!strcmp(feedStage.reason,"return_motion_timeout"));
  CHECK(reachReturn()==0);feedFrame(-10000,false,link.commandId);feedFrame(-10000,false);
  CHECK(phase("wait_return_contact"));
  for(int i=0;i<151&&feedStage.running();++i)feedFrame(-10000,false);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(!strcmp(feedStage.reason,"return_contact_timeout"));return 0;
}
static int stopAlwaysWins(){
  for(int step=0;step<5;++step){
    if(step==0){CHECK(beginCycle()==0);}
    else if(step==1){CHECK(reachDownContact()==0);}
    else if(step==2){CHECK(reachMilk()==0);fake::now=relays.startedAt+3000;}
    else {CHECK(reachReturn()==0);if(step==4){feedFrame(-10000,false,link.commandId);feedFrame(-10000,false);}}
    auto down=commandCount("o0,v100"),up=commandCount("o0,v-100");
    CHECK(!strcmp(execute("STOP"),"spi_pending"));
    CHECK(!feedStage.running()&&!lease.armed&&!relays.running());
    CHECK(fake::relayAtTransfer.back()==std::make_pair(LOW,LOW));
    feedFrame(0,true,link.commandId);for(int i=0;i<5;++i)feedFrame(0,true);
    CHECK(commandCount("o0,v100")==down&&commandCount("o0,v-100")==up);
  }return 0;
}
static int failedFeedback(){
  for(int part=0;part<3;++part){
    CHECK(reachReturn()==0);feedFrame(-10000,false,link.commandId);feedFrame(-10000,false);
    auto bad=link.status;bad.sample=++feedSample;fake::now+=100;bad.uptime=fake::now;
    if(part==0)bad.flags&=~FLP_ENCODER1;
    if(part==1)bad.faults=FLP_FAULT_TRANSPORT;
    if(part==2)bad.uptime=1;
    queueFeedSnapshot(bad);serviceLink();serviceFeed();serviceRelays();
    CHECK(!feedStage.running()&&!lease.armed&&!relays.running());
    CHECK(commandCount("o0,v-100")==1);
  }
  CHECK(reachReturn()==0);fake::now+=501;serviceFeed();
  CHECK(!feedStage.running()&&!relays.running()&&!strcmp(feedStage.reason,"feedback_stale"));
  CHECK(feedRig(true,1)==0);enableCycle(feedconfig::current,0);linkVerified=false;
  auto countBefore=fake::frames.size();CHECK(!strcmp(execute("FEED"),"rejected_foc_unverified"));
  CHECK(fake::frames.size()==countBefore&&!lease.armed&&!relays.running());
  CHECK(feedRig(true,1)==0);enableCycle(feedconfig::current,0);nextId=UINT32_MAX;
  CHECK(!strncmp(execute("FEED"),"accepted",8));CHECK(lastRequest().id==1&&link.commandId==1);
  return 0;
}
static int freshReturnAndFinalStop(){
  CHECK(reachReturn()==0);auto id=link.commandId;
  // Even correctly directed velocity cannot qualify while the old contact is still latched.
  feedFrame(-10000,true,id);feedFrame(-10000,true);
  CHECK(phase("wait_return_motion")&&!relays.running());
  feedFrame(-10000,false);CHECK(phase("wait_return_motion"));
  auto before=feedStage.movingCount;
  serviceFeed();serviceFeed();CHECK(feedStage.movingCount==before&&phase("wait_return_motion"));
  feedFrame(-10000,false);CHECK(phase("wait_return_contact"));
  feedFrame(0,true);auto stop=lastRequest();CHECK(phase("wait_return_stop_ack"));
  feedFrame(0,true,stop.id+10);CHECK(phase("wait_return_stop_ack"));
  feedFrame(0,true,stop.id);CHECK(phase("confirm_return_stopped"));
  before=feedStage.stoppedCount;
  serviceFeed();serviceFeed();CHECK(feedStage.stoppedCount==before&&feedStage.running());
  feedFrame(-9000,true);CHECK(feedStage.stoppedCount==0&&feedStage.running());
  feedFrame(0,true);feedFrame(0,true);CHECK(feedStage.running());
  feedFrame(0,true);CHECK(phase("cycle_complete")&&!lease.armed&&!relays.running());
  // A refused final STOP is not a completed return and never starts another pump pulse.
  CHECK(reachReturn()==0);feedFrame(-10000,false,link.commandId);feedFrame(-10000,false);
  feedFrame(0,true);stop=lastRequest();feedFrame(0,true,stop.id,FLP_BAD_COMMAND);
  CHECK(!feedStage.running()&&!relays.running()&&!lease.armed);
  CHECK(!strcmp(feedStage.reason,"stop_rejected"));
  // Full-cycle commands are tied to the one physically confirmed down-direction configuration.
  CHECK(feedRig(true,-1)==0);enableCycle(feedconfig::current,0);
  auto sent=fake::frames.size();CHECK(!strcmp(execute("FEED"),"rejected_feed_unconfigured"));
  CHECK(fake::frames.size()==sent&&!lease.armed&&!relays.running());
  return 0;
}
int main(){
  // Telemetry must agree with the interlock in every phase, including return.
  for(unsigned i=0;i<=static_cast<unsigned>(feed::State::Aborted);++i){
    feedStage.state=static_cast<feed::State>(i);
    CHECK(publishedFeedBusy(feedStage.state)==feedStage.running());
  }
  if(completeCycle()||acknowledgements()||timeoutAndDirection()||stopAlwaysWins()||failedFeedback()||freshReturnAndFinalStop())return 1;
  CHECK(cycleEnabled(compiledfeedconfig::settings(),0));
  std::printf("M1 automatic feed/return: %d checks; real gateway and controllers, simulated hardware only\n",count);
  return 0;
}
