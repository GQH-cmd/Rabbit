# ESP32-S3 配网 / MQTT / SPI 网关 v5（SPI 双向协议 v2）

目标板：ESP32-S3 / S3-N16R8。ESP32 负责网络配置、命令检查与 SPI 转发；兔子档案、喂养计划和数据库由网站后端负责。

```text
网站前端 → 网站后端 → MQTT broker → ESP32-S3 → SPI3 → STM32 FOC
```

## 当前验证边界

主机测试、固件编译和硬件联调是三件不同的事。主机测试不代表电机已经验证。
本版新增 FOC 回传 ACK 和真实变量遥测，必须配套烧录 STM32 的 `F407FOCtest4_gcc_spi_duplex.bin`。旧版 32 字节协议不兼容，不允许新旧混刷。
`spi_pending` 仅表示 ESP32 发帧并等待；只有收到命令编号一致、版本和 CRC 正确的 FOC 回执，才会发布 `foc_accepted` / `foc_pong`。
`foc_accepted` 表示目标参数已被 FOC 接受，不代表电机已转动、已到达目标或喂奶已完成。实际状态需要看遥测。
代码和主机测试不替代真机联调；停止帧丢失仍可能发生。FOC 增加了仅监管 SPI 接管的两秒软件通信看门狗，不替代硬件急停和限位；它依赖 FOC 主循环继续运行。

## 编译与上传

源文件必须放在同一个目录：打开本目录的 `esp32_spi_bridge_mqtt_provisioned.ino`，不要只复制 INO；它依赖同目录的全部本地头文件（包含新增 FocLinkProtocol.h、FocLinkClient.h）。

已安装依赖：ESP32 core 3.3.11、PubSubClient 2.8。SPI、WiFi、Preferences、WebServer、Hash 随核心包提供。INO 显式包含 SHA1Builder 和 MD5Builder，以便 Arduino 找到 Hash 依赖。

Arduino IDE 选择：
- Board：ESP32S3 Dev Module
- Flash Size：16MB
- PSRAM：OPI PSRAM
- Partition Scheme：对应 `app3M_fat9M_16MB` 的 16MB 分区
- Flash Mode：DIO；CPU Frequency：80MHz
- USB CDC On Boot：Disabled（本次交付使用板上 COM 接口）
- Upload Speed：115200；端口选择插拔 COM 接口后出现的端口，不能固定假定为 COM7

电脑接 ESP32 标有 **COM** 的 USB 口，先断开 ESP32 与 FOC 的所有线，独立上传和验证配网。若板子异常烫，断电检查，不要靠不断刷程序验证硬件。

推荐用单线程脚本，避免 Arduino IDE 同时编译：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\build_esp32_mqtt_provisioned.ps1'
```

脚本不烧录。输出构建目录 `E:\RabbitBuild\verified-com`；只有编译成功，才在脚本旁创建带时间戳的 `release_mqtt_provisioned_com_*` 目录，保存固件、日志、SHA256 清单和源码哈希。编译日志在 `E:\RabbitBuild\logs`，不放在 Arduino 会自动清理的构建目录内。

如果再次出现 `cc1plus out of memory`，先关闭无用程序，确保 Windows 分页文件未被禁用，必要时重启，再单独运行脚本。不要修改 C++ 标准库头文件来绕过错误。`Invalid argument` 若重现，需要保留完整日志继续定位，不能直接认定为缺少头文件。

## 首次配网 / 换网络

1. 上传成功后打开串口监视器，115200、New Line，按 RST。
2. 未配置时自动开热点。串口打印 `PROVISION_AP ssid=Rabbit-Setup-... password=Rbt-... url=http://192.168.4.1/`。密码每次开启热点随机生成，以实际日志为准。
3. 手机或电脑连接该热点，浏览器手动打开 `http://192.168.4.1/`。没有自动弹窗也可用。
4. 填 2.4GHz WiFi、MQTT 主机、端口、账号、密码、设备 ID。MQTT 主机只填域名或 IPv4，不带 `http://`，不要填电脑自身的 `localhost`。
5. 保存后 ESP32 重启；配置保存在 NVS，不需要每次更换网络都烧录。
6. 再配置：先停止并锁定，然后串口发送 `CONFIG`；或程序已运行且锁定时长按 BOOT 五秒。上电时不要按住 BOOT。
7. `FACTORY_RESET` 清除网络配置。本次 v4 → v5 保留网络配置结构，不要求清除原有配置。

