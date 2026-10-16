#pragma once
#include "FeedContactStage.h"
// 根据feed_stage.env生成；不要手改本文件。
namespace feedconfig {
inline feed::Settings settings(){
  feed::Settings s;
  s.autoCycle=true; // FEED自动下降、定时喂奶、反向至第二次新接触；无横移或清洗。
  s.milkMs=3000; // 奶泵时长，毫秒；0禁止开泵
  s.maxMilkMs=30000; // 最长开启上限，毫秒
  s.downSign=1; // 下降实测转速符号；0尚未标定
  s.waitMotionMs=30000; // 等待下降运动超时，毫秒
  s.contactTimeoutMs=15000; // 等待本次接触超时，毫秒
  s.stopTimeoutMs=3000; // 确认停止超时，毫秒
  s.maxAgeMs=500; // 反馈最大年龄，毫秒
  s.movingSamples=2; // 连续下降运动样本数
  s.stoppedSamples=3; // 连续停稳样本数
  return s;
}
}
