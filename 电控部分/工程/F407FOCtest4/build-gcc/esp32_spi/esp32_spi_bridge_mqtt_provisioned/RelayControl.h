#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// 包含本机联调和本次触碰后的定时奶泵输出，不实现完整两笼FEED/清洗/机械回零。
// GPIO 是板上丝印号，不是排针的物理序号。SPI 的 12/13/14/15 不改动。
namespace relay {
constexpr uint8_t MILK_PIN = 4;          // 奶泵继电器 IN：GPIO4。
constexpr uint8_t WATER_PIN = 5;         // 清水泵继电器 IN：GPIO5。
constexpr uint8_t DEFAULT_MILK_ACTIVE=1; // 用户确认高电平触发：HIGH 吸合，LOW 释放。
constexpr uint8_t DEFAULT_WATER_ACTIVE=1;// 清水泵也按同类高电平模块；已保存极性可覆盖默认值。
constexpr uint32_t MAX_TEST_MS = 2000;   // 单次测试最长 2000 ms；不是正式供奶时间。
constexpr uint32_t MAX_FEED_MS = 30000;  // 阶段开泵的硬上限；不代表已确定的喂奶量。

// 一次保存完整记录，触发电平未知时不猜测。0=低电平吸合，1=高电平吸合。
struct Settings { uint8_t version=1, milkActive=0, waterActive=0, checksum=0; };
inline void seal(Settings& s) { s.checksum=static_cast<uint8_t>(0xa7U^s.version^s.milkActive^s.waterActive); }
inline bool valid(const Settings& s) {
  return s.version==1 && s.milkActive<=1 && s.waterActive<=1 &&
    s.checksum==static_cast<uint8_t>(0xa7U^s.version^s.milkActive^s.waterActive);
}
inline Settings defaultSettings() {
  Settings s;s.milkActive=DEFAULT_MILK_ACTIVE;s.waterActive=DEFAULT_WATER_ACTIVE;seal(s);return s;
}
inline bool number(const char* text,uint32_t maximum,uint32_t& out) {
  if(!text || !*text) return false;
  uint32_t n=0;
  for(;*text;++text) {
    if(*text<'0'||*text>'9') return false;
    const uint32_t digit=static_cast<uint32_t>(*text-'0');
    if(n>maximum/10 || (n==maximum/10 && digit>maximum%10)) return false;
    n=n*10+digit;
  }
  if(!n) return false;
  out=n;return true;
}
enum class Kind { Invalid, Status, Off, Configure, Pulse };
struct Command { Kind kind=Kind::Invalid; Settings settings{}; int channel=-1; uint32_t duration=0; };
inline bool level(const char* text,uint8_t& out) {
  if(!strcmp(text,"LOW")){out=0;return true;}
  if(!strcmp(text,"HIGH")){out=1;return true;}
  return false;
}
// 串口输入已由网关转成大写；严格限定词数和整数，拒绝无限时 ON。
inline Command parse(const char* line) {
  Command out{};
  if(!line || strlen(line)>=64) return out;
  char copy[64];strcpy(copy,line);
  char* words[5]={};size_t count=0;
  char* p=copy;
  while(*p) {
    while(*p==' ') ++p;
    if(!*p) break;
    if(count==5) return out;
    words[count++]=p;
    while(*p&&*p!=' ') ++p;
    if(*p) *p++=0;
  }
  if(count<2 || strcmp(words[0],"RELAY")) return out;
  if(count==2&&!strcmp(words[1],"STATUS"))out.kind=Kind::Status;
  else if(count==2&&!strcmp(words[1],"OFF"))out.kind=Kind::Off;
  else if((count==3||count==4)&&!strcmp(words[1],"CONFIG")) {
    if(!level(words[2],out.settings.milkActive))return Command{};
    out.settings.waterActive=out.settings.milkActive;
    if(count==4&&!level(words[3],out.settings.waterActive))return Command{};
    seal(out.settings);out.kind=Kind::Configure;
  } else if(count==3&&(!strcmp(words[1],"MILK")||!strcmp(words[1],"WATER"))) {
    if(!number(words[2],MAX_TEST_MS,out.duration))return Command{};
    out.channel=!strcmp(words[1],"MILK")?0:1;out.kind=Kind::Pulse;
  }
  return out;
}
struct Controller {
  Settings settings{};
  bool configured=false,linkBound=false,feedPulse=false;
  int active=-1;
  uint32_t startedAt=0,duration=0;
  bool configure(const Settings& next) {
    if(!valid(next))return false;
    stop();settings=next;configured=true;return true;
  }
  bool running() const { return active>=0; }
  // 只描述请求输出，不把它当作触点、电流或流量反馈。
  int output(unsigned channel) const {
    if(!configured||channel>1)return -1;
    const int on=channel==0?settings.milkActive:settings.waterActive;
    return active==static_cast<int>(channel)?on:1-on;
  }
  const char* start(int channel,uint32_t ms,uint32_t now,bool bindToLink) {
    if(!configured)return "rejected_relay_unconfigured";
    if(channel<0||channel>1||!ms||ms>MAX_TEST_MS)return "rejected_relay_duration";
    if(running())return "rejected_relay_busy";
    active=channel;duration=ms;startedAt=now;linkBound=bindToLink;feedPulse=false;
    return "accepted_relay_pulse";
  }
  // 仅由通过本次触碰/停稳检查的业务状态机调用，不开放裸开泵网络指令。
  const char* startMilk(uint32_t ms,uint32_t maximum,uint32_t now) {
    if(!configured)return "rejected_relay_unconfigured";
    if(!ms||!maximum||maximum>MAX_FEED_MS||ms>maximum)return "rejected_relay_duration";
    if(running())return "rejected_relay_busy";
    active=0;duration=ms;startedAt=now;linkBound=true;feedPulse=true;
    return "accepted_milk_stage";
  }
  void stop() { active=-1;duration=0;linkBound=false;feedPulse=false; }
  const char* tick(uint32_t now,bool linkHealthy) {
    if(!running())return nullptr;
    if(linkBound&&!linkHealthy){stop();return "foc_link_lost";}
    if(static_cast<uint32_t>(now-startedAt)>=duration){bool wasFeed=feedPulse;stop();return wasFeed?"milk_timer_expired":"test_timer_expired";}
    return nullptr;
  }
};
} // relay 命名空间

namespace diagnostic {
// 连续探测只发送 PING；不 ARM、不发送速度，也不自动启动继电器。
struct ProbeBatch {
  bool active=false,awaiting=false,completed=false;
  uint32_t target=0,passed=0,failed=0,id=0;
  bool begin(uint32_t count) {
    if(active||!count||count>50)return false;
    target=count;passed=failed=id=0;active=true;awaiting=completed=false;return true;
  }
  bool due() const {return active&&!awaiting;}
  void sent(uint32_t commandId){id=commandId;awaiting=true;}
  bool result(uint32_t commandId,bool pong) {
    if(!active||!awaiting||commandId!=id)return false;
    awaiting=false;if(pong)++passed;else ++failed;
    if(passed+failed>=target){active=false;completed=true;}
    return true;
  }
  void cancel(){active=awaiting=completed=false;}
};
inline bool parseCount(const char* cmd,uint32_t& count) {
  if(!strcmp(cmd,"SPITEST")){count=10;return true;}
  return !strncmp(cmd,"SPITEST ",8)&&relay::number(cmd+8,50,count);
}
} // diagnostic 命名空间
