#include <cstdio>
#include <cstring>
#if __has_include("FocLinkProtocol.h") && __has_include("FocLinkClient.h")
#include "FocLinkProtocol.h"
#include "FocLinkClient.h"
static int count=0;
#define CHECK(x) do{++count;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
  CHECK(flp_crc16((const uint8_t*)"123456789",9)==0x29B1);
  uint8_t frame[FLP_SIZE]; FlpRequest q={};
  CHECK(flp_request(frame,FLP_COMMAND,0x89abcdef,"v30,o0",0));
  CHECK(flp_read_request(frame,&q)&&q.id==0x89abcdef&&!strcmp(q.text,"v30,o0"));
  CHECK(!flp_request(frame,FLP_COMMAND,0,"PING",0));
  CHECK(!flp_request(frame,FLP_COMMAND,1,"12345678901234567890123456789",0));
  CHECK(flp_request(frame,FLP_POLL,0,"",1));
  CHECK(flp_read_request(frame,&q)&&q.kind==FLP_POLL&&q.keepalive==1);
  FlpStatus s={}; s.flags=FLP_READY|FLP_ACK|FLP_ENCODER0;s.id=123;s.result=FLP_ACCEPTED;
  s.sample=15;s.uptime=1000;s.rpm0=-1250;s.iq0=-120;s.rpm1=500;s.mode0=2;
  flp_reply(frame,&s);FlpStatus out={};
  CHECK(flp_read_reply(frame,&out)&&out.id==123&&out.rpm0==-1250&&out.iq0==-120&&out.sample==15);
  for(int i=0;i<FLP_SIZE;++i){frame[i]^=1;CHECK(!flp_read_reply(frame,&out));frame[i]^=1;}
  frame[1]=99;flp_finish(frame);CHECK(!flp_read_reply(frame,&out));
  memset(frame,0xff,sizeof(frame));CHECK(!flp_read_reply(frame,&out));
  memset(frame,0,sizeof(frame));CHECK(!flp_read_reply(frame,&out));
  FlpMotor a,b;
  CHECK(flp_motor_pair("v-30.5,o0",9,&a,&b)&&a.mode=='v'&&a.value==-30.5f);
  const char* bad[]={"v30,0","vnan,o0","v1garbage,o0","v1,o0,o0","v1e999,o0","v1e,o0","q1,o0","v1,",",o0"};
  for(auto t:bad)CHECK(!flp_motor_pair(t,strlen(t),&a,&b));
  foclink::Client client;
  CHECK(!client.online(0));
  s.flags=FLP_READY;s.sample=1;flp_reply(frame,&s);
  CHECK(client.observe(frame,100));CHECK(client.online(100));
  CHECK(client.begin(42,110));CHECK(!client.begin(43,110));
  s.flags|=FLP_ACK;s.id=41;s.sample=2;flp_reply(frame,&s);CHECK(client.observe(frame,200));
  CHECK(client.pending); // 旧回执不能完成新命令。
  s.id=42;s.sample=3;flp_reply(frame,&s);CHECK(client.observe(frame,300));
  CHECK(!client.pending&&client.takeResult()==FLP_ACCEPTED);
  CHECK(client.takeResult()==-1);
  CHECK(client.begin(43,400));
  CHECK(!client.observe(frame,500)); // 重复快照不刷新在线状态或产生回执。
  CHECK(!client.online(1600));CHECK(client.timeout(2100));CHECK(!client.pending);
  CHECK(client.begin(44,0xffffff00));CHECK(!client.timeout(0x10));CHECK(client.timeout(0x700));
  s.sample=1;s.uptime=0;s.id=42;flp_reply(frame,&s);CHECK(client.observe(frame,2200));
  CHECK(client.takeResult()==-1); // FOC 重启也不能接受旧 ID。
  printf("PASS %d duplex checks\n",count);
}
#else
int main(){puts("FAIL: bidirectional protocol and ACK tracker are not implemented");return 1;}
#endif
