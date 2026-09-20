#include <atomic>
#include <cstdio>
#include "Arduino.h"
#include "SPI.h"
#include "GatewayValidation.h"
#include "FocLinkClient.h"
constexpr uint8_t CS=15;
SPIClass spi(FSPI);
gateway::Lease lease{};
bool spiReady=true,sentFrame=false,polling=false,linkVerified=false,linkReset=false;
uint32_t txCount=0,lastFrame=0,nextId=0;
foclink::Client link;
struct Pending {char request[48];uint32_t epoch;bool remote;} pending{};
struct Ack {char request[48];char result[40];uint32_t epoch,tx;bool armed;};
std::vector<Ack> captured;
int acks=1,commands=2,resets=0;
constexpr int pdTRUE=1;
int xQueueSend(int,const Ack* a,int){captured.push_back(*a);return pdTRUE;}
void xQueueReset(int){++resets;}
std::atomic<bool> portal{false},netOnline{false};
std::atomic<uint32_t> netEpoch{0};
// 直接提取生产函数，覆盖发帧、回执关联、锁定和超时。
#include "gateway_runtime_extracted.h"
static int count=0;
#define CHECK(x) do {++count;if(!(x)){std::printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static FlpStatus status{};
static void reply(uint32_t id,uint8_t result){
  uint8_t rx[FLP_SIZE];status.flags=FLP_READY|FLP_ACK;status.id=id;status.result=result;
  ++status.sample;status.uptime=millis();flp_reply(rx,&status);
  fake::replies.emplace_back(rx,rx+FLP_SIZE);
}
static void pollReply(uint32_t id,uint8_t result){fake::now+=101;reply(id,result);serviceLink();}
int main(){
  CHECK(fake::frames.empty());serviceLink();CHECK(fake::frames.empty());
  CHECK(!strcmp(execute("v30,o0"),"rejected_not_armed"));
  CHECK(!strcmp(execute("ARM"),"rejected_foc_unverified"));
  CHECK(!strcmp(execute("PING"),"spi_pending"));CHECK(link.pending&&!linkVerified);
  CHECK(fake::frames.back().size()==64&&fake::frames.back()[0]==0xa6);
  CHECK(!strcmp(execute("PING"),"rejected_spi_busy"));
  auto id=link.commandId;pollReply(id+1,FLP_PONG);CHECK(link.pending&&!linkVerified);
  pollReply(id,FLP_PONG);CHECK(!link.pending&&linkVerified);
  CHECK(!strcmp(execute("FACTORY_RESET",true,1),"rejected_stale_session"));
  netOnline=true;netEpoch=1;
  CHECK(!strcmp(execute("ARM",true,1),"accepted_local"));
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
  stopAndLock("test");spiReady=false;CHECK(!strcmp(execute("ARM"),"rejected_spi"));
  printf("PASS %d gateway runtime checks\n",count);
}