配置模式禁止 ARM 和运动命令。已配置设备的临时热点十分钟后关闭；首次无配置时持续等待配网。
当前代码仅支持明文 MQTT，使用受控局域网及 broker 账号/主题权限；**尚未实现 TLS**，不能只换成 8883 就认为获得加密。

## SPI 接口

| ESP32 GPIO | STM32 信号 |
|---|---|
| 15 | PA15 / SPI3 CS |
| 13 | PC10 / SPI3 SCK |
| 14 | PC11 / SPI3 MISO |
| 12 | PC12 / SPI3 MOSI |
| GND | GND |

SPI Mode 0，100kHz，64 字节帧，协议版本 2，CRC16-CCITT-FALSE，最小帧间隔 100ms。逻辑是 3.3V；各板独立供电时只接信号和 GND，不连接两板电源引脚。此表是信号映射，不是六针插头物理排列，插头顺序必须由原理图/导通测量确认。

本次同步修改 STM32 通信代码；引脚、PWM、电机算法和小于 65 RPM 的接触检测条件保持不变。USART1/XCOM 仍然独立使用；有效串口命令接管后退出 SPI 看门狗监管。

## 串口命令

`STATUS` 查看本地状态和最后的 FOC 遥测；`PING` 探测并启动无运动的状态轮询；收到 `foc_pong` 后才能 `ARM` 本地解锁；`STOP`/`DISARM` 发 `o0,o0` 并锁定。
启动不自动发送 SPI 帧。第一次测试只发送 `PING`，等待 `FOC_RESULT ... result=foc_pong`，再发 `STATUS`，应看到 `FOC_LINK=VERIFIED`。没有回执先查接线和两端固件，不要反复发速度命令。
从机回传是预先准备的上一份快照，因此不能在发命令的同一帧就认为执行成功；ESP32 每 100ms 发只读轮询取回回执。
两个电机字段都必须有模式，例如 `v30,o0`，不能发 `v30,0`。这只是格式示例，不保证克服 FOC 的接触检测停机条件。
本地和远程控制不能交叉接管；先 STOP，再由新的控制源 ARM。

## 网站后端 MQTT 对接（新版）

设备 ID 示例 `rabbit-foc-001`：

```text
命令   rabbit/rabbit-foc-001/cmd
回执   rabbit/rabbit-foc-001/ack
状态   rabbit/rabbit-foc-001/status
```

**命令必须 retain=false，不要保留 ARM、速度或 KEEPALIVE。** 后端先订阅状态，确认收到新鲜在线心跳，取当前 `session_id`。先发送 `PING` 并等待 `foc_pong`，确认状态 `foc_link=verified`，再 ARM。断线重连会换 session，旧命令拒绝。

纯文本只兼容 `PING` 和 `STOP`；运动与解锁必须发 JSON。例子中的 SESSION_FROM_STATUS 需替换成当前状态的 session_id，每个 request_id 必须不同：

```json
{"request_id":"arm-001","session_id":"SESSION_FROM_STATUS","command":"ARM"}
```

收到 `accepted_local` 后，发送目标命令：

```json
{"request_id":"speed-002","session_id":"SESSION_FROM_STATUS","command":"v30,o0"}
```

远程解锁采用五秒租约，后端运行控制期间每秒发新的 KEEPALIVE：

```json
{"request_id":"keep-003","session_id":"SESSION_FROM_STATUS","command":"KEEPALIVE"}
```

超过五秒未续约、检测到网络断线或 session 更换，ESP32 发停止帧并锁定，不自动恢复。重复 request_id 最近十六条去重，不会续约，因此每次心跳都要新 ID。

停止不需要 ARM 或 session：

```json
{"request_id":"stop-004","command":"STOP"}
```

