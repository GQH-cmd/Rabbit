#pragma once
#include "FocLinkProtocol.h"
namespace feed {
enum class State : uint8_t {
  Idle, WaitDownAck, WaitMotion, WaitContact, WaitStopAck, ConfirmStopped, ReadyMilk, MilkRunning,
  WaitReturnAck, WaitReturnMotion, WaitReturnContact, WaitReturnStopAck, ConfirmReturnStopped,
  Complete, CycleComplete, Aborted
};
enum class Action : uint8_t { None, SendStop, StartMilk, StopMilk, StartReturn, FinishCycle, Abort };
// 等待接触期间只允许M0=o0、M1非零开环/电流/速度；不允许横移或位置模式。
inline bool stageMotorCommand(const char* text){
  if(!text)return false;
  FlpMotor x{},z{};
  return flp_motor_pair(text,strlen(text),&x,&z)&&x.mode=='o'&&x.value==0&&
    strchr("ocv",z.mode)&&z.value!=0;
}
struct Settings {
  bool singleM1=false; // 默认保留双轴兼容测试；本项目网关显式启用单M1。
  bool autoCycle=false; // 生成的生产配置为true；false仅保留旧分段流程的回归入口。
  uint32_t milkMs=0,maxMilkMs=30000,waitMotionMs=30000,contactTimeoutMs=15000;
  uint32_t stopTimeoutMs=3000,maxAgeMs=500;
  int8_t downSign=0;uint8_t movingSamples=2,stoppedSamples=3;
};
struct Controller {
  State state=State::Idle;const char* reason="idle";Settings settings{};
  uint32_t enteredAt=0,pumpAt=0,lastSample=0,lastUptime=0,stopId=0,motionId=0;
  uint8_t movingCount=0,stoppedCount=0;
  // SPI中的rpm字段单位是0.01 RPM；保持原FOC的65 RPM阈值，不改变电机算法。
  static constexpr int32_t StopRpmX100=6500;
  static constexpr uint32_t MotionAckMs=1500;
  static int64_t absolute(int32_t n){return n<0?-static_cast<int64_t>(n):n;}
  static bool valid(const Settings& s){
    return s.milkMs>0&&s.milkMs<=s.maxMilkMs&&s.maxMilkMs<=30000&&
      (s.downSign==1||s.downSign==-1)&&s.waitMotionMs>0&&s.waitMotionMs<=120000&&
      s.contactTimeoutMs>0&&s.contactTimeoutMs<=120000&&s.stopTimeoutMs>0&&s.stopTimeoutMs<=10000&&
      s.maxAgeMs>0&&s.maxAgeMs<=1000&&s.movingSamples>=2&&s.movingSamples<=10&&
      s.stoppedSamples>=3&&s.stoppedSamples<=10&&(!s.autoCycle||s.downSign==1);
    // 一键候选仅使用已确认的o0,v100下降及其反向o0,v-100，不猜另一种方向标定。
  }
  static bool feedbackValid(const FlpStatus& s,bool singleM1=false){
    if(singleM1)return (s.flags&(FLP_READY|FLP_ENCODER1|FLP_M1_ONLY))==
      (FLP_READY|FLP_ENCODER1|FLP_M1_ONLY)&&!(s.flags&(FLP_ENCODER0|FLP_CONTACT0))&&
      !s.faults&&s.mode0==0&&s.rpm0==0&&s.iq0==0&&s.mode1<=3;
    // ENABLED只表示编码器已配置，不冒充实物传感器健康检测。
    return (s.flags&(FLP_READY|FLP_ENCODER0|FLP_ENCODER1))==
      (FLP_READY|FLP_ENCODER0|FLP_ENCODER1)&&!s.faults&&s.mode0<=3&&s.mode1<=3;
  }
  static bool xStopped(const FlpStatus& s){return s.mode0==0&&absolute(s.rpm0)<=StopRpmX100;}
  static bool bothStopped(const FlpStatus& s){return xStopped(s)&&s.mode1==0&&absolute(s.rpm1)<=StopRpmX100;}
  static bool active(State value){return value>=State::WaitDownAck&&value<=State::ConfirmReturnStopped;}
  bool running() const {return active(state);}
  bool permitsMotion() const {return !settings.autoCycle&&(state==State::WaitMotion||state==State::WaitContact);}
  bool returning() const {return state>=State::WaitReturnAck&&state<=State::ConfirmReturnStopped;}
  const char* motionCommand() const {
    if(state==State::WaitDownAck)return "o0,v100";
    if(state==State::WaitReturnAck)return "o0,v-100";
    return nullptr;
  }
  void enter(State next,uint32_t now,const char* why){state=next;enteredAt=now;reason=why;}
  Action fail(const char* why){state=State::Aborted;reason=why;return Action::Abort;}
  const char* begin(const Settings& cfg,const FlpStatus& s,uint32_t now,uint32_t age){
    if(running())return "rejected_feed_busy";
    if(!valid(cfg))return "rejected_feed_unconfigured";
    if(age>cfg.maxAgeMs||!feedbackValid(s,cfg.singleM1))return "rejected_feed_feedback";
    if(!bothStopped(s))return "rejected_feed_not_stopped";
    settings=cfg;lastSample=s.sample;lastUptime=s.uptime;
    movingCount=stoppedCount=0;stopId=motionId=0;pumpAt=0;
    if(cfg.autoCycle){enter(State::WaitDownAck,now,"wait_downward_command_ack");return "accepted_feed_starting";}
    enter(State::WaitMotion,now,"wait_downward_motion");return "accepted_feed_wait_motion";
  }
  // 所有等待均为非阻塞；重复帧不推进运动/停稳计数。上电不会自行调用begin。
  Action tick(const FlpStatus& s,uint32_t now,uint32_t age,bool pumpOn){
    if(!running())return Action::None;
    if(age>settings.maxAgeMs)return fail("feedback_stale");
    if(!feedbackValid(s,settings.singleM1))return fail("foc_fault_or_encoder");
    if(static_cast<int32_t>(s.uptime-lastUptime)<0)return fail("foc_restarted");
    bool fresh=s.sample!=lastSample;
    if((fresh&&static_cast<int32_t>(s.sample-lastSample)<0)||(!fresh&&s.uptime!=lastUptime))return fail("sample_invalid");
    if(fresh){lastSample=s.sample;lastUptime=s.uptime;}
    if(!xStopped(s))return fail("x_not_stopped");
    if(state==State::MilkRunning){
      if(!bothStopped(s)||!(s.flags&FLP_CONTACT1))return fail("motion_during_milk");
      if(static_cast<uint32_t>(now-pumpAt)>=settings.milkMs){
        if(settings.autoCycle){
          movingCount=stoppedCount=0;stopId=motionId=0;
          enter(State::WaitReturnAck,now,"wait_return_command_ack");return Action::StartReturn;
        }
        enter(State::Complete,now,"contact_stage_complete");return Action::StopMilk;
      }
      if(!pumpOn)return fail("milk_output_lost");
      return Action::None;
    }
    const bool reverse=returning();
    const int64_t signedSpeed=static_cast<int64_t>(s.rpm1)*settings.downSign*(reverse?-1:1);
    if(state==State::WaitDownAck||state==State::WaitReturnAck){
      if(static_cast<uint32_t>(now-enteredAt)>=MotionAckMs)
        return fail(reverse?"return_start_ack_timeout":"down_start_ack_timeout");
      if(signedSpeed<-StopRpmX100)return fail(reverse?"wrong_return_direction":"wrong_z_direction");
      if(!fresh||!motionId||!(s.flags&FLP_ACK)||s.id!=motionId)return Action::None;
      if(s.result!=FLP_ACCEPTED)return fail(reverse?"return_start_rejected":"down_start_rejected");
      movingCount=0;
      enter(reverse?State::WaitReturnMotion:State::WaitMotion,now,
        reverse?"wait_new_return_motion":"wait_downward_motion");
      // 匹配回执这一帧若含新运动，也必须参与连续采样，不重发运动指令。
    }
    const bool waitMotion=state==State::WaitMotion||state==State::WaitReturnMotion;
    const bool waitContact=state==State::WaitContact||state==State::WaitReturnContact;
    if(waitMotion||waitContact){
      uint32_t limit=waitMotion?settings.waitMotionMs:settings.contactTimeoutMs;
      if(static_cast<uint32_t>(now-enteredAt)>=limit)
        return fail(reverse?(waitMotion?"return_motion_timeout":"return_contact_timeout"):
          (settings.autoCycle&&waitMotion?"down_motion_timeout":"contact_wait_timeout"));
      if(signedSpeed<-StopRpmX100)return fail(reverse?"wrong_return_direction":"wrong_z_direction");
      if(!fresh)return Action::None;
      if(waitMotion){
        // 每一段都必须先见到正确方向的新运动及旧接触标志清除；旧接触不能结束回程。
        if(signedSpeed>StopRpmX100&&!(s.flags&FLP_CONTACT1)){
          if(++movingCount>=settings.movingSamples)enter(reverse?State::WaitReturnContact:State::WaitContact,
            now,reverse?"wait_new_return_contact":"wait_new_m1_contact");
        }else movingCount=0;
        return Action::None;
      }
      if(s.flags&FLP_CONTACT1){
        if(s.mode0!=0||s.mode1!=0)return fail("contact_without_stop");
        stopId=0;stoppedCount=0;
        enter(reverse?State::WaitReturnStopAck:State::WaitStopAck,now,
          reverse?"confirm_return_stop_ack":"confirm_stop_ack");return Action::SendStop;
      }
      return Action::None;
    }
    if(static_cast<uint32_t>(now-enteredAt)>=settings.stopTimeoutMs)return fail("stop_confirm_timeout");
    if(!(s.flags&FLP_CONTACT1))return fail("contact_lost");
    if(!fresh)return Action::None;
    if(state==State::WaitStopAck||state==State::WaitReturnStopAck){
      if(!stopId||!(s.flags&FLP_ACK)||s.id!=stopId)return Action::None;
      if(s.result!=FLP_ACCEPTED)return fail("stop_rejected");
      enter(reverse?State::ConfirmReturnStopped:State::ConfirmStopped,now,
        reverse?"wait_return_stable_stop":"wait_stable_stop");
    }
    if(state==State::ConfirmStopped||state==State::ConfirmReturnStopped){
      if(bothStopped(s))++stoppedCount;else stoppedCount=0;
      if(stoppedCount>=settings.stoppedSamples){
        if(reverse){enter(State::CycleComplete,now,"feed_cycle_complete");return Action::FinishCycle;}
        enter(State::ReadyMilk,now,"ready_milk");return Action::StartMilk;
      }
    }
    return Action::None;
  }
  bool bindStop(uint32_t id,uint32_t now){
    if((state!=State::WaitStopAck&&state!=State::WaitReturnStopAck)||stopId||!id)return false;
    stopId=id;enteredAt=now;return true;
  }
  bool bindMotion(uint32_t id,uint32_t now){
    if(!settings.autoCycle||(state!=State::WaitDownAck&&state!=State::WaitReturnAck)||motionId||!id)return false;
    motionId=id;enteredAt=now;return true;
  }
  bool pumpStarted(uint32_t now){
    if(state!=State::ReadyMilk)return false;
    pumpAt=now;enter(State::MilkRunning,now,"milk_running");return true;
  }
  void cancel(const char* why){state=State::Aborted;reason=why;}
};
inline const char* name(State state){
  switch(state){
    case State::Idle:return "idle";case State::WaitDownAck:return "wait_down_ack";
    case State::WaitMotion:return "wait_motion";
    case State::WaitContact:return "wait_contact";case State::WaitStopAck:return "wait_stop_ack";
    case State::ConfirmStopped:return "confirm_stopped";case State::ReadyMilk:return "ready_milk";
    case State::MilkRunning:return "milk_running";case State::Complete:return "contact_stage_complete";
    case State::WaitReturnAck:return "wait_return_ack";case State::WaitReturnMotion:return "wait_return_motion";
    case State::WaitReturnContact:return "wait_return_contact";case State::WaitReturnStopAck:return "wait_return_stop_ack";
    case State::ConfirmReturnStopped:return "confirm_return_stopped";case State::CycleComplete:return "cycle_complete";
    case State::Aborted:return "aborted";
  }return "invalid";
}
}
