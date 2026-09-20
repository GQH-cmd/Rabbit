#pragma once
#include "FocLinkProtocol.h"
namespace foclink {
struct Client {
  FlpStatus status{};
  bool seen=false,pending=false,restarted=false;
  uint32_t receivedAt=0,startedAt=0,commandId=0;
  int result=-1;
  bool online(uint32_t now) const {return seen&&(uint32_t)(now-receivedAt)<1000;}
  bool begin(uint32_t id,uint32_t now) {
    if(pending||!id)return false;
    commandId=id;startedAt=now;pending=true;result=-1;return true;
  }
  void cancel() {pending=false;result=-1;}
  bool observe(const uint8_t* frame,uint32_t now) {
    FlpStatus next{};restarted=false;
    if(!flp_read_reply(frame,&next))return false;
    if(seen&&next.sample==status.sample&&next.uptime==status.uptime)return false;
    restarted=seen&&(int32_t)(next.uptime-status.uptime)<0;
    status=next;seen=true;receivedAt=now;
    if(pending&&!restarted&&(next.flags&FLP_ACK)&&next.id==commandId) {pending=false;result=next.result;}
    return true;
  }
  int takeResult() {int r=result;result=-1;return r;}
  bool timeout(uint32_t now) {
    if(!pending||(uint32_t)(now-startedAt)<1500)return false;
    cancel();return true;
  }
};
}
