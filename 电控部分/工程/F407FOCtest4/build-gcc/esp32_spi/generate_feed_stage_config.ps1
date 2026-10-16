param(
    [string]$InputPath = (Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt_provisioned\feed_stage.env'),
    [string]$OutputPath = (Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt_provisioned\FeedStageConfig.h')
)
$ErrorActionPreference = 'Stop'
$fields = [ordered]@{
    FEED_MILK_RUN_MS = @('milkMs',0,30000,'奶泵时长，毫秒；0禁止开泵')
    FEED_MAX_MILK_MS = @('maxMilkMs',1,30000,'最长开启上限，毫秒')
    FEED_DOWN_RPM_SIGN = @('downSign',-1,1,'下降实测转速符号；0尚未标定')
    FEED_WAIT_MOTION_MS = @('waitMotionMs',1,120000,'等待下降运动超时，毫秒')
    FEED_CONTACT_TIMEOUT_MS = @('contactTimeoutMs',1,120000,'等待本次接触超时，毫秒')
    FEED_STOP_TIMEOUT_MS = @('stopTimeoutMs',1,10000,'确认停止超时，毫秒')
    FEED_MAX_FEEDBACK_AGE_MS = @('maxAgeMs',1,1000,'反馈最大年龄，毫秒')
    FEED_MOVING_SAMPLES = @('movingSamples',2,10,'连续下降运动样本数')
    FEED_STOPPED_SAMPLES = @('stoppedSamples',3,10,'连续停稳样本数')
}
$values = @{}
foreach($line in (Get-Content -LiteralPath $InputPath -Encoding UTF8)) {
    $line=$line.Trim(); if(!$line -or $line.StartsWith('#')){continue}
    if($line -notmatch '^([A-Z_]+)=(-?\d+)$'){throw "配置行无效：$line"}
    $key=$Matches[1]; $value=0L
    if(!$fields.Contains($key) -or $values.ContainsKey($key)){throw "未知或重复参数：$key"}
    if(![long]::TryParse($Matches[2],[ref]$value)){throw "整数无效：$key"}
    if($value -lt $fields[$key][1] -or $value -gt $fields[$key][2]){throw "参数超出范围：$key"}
    $values[$key]=$value
}
foreach($key in $fields.Keys){if(!$values.ContainsKey($key)){throw "缺少参数：$key"}}
if($values.FEED_MILK_RUN_MS -gt $values.FEED_MAX_MILK_MS){throw '奶泵时长超过最长保护时间'}
$header=@('#pragma once','#include "FeedContactStage.h"','// 根据feed_stage.env生成；不要手改本文件。','namespace feedconfig {','inline feed::Settings settings(){','  feed::Settings s;','  s.autoCycle=true; // FEED自动下降、定时喂奶、反向至第二次新接触；无横移或清洗。')
foreach($key in $fields.Keys){$header+=('  s.{0}={1}; // {2}' -f $fields[$key][0],$values[$key],$fields[$key][3])}
$header+=@('  return s;','}','}')
[IO.File]::WriteAllText($OutputPath,($header -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "已生成：$OutputPath"
if(!$values.FEED_MILK_RUN_MS -or !$values.FEED_DOWN_RPM_SIGN){Write-Output 'FEED未标定：当前生成配置禁止自动开泵。'}
