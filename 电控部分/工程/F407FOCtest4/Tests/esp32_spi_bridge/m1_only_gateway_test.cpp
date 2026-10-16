#define FOC_M1_ONLY 1
#define main dual_suite_not_run
#include "gateway_runtime_test.cpp"
#undef main
int main(){
  CHECK(feedRig()==0);CHECK(!strcmp(execute("ARM"),"accepted_local"));
  CHECK(!strcmp(execute("v100,v100"),"rejected_m0_disabled"));
  CHECK(!strcmp(execute("v0,v100"),"rejected_m0_disabled"));
  CHECK(!strcmp(execute("o0,v100"),"spi_pending"));
  CHECK(feedToMilk(false,1)==0);feedFrame(0,true);CHECK(relays.active==0);
  fake::now+=100;auto bad=link.status;bad.sample++;bad.uptime=fake::now;bad.flags&=~FLP_ENCODER1;
  queueFeedSnapshot(bad);serviceLink();serviceFeed();CHECK(!relays.running()&&!feedStage.running());
  CHECK(feedRig()==0);auto old=link.status;old.flags=FLP_READY|FLP_ENCODER0|FLP_ENCODER1;
  fake::now+=100;old.sample++;old.uptime=fake::now;queueFeedSnapshot(old);serviceLink();
  CHECK(!strncmp(execute("FEED"),"rejected",8));
  CHECK(feedToMilk(false,1)==0);fake::now+=3001;serviceRelays();serviceFeed();CHECK(!relays.running());
  CHECK(feedRig()==0);
  CHECK(!strcmp(execute("RECOVER",true),"rejected_local_only"));
  lease.arm(false,0,fake::now);CHECK(!strcmp(execute("RECOVER"),"rejected_stop_first"));lease.lock();
  link.status.flags&=~FLP_ENCODER1;portal=true;fake::now+=100;
  CHECK(!strcmp(execute("RECOVER"),"spi_pending"));
  FlpRequest recovery{};CHECK(flp_read_request(fake::frames.back().data(),&recovery)&&!strcmp(recovery.text,"RECOVER"));
  CHECK(!lease.armed&&!relays.running()&&!feedStage.running());
  CHECK(!strcmp(execute("RECOVER"),"rejected_spi_busy"));
  std::printf("M1-only gateway: %d checks\n",count);return 0;
}