允许 JSON 字段仅为 `command`、`request_id`、`session_id`；拒绝未知/重复字段、转义、截断、尾随内容。ID 使用字母、数字、短横线、下划线。远程不能执行 CONFIG 或 FACTORY_RESET。

主要回执：
- `accepted_local`：ESP32 接受解锁/续约，不是 FOC 回执。
- `spi_pending`：SPI 已发，等待 FOC；前端显示“等待设备确认”。
- `foc_pong`：FOC 已回复当前 PING，双向链路探测通过。
- `foc_accepted`：FOC 接受了该运动/停止命令，不等于运动完成。
- `foc_rejected`：FOC 拒绝命令，未修改目标。
- `foc_timeout`：1.5 秒未收到匹配回执，执行结果未知；锁定，不自动重发运动。
- `cancelled_by_stop`：等待中的命令被 STOP 抢占；它可能已在 FOC 执行，必须继续等停止回执。
- `rejected_foc_unverified` / `rejected_spi_busy`：未通过 PING 验证、链路过期，或前一命令尚未完成确认。
- `rejected_not_armed` / `rejected_stale_session`：未解锁或 session 过期。
- `rejected_owner` / `rejected_config_mode`：控制源冲突或正在配网。
- `rejected_bad_json` / `rejected_bad_command`：消息格式错误。
- `duplicate_ignored` / `rejected_queue_full`：重复或队列满。

状态每秒发布，除原字段外，增加 `protocol=2`、`telemetry_valid`、`foc_age_ms`、`sample`、`foc_uptime_ms`、`m0_rpm`/`m1_rpm`、`m0_iq_a`/`m1_iq_a`、`m0_mode`/`m1_mode`、`contact0`/`contact1`、`encoder0_enabled`/`encoder1_enabled`、`spi_owner`、`link_faults_latched`、`bad_frames`。
转速、电流有正负号；模式为 0 开环 / 1 电流 / 2 速度 / 3 位置。`telemetry_valid=1` 表示收到的新鲜协议快照，不证明传感器健康。编码器 enabled 只表示 FOC 配置启用，不证明物理连线正常；未启用的转速在页面显示“未接入”。
FOC 状态超过一秒无更新或 MCU 重启时，ESP32 锁定正在进行的控制，清理队列并尝试停止；FOC 的 SPI 看门狗在两秒无有效续约时停止 SPI 控制的目标，恢复连接不自动恢复旧目标。
`link_faults_latched` 是开机后累计位图：1=SPI 传输/短帧，2=CRC/版本/格式，4=通信看门狗停机，8=遥测数值非有限或越界。这不是完整电机故障诊断；存在位 8 时不要把填零的遥测数值当真实测量。重启清零。
失联保留最后值但 `telemetry_valid=0`；前端应显示“过期/未知”，不能显示为正常实时值。状态为保留消息，后端必须检查接收时间和递增 sample，不能把历史 retained 心跳当成在线。尚未回传位置，也未实现回零/喂奶完成事件。
MQTT 发布回执使用 QoS0，断线时可能丢失；网站后端还应给请求设置超时，不能因收不到回执就自动重发运动。每次 request_id 唯一。

网站后端仍负责 HTTP API、登录授权、设备绑定、命令记录、续约和状态订阅；页面负责展示与发用户操作。停止控制后必须发 STOP，并停止续约。网页“复位”含义需另行确定：停止、回零和重启不是同一个功能。

## 上传已编译固件（不重复编译）

成功构建后可用脚本直接上传，避免 Arduino IDE 再次多线程编译。先断开 FOC 连线、关闭串口监视器，确认 ESP32 COM 口对应的 Windows 端口：

```powershell
# 把 COM7 和发布目录替换为本机实际值；加 -WhatIf 仅校验文件，不连接板子。
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\upload_esp32_mqtt_provisioned.ps1' -Port COM7 -ReleaseDirectory '实际发布目录' -WhatIf
```

确认后去掉 `-WhatIf` 才会上传。脚本先核对应用、bootloader、分区文件的 SHA256；不得把此 ESP32 固件写入 STM32 FOC，也不要将仅应用 BIN 当成从地址 0 开始的完整镜像。
