#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "FocSpiProtocol.h"

namespace gateway {
// 配置作为一个带版本和校验的完整记录提交，避免逐字段写入造成新旧参数混合。
struct Config {
  uint32_t version = 1;
  char ssid[33] = {};
  char wifiPassword[64] = {};
  char host[128] = {};
  char username[65] = {};
  char password[129] = {};
  char device[40] = {};
  uint16_t port = 1883;
  uint32_t checksum = 0;
};
inline uint32_t configHash(const Config& c) {
  uint32_t h=2166136261U;
  const auto* b=reinterpret_cast<const uint8_t*>(&c);
  for(size_t i=0;i<offsetof(Config,checksum);++i) h=(h^b[i])*16777619U;
  return h;
}
inline void seal(Config& c) { c.checksum=configHash(c); }
inline bool identifier(const char* s) {
  if(!s || !*s) return false;
  for(;*s;++s) if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_')) return false;
  return true;
}
inline bool validHost(const char* s) {
  if(!s || !*s) return false;
  for(;*s;++s) if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='.')) return false;
  return true;
}
template<size_t N> inline bool terminated(const char (&s)[N]) { return memchr(s,0,N)!=nullptr; }
inline bool validConfig(const Config& c) {
  if(c.version!=1 || c.checksum!=configHash(c) || c.port==0) return false;
  if(!terminated(c.ssid)||!terminated(c.wifiPassword)||!terminated(c.host)||!terminated(c.username)||!terminated(c.password)||!terminated(c.device)) return false;
  const size_t n=strlen(c.wifiPassword);
  return c.ssid[0] && (n==0 || (n>=8 && n<=63)) && validHost(c.host) && identifier(c.device);
}
inline bool parsePort(const char* s,uint16_t& port) {
  if(!s || !*s) return false;
  uint32_t v=0;
  for(;*s;++s) { if(*s<'0'||*s>'9') return false; v=v*10+(*s-'0'); if(v>65535) return false; }
  if(!v) return false;
  port=static_cast<uint16_t>(v); return true;
}
struct Command { char text[29]={}; char request[48]={}; char session[33]={}; };
inline bool space(char c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }
// 只接受三个已约定的字符串字段；拒绝重复键、截断、尾随内容和转义，绝不从坏消息中提取命令。
inline bool parseCommand(const char* data,size_t length,Command& out) {
  out=Command{};
  if(!data||!length||length>256||memchr(data,0,length)) return false;
  size_t i=0; unsigned seen=0;
  auto ws=[&](){while(i<length&&space(data[i])) ++i;};
  auto str=[&](char* dst,size_t cap)->bool {
    if(i>=length||data[i++]!='"') return false;
    size_t n=0;
    while(i<length&&data[i]!='"') {
      const unsigned char c=data[i++];
      if(c<32||c>126||c=='\\'||n+1>=cap) return false;
      dst[n++]=static_cast<char>(c);
    }
    if(i>=length) return false;
    ++i; dst[n]=0; return true;
  };
  ws(); if(i>=length||data[i++]!='{') return false; ws();
  for(;;) {
    char key[16]={}; if(!str(key,sizeof(key))) return false; ws();
    if(i>=length||data[i++]!=':') return false;
    ws();
    char* dst=nullptr; size_t cap=0; unsigned bit=0;
    if(!strcmp(key,"command")){dst=out.text;cap=sizeof(out.text);bit=1;}
    else if(!strcmp(key,"request_id")){dst=out.request;cap=sizeof(out.request);bit=2;}
    else if(!strcmp(key,"session_id")){dst=out.session;cap=sizeof(out.session);bit=4;}
    else return false;
    if((seen&bit)||!str(dst,cap)||!dst[0]) return false;
    seen|=bit; ws(); if(i>=length) return false;
    const char next=data[i++];
    if(next=='}') break;
    if(next!=',') return false;
    ws();
  }
  ws(); return i==length && (seen&1) && (!(seen&2)||identifier(out.request)) && (!(seen&4)||identifier(out.session));
}
inline bool isStop(const char* s) { return !strcmp(s,"STOP")||!strcmp(s,"DISARM")||!strcmp(s,"o0,o0"); }
inline bool remoteAllowed(const char* s) {
  bool zero=false;
  return isStop(s)||!strcmp(s,"ARM")||!strcmp(s,"PING")||!strcmp(s,"KEEPALIVE")||foc::validMotorCommand(s,zero);
}
// 网络控制采用五秒租约；断线、换会话或心跳到期都需要重新解锁，不自动恢复运行。
struct Lease {
  bool armed=false, remote=false; uint32_t epoch=0,last=0;
  void arm(bool fromNetwork,uint32_t connection,uint32_t now){armed=true;remote=fromNetwork;epoch=connection;last=now;}
  void renew(uint32_t now){last=now;}
  void lock(){armed=false;remote=false;}
  bool expired(uint32_t now,bool online,uint32_t connection) const {
    return armed&&remote&&(!online||connection!=epoch||static_cast<uint32_t>(now-last)>=5000);
  }
};
} // gateway 命名空间
