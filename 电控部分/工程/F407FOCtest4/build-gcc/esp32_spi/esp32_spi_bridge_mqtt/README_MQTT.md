# ESP32-S3 MQTT 网关第一版

这个目录是在已验证的 SPI bridge 上增加 MQTT，不修改原来的 SPI 引脚、CRC、ARM、STOP 和 FOC 命令格式。

## 设备职责

```text
网站前端 -> 网站后端 -> MQTT -> ESP32-S3 -> SPI3 -> STM32 FOC
```

ESP32 不承载兔子业务数据库，只负责 Wi-Fi、MQTT、命令校验、SPI 转发和设备状态上报。

## 配置

依赖 `PubSubClient`；本机已安装 2.8。换电脑时在 Arduino IDE 的 Library Manager
搜索并安装 `PubSubClient`，或使用：

```powershell
arduino-cli lib install PubSubClient
```

在 `esp32_spi_bridge_mqtt.ino` 顶部修改：

```cpp
WIFI_SSID
WIFI_PASSWORD
MQTT_HOST
MQTT_PORT
MQTT_USERNAME
MQTT_PASSWORD
DEVICE_ID
```

占位配置时程序不会尝试联网，串口会显示 `MQTT_CONFIG=NOT_SET`。当前使用普通 MQTT 1883
做局域网联调；正式部署应切换到 TLS 端口和证书配置。

## 主题

设备编号为 `rabbit-foc-001` 时：

```text
rabbit/rabbit-foc-001/cmd
rabbit/rabbit-foc-001/ack
rabbit/rabbit-foc-001/status
```

命令主题同时接受旧式纯文本和第一版 JSON：

```text
ARM
PING
STOP
v30,v0
```

```json
{"request_id":"cmd-001","command":"v30,v0"}
```

命令字段仍遵循 FOC 当前格式；两个电机字段都必须带模式。`v30,v0` 是两个电机速度模式，
而 `v30,0` 会被拒绝。收到命令后 ESP32 发布确认，例如：

```json
{
  "device_id":"rabbit-foc-001",
  "request_id":"cmd-001",
  "result":"spi_sent",
  "armed":1,
  "tx":3,
  "foc_link":"unverified"
}
```

目前 `foc_link` 仍是 `unverified`，因为现有 FOC 固件还没有 SPI 回传 ACK；下一阶段要扩展
STM32 的双向 SPI 状态帧。

状态主题会保留发布设备在线、Wi-Fi、MQTT、SPI ready、ARM、发送计数和拒绝计数。

## 给网页同学的对接内容

网页前端不直接连 ESP32，也不直接发 MQTT。后端负责：

1. 接收前端的开始、停止、速度、复位请求；
2. 记录 `device_commands` 和 `request_id`；
3. 向设备命令主题发布 JSON；
4. 订阅 ACK/status，更新设备状态和喂养记录；
5. 前端只调用后端 HTTP API 或 WebSocket。

后端建议提供：

```text
POST /api/devices/{device_id}/commands
GET  /api/devices/{device_id}/status
GET  /api/devices/{device_id}/telemetry
```

前端需要的按钮/数据显示：设备在线、开始、停止、速度设置、软件复位、当前转速、接触检测、
故障码、最后通信时间、喂养任务进度和历史记录。

## 编译

只编译，不烧录：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\build_esp32_mqtt.ps1'
```

首次联调先用局域网 MQTT broker。未填写 broker 和 Wi-Fi 参数时，串口/SPI 功能仍可单独使用。
