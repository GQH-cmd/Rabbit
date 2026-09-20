#ifndef FOC_LINK_PROTOCOL_H
#define FOC_LINK_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

/* 双向协议 v2：固定 64 字节、小端；不发送结构体内存，避免两端填充差异。
 * 本文件是协议唯一源，构建前同步到 ESP32 草图目录。 */
#define FLP_SIZE 64
#define FLP_VERSION 2
#define FLP_COMMAND 1
#define FLP_POLL 2
#define FLP_READY 1
#define FLP_ACK 2
#define FLP_ENCODER0 4
#define FLP_ENCODER1 8
#define FLP_CONTACT0 16
#define FLP_CONTACT1 32
#define FLP_SPI_OWNER 64
#define FLP_ACCEPTED 1
#define FLP_PONG 2
#define FLP_BAD_COMMAND 3
#define FLP_FAULT_TRANSPORT 1
#define FLP_FAULT_FRAME 2
#define FLP_FAULT_WATCHDOG 4
#define FLP_FAULT_VALUE 8
#define FLP_WATCHDOG_MS 2000U
typedef struct { uint32_t id; uint8_t kind,keepalive; char text[29]; } FlpRequest;
typedef struct {
  uint32_t id,sample,uptime,badFrames;
  int32_t rpm0,rpm1,iq0,iq1;
  uint16_t faults;
  uint8_t flags,result,mode0,mode1;
} FlpStatus;
typedef struct { char mode; float value; } FlpMotor;
static inline uint16_t flp_crc16(const uint8_t* p,size_t n) {
  uint16_t crc=0xffff;
  while(n--) { crc^=(uint16_t)*p++<<8; for(int i=0;i<8;++i) crc=(uint16_t)((crc&0x8000)?(crc<<1)^0x1021:crc<<1); }
  return crc;
}
static inline void flp_put32(uint8_t* p,uint32_t v) { for(int i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i)); }
static inline uint32_t flp_get32(const uint8_t* p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static inline void flp_finish(uint8_t* p) { uint16_t c=flp_crc16(p,62);p[62]=(uint8_t)c;p[63]=(uint8_t)(c>>8); }
static inline int flp_valid(const uint8_t* p,uint8_t magic) { return p[0]==magic&&p[1]==FLP_VERSION&&flp_crc16(p,62)==((uint16_t)p[62]|((uint16_t)p[63]<<8)); }
static inline int flp_request(uint8_t* p,uint8_t kind,uint32_t id,const char* text,uint8_t keepalive) {
  size_t n=text?strlen(text):0;
  memset(p,0,FLP_SIZE);
  if(kind==FLP_COMMAND) { if(!id||!n||n>28)return 0; }
  else if(kind!=FLP_POLL||id||n||keepalive>1)return 0;
  p[0]=0xa6;p[1]=FLP_VERSION;p[2]=kind;p[3]=(uint8_t)(kind==FLP_COMMAND?n:1);
  flp_put32(p+4,id);
  if(kind==FLP_COMMAND)memcpy(p+8,text,n);else p[8]=keepalive;
  flp_finish(p);return 1;
}
static inline int flp_read_request(const uint8_t* p,FlpRequest* q) {
  if(!flp_valid(p,0xa6))return 0;
  memset(q,0,sizeof(*q));q->kind=p[2];q->id=flp_get32(p+4);
  if(q->kind==FLP_COMMAND) {
    if(!q->id||!p[3]||p[3]>28)return 0;
    for(unsigned i=0;i<p[3];++i)if(p[8+i]<32||p[8+i]>126)return 0;
    memcpy(q->text,p+8,p[3]);
  } else if(q->kind==FLP_POLL&&p[3]==1&&!q->id&&p[8]<=1)q->keepalive=p[8];
  else return 0;
  for(unsigned i=8+p[3];i<62;++i)if(p[i])return 0;
  return 1;
}
static inline void flp_reply(uint8_t* p,const FlpStatus* s) {
  memset(p,0,FLP_SIZE);p[0]=0x5a;p[1]=FLP_VERSION;p[2]=s->flags;p[3]=s->result;
  flp_put32(p+4,s->id);flp_put32(p+8,s->sample);flp_put32(p+12,s->uptime);
  flp_put32(p+16,(uint32_t)s->rpm0);flp_put32(p+20,(uint32_t)s->rpm1);
  flp_put32(p+24,(uint32_t)s->iq0);flp_put32(p+28,(uint32_t)s->iq1);
  p[32]=s->mode0;p[33]=s->mode1;p[34]=(uint8_t)s->faults;p[35]=(uint8_t)(s->faults>>8);
  flp_put32(p+36,s->badFrames);flp_finish(p);
}
static inline int flp_read_reply(const uint8_t* p,FlpStatus* s) {
  if(!flp_valid(p,0x5a)||!(p[2]&FLP_READY)||(p[2]&0x80)||p[3]>FLP_BAD_COMMAND||p[32]>3||p[33]>3)return 0;
  if((p[2]&FLP_ACK)&&(!flp_get32(p+4)||!p[3]))return 0;
  for(int i=40;i<62;++i)if(p[i])return 0;
  s->flags=p[2];s->result=p[3];s->id=flp_get32(p+4);s->sample=flp_get32(p+8);s->uptime=flp_get32(p+12);
  s->rpm0=(int32_t)flp_get32(p+16);s->rpm1=(int32_t)flp_get32(p+20);
  s->iq0=(int32_t)flp_get32(p+24);s->iq1=(int32_t)flp_get32(p+28);
  s->mode0=p[32];s->mode1=p[33];s->faults=(uint16_t)(p[34]|((uint16_t)p[35]<<8));s->badFrames=flp_get32(p+36);return 1;
}
static inline int flp_digit(char c) { return c>='0'&&c<='9'; }
static inline int flp_motor(const char* t,size_t n,FlpMotor* m) {
  if(n<2||n>48||!strchr("ocvp",t[0])||!t[0])return 0;
  size_t i=1,d=0;
  if(t[i]=='+'||t[i]=='-')++i;
  while(i<n&&flp_digit(t[i])) {++i;++d;}
  if(i<n&&t[i]=='.') {++i;while(i<n&&flp_digit(t[i])) {++i;++d;}}
  if(!d)return 0;
  if(i<n&&(t[i]=='e'||t[i]=='E')) {
    ++i;if(i<n&&(t[i]=='+'||t[i]=='-'))++i;
    size_t start=i;while(i<n&&flp_digit(t[i]))++i;if(i==start)return 0;
  }
  if(i!=n)return 0;
  char number[49]={0};memcpy(number,t+1,n-1);char* end=0;errno=0;
  float value=strtof(number,&end);
  if(errno==ERANGE||!isfinite(value)||end!=number+n-1)return 0;
  m->mode=t[0];m->value=value;return 1;
}
/* 串口最多 49 字符，SPI 最多 28；两电机均校验成功后调用者才能修改目标。 */
static inline int flp_motor_pair(const char* t,size_t n,FlpMotor* a,FlpMotor* b) {
  if(!t||n<5||n>49)return 0;
  const char* comma=(const char*)memchr(t,',',n);
  if(!comma)return 0;
  size_t split=(size_t)(comma-t);
  return flp_motor(t,split,a)&&flp_motor(comma+1,n-split-1,b);
}
#endif
