# ESP32-S3 配网 / MQTT / SPI 网关 v8.2（单M1一键喂奶及反向回程）

目标板：ESP32-S3 / S3-N16R8。ESP32 负责网络配置、命令检查与 SPI 转发；兔子档案、喂养计划和数据库由网站后端负责。

## v8.2 当前流程（2026-10-16，候选，尚未烧录）

`FEED` 不再仅启用监听，而是发起完整单M1循环：`o0,v100`下降 → 匹配运动回执和本次新运动 → 第一次接触后STOP回执及3个新停稳样本 → 奶泵3000ms并关闭 → `o0,v-100`回程 → 旧接触清除、匹配回执和2个新反向运动样本 → 第二次新接触后STOP回执及3个新停稳样本 → `cycle_complete`。

**语义变化：收到并接受FEED后会自动启动M1，不再另外发速度命令，也不需要单独ARM。** 上电仍先`LOCAL`，再`PING`并等待`foc_pong`；反馈有效、停止条件满足后才接受FEED。重复FEED在运行期间拒绝，不重发运动；STOP随时取消且不会自动续转或回程。只有正常供奶计时结束才允许自动回程。

本候选仅使用已经确认的正反馈下降配置`FEED_DOWN_RPM_SIGN=1`，运动命令固定为`o0,v100`和`o0,v-100`。沿用STM32既有65 RPM判定、M1故障保护、64字节SPI v2协议和GPIO4奶泵输出，不含M0横移、清洗、双笼调度或绝对位置回零。回程的“接触”是现有M1低速接触标志，不是新增上限位传感器。

说明：[一键FEED与回程候选](E:/养兔子/docs/ESP32_FEED自动下降与反向回程_2026-10-16.md)。候选仅在主机模拟及编译验证；没有烧录、运动或开泵实测，也没有修复BOOT0启动选择。

## 历史版本记录（以下发布包及验证仅属于对应历史固件）

**v7.2 触发帧诊断（2026-10-14）**：FEED 阶段检查触发的保护取消会输出 `FEED_ABORT`，包括本次 `x_not_stopped`；手动 STOP、FEED 拒绝以及通信处理提前取消不保证产生该行。先保存本次检查的 FOC 快照，先关泵并请求双轴 STOP，再打印保存的数据；不会把 STOP 回传后的正常状态误当触发帧。记录 `phase_before`、`mode0`、`rpm0_x100`、`sample`、`age_ms`、编码器启用位及通信故障等。`rpm0_x100=8500` 表示85 RPM。`x_not_stopped` 时，`mode0!=0` 是横向轴模式不符合；`mode0=0` 且转速绝对值大于6500表示回传转速超过65 RPM。日志证明的是收到的反馈，不单独证明实际轴运动；停机之后的正常状态不能否定此前触发。运动、开泵条件和保护门槛不变，FOC 固件不变。该版本的发布与上传证据见 `E:\养兔子\docs\ESP32_v7_2_FEED触发帧诊断_2026-10-14.md`；下述 v7.1 发布包是历史版本。

**v7.1 本地模式（2026-10-14）**：新增本机串口命令 `LOCAL`。无需保存 Wi-Fi/MQTT 配置即可退出配网，保持电机未解锁、两路泵输出关闭，再用 `PING → FEED` 联调。必须重新编译上传包含此功能的固件，旧 v7 BIN 不支持该命令。

本地模式交付包：`E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\release_mqtt_provisioned_com_20261014_203908`。已完成交叉编译、398项网关/继电器检查及现有回归；现已通过COM13上传，四段闪存哈希验证成功，实机串口确认LOCAL接受、未解锁、两路输出关闭。未发PING/FEED、运动或开泵指令，未做供奶实物闭环。上传证据见包内`upload_receipt_COM13.json`、`upload_COM13.log`、`status_COM13_LOCAL.log`；`manifest.json`中的flashed=false仍表示构建时状态，以实际上传回执为准。下文旧的 `20261014_183306` 是前一版正方向包，不包含 LOCAL。

继承 v6 的 GPIO4 奶泵、GPIO5 清水泵限时测试、触发极性保存和 `SPITEST` 连续探测。v7 增加 **M1 本次下降触碰停机 → STOP 回执及连续停稳 → 奶泵测试3秒 → 关闭** 阶段。不是完整双笼 FEED，不自动横移、下降、抬升，也未实现 CLEAN 或机械 RESET；FOC 电机算法、PID、65 RPM 判据和 SPI v2 帧结构不变。

**2026-10-14 方向确认：用户已确认 M1 实际下降时，FOC 遥测 `m1_rpm` 为正。** 当前生产配置为 `FEED_DOWN_RPM_SIGN=1`，并已生成 `FeedStageConfig.h`；`0` 仍表示方向未知，使用该值时拒绝 FEED。更换电机、编码器或接线后应重新确认反馈符号，不随意猜正负。

**反馈转速为正不等于电机控制指令必须为正。** 下降命令仍使用现场已验证的 M1 参数；本次仅校准阶段的反馈方向判定，FEED 只启用监听，不自动发送下降命令。3秒是测试时长，不代表正式供奶量。

```text
网站前端 → 网站后端 → MQTT broker → ESP32-S3 → SPI3 → STM32 FOC
```

## 当前验证边界

主机测试、固件编译和硬件联调是三件不同的事。主机测试不代表电机已经验证。
沿用 v5 的 FOC 回传 ACK 和真实变量遥测，配套 STM32 的 `F407FOCtest4_gcc_spi_duplex.bin`。本次继电器补丁不要求重烧已匹配的 FOC 固件；旧版 32 字节协议不兼容，不允许新旧混刷。
`spi_pending` 仅表示 ESP32 发帧并等待；只有收到命令编号一致、版本和 CRC 正确的 FOC 回执，才会发布 `foc_accepted` / `foc_pong`。
`foc_accepted` 表示目标参数已被 FOC 接受，不代表电机已转动、已到达目标或喂奶已完成。实际状态需要看遥测。
代码和主机测试不替代真机联调；停止帧丢失仍可能发生。FOC 增加了仅监管 SPI 接管的两秒软件通信看门狗，不替代硬件急停和限位；它依赖 FOC 主循环继续运行。

`release_mqtt_provisioned_com_20261014_003735` 是方向尚未确认、`FEED_DOWN_RPM_SIGN=0` 时生成的历史包，其 BIN 仍拒绝 FEED，不会因电脑上的参数文件改成1而获得校准。此次正反馈方向配置已完成主机回归及重新编译，结果如下；历史包不作为此次校准后的固件。

### 2026年10月14日正反馈方向配置验证

- 生产配置：`FEED_DOWN_RPM_SIGN=1`，奶泵测试时长 `FEED_MILK_RUN_MS=3000`；FEED只监听，不自动下降，反馈正号不等于运动命令正号。
- 主机回归通过：39项网关校验、219项生产网关运行、60项继电器/探测，共318项；另通过生产配置阶段测试、19项旧桥接、105项SPI协议、40项STM32运行及46项速度/接触保护回归。
- ESP32-S3交叉编译成功：程序957391字节，静态RAM48820字节。
- 当前正方向交付目录：[release_mqtt_provisioned_com_20261014_183306](E:/养兔子/FOC_all/3_F407FOCtest4_new/build-gcc/esp32_spi/release_mqtt_provisioned_com_20261014_183306)。
- 证据：[主机回归日志](E:/RabbitBuild/logs/feed-positive-direction-tests-20261014.log)、[编译日志](E:/RabbitBuild/logs/feed-positive-direction-build-20261014.log)。编译日志明确记录 `Compile completed. NO hardware was flashed.`；**本轮未烧录，未实机验收，也没有执行电机或泵控制。**

## 编译与上传

源文件必须放在同一个目录：打开本目录的 `esp32_spi_bridge_mqtt_provisioned.ino`，不要只复制 INO；它依赖同目录的全部本地头文件，包括 `FeedContactStage.h`、`FeedStageConfig.h`、`FocLinkProtocol.h`、`FocLinkClient.h`。Arduino IDE 编译前须运行 `generate_feed_stage_config.ps1`；项目构建脚本会自动生成配置头。

主机回归统一入口（不连接硬件）：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\Tests\verify_feed_stage.ps1'
```

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

脚本默认单线程，也可指定 1–8 个任务；不要同时让 Arduino IDE 编译同一工程。本机已验证的命令使用真实 SDK 配置路径：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\build_esp32_mqtt_provisioned.ps1' -Jobs 8 -ConfigFile 'E:\RabbitBuild\relay-v6\arduino-cli.yaml' -BuildDirectory 'E:\RabbitBuild\relay-v6\build'
```

脚本不烧录。输出构建目录 `E:\RabbitBuild\verified-com`；只有编译成功，才在脚本旁创建带时间戳的 `release_mqtt_provisioned_com_*` 目录，保存固件、日志、SHA256 清单和源码哈希。编译日志在 `E:\RabbitBuild\logs`，不放在 Arduino 会自动清理的构建目录内。

默认输出目录是 `verified-com`，上例显式改为 `relay-v6\build`；`Jobs` 只影响电脑编译并行数，不改变 ESP32 的 80MHz 运行频率。发布前会核对实际构建输入与源码，Arduino 自动补的源路径 `#line` 标记和 INO 末尾换行不会被误判为代码变动。

### 2026年10月10日实际验证结果

- 已成功交叉编译：程序 952763 字节，占 3MB 应用分区 30%；静态全局变量 48700 字节，占 14%。最终应用 BIN 为 952912 字节。
- 网关/继电器主机检查：39 + 119 + 60 = 218 项通过；另通过现有 SPI、STM32 通信、速度/接触与编码器回归。
- 历史 v6 交付目录：[release_mqtt_provisioned_com_20261010_010535](E:/养兔子/FOC_all/3_F407FOCtest4_new/build-gcc/esp32_spi/release_mqtt_provisioned_com_20261010_010535)。它不包含 v7 触碰供奶阶段；当前 v7 应使用与当前配置匹配且已成功构建、校验的新发布包，不能沿用历史 v6 或方向未标定包。
- **没有烧录，也没有启动真机电机或泵；继电器实物、默认释放和带载干扰仍待现场验证。**

如果再次出现 `cc1plus out of memory`，先关闭无用程序，确保 Windows 分页文件未被禁用，必要时重启，再单独运行脚本。不要修改 C++ 标准库头文件来绕过错误。`Invalid argument` 若重现，需要保留完整日志继续定位，不能直接认定为缺少头文件。

## 首次配网 / 换网络

1. 上传成功后打开串口监视器，115200、New Line，按 RST。
2. 未配置时自动开热点。串口打印 `PROVISION_AP ssid=Rabbit-Setup-... password=Rbt-... url=http://192.168.4.1/`。密码每次开启热点随机生成，以实际日志为准。
3. 手机或电脑连接该热点，浏览器手动打开 `http://192.168.4.1/`。没有自动弹窗也可用。
4. 填 2.4GHz WiFi、MQTT 主机、端口、账号、密码、设备 ID。MQTT 主机只填域名或 IPv4，不带 `http://`，不要填电脑自身的 `localhost`。
5. 保存后 ESP32 重启；配置保存在 NVS，不需要每次更换网络都烧录。
6. 再配置：先停止并锁定，然后串口发送 `CONFIG`；或程序已运行且锁定时长按 BOOT 五秒。上电时不要按住 BOOT。
7. `FACTORY_RESET` 只清除网络配置，不清继电器极性。v7 保留已有网络配置结构，不要求清除原有配置；它不是机械 RESET。

配置模式禁止 ARM、FEED 和运动命令。已配置设备的临时热点十分钟后关闭；首次无配置时持续等待配网，也可通过下述 `LOCAL` 显式进入本地模式。
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

本次没有修改 STM32 生产代码；引脚、PWM、电机算法和不超过 65 RPM 的接触检测条件保持不变。USART1/XCOM 仍然独立使用；有效串口命令接管后退出 SPI 看门狗监管。

## 串口命令

### 不配网的 USB 本地联调（v7.1）

先确认 FOC 和两路泵已停止，Arduino 串口监视器连接 **ESP32**，115200、发送换行，逐条发送：

```text
LOCAL
PING
STATUS
FEED
```

1. `LOCAL` 必须返回 `accepted_local_mode`。它关闭配网页面和配置热点，清除排队的普通网络命令，不发运动/开泵命令，不保存/清除 NVS 配置；`STATUS` 显示 `mode=LOCAL`。网络 `config=0`、`wifi=0`、`mqtt=0` 不再阻止本机 FEED。
2. 切换后必须重新 `PING`，等待匹配的 `FOC_RESULT ... result=foc_pong`，再检查 `FOC_LINK=VERIFIED`；不能连续粘贴整段而不等回执。`LOCAL` 不跳过 SPI、编码器、停机或方向检查。
3. v8.2的`FEED`返回`accepted_feed_starting`，自动发送`o0,v100`并等待匹配回执，不再从XCOM另发速度命令；关闭XCOM循环发送，避免另一个命令源干扰本轮。
4. 本次下降运动、第一次新接触、STOP回执和连续停稳确认后，开启奶泵3000ms并关闭，然后自动发送`o0,v-100`。回程必须看到旧接触清除及本次新反向运动，再确认第二次新接触、STOP回执和连续停稳，才输出`FEED_CYCLE_COMPLETE`。无需另发ARM；紧急中止发送`STOP`，不自动续转。
5. 模式只对本次运行有效，重启后按原流程启动，未配网设备需要再次发 `LOCAL`。停止并锁定后，发 `CONFIG` 可重新进入配网；保存后重启恢复原网络流程。已保存网络不会被 LOCAL 删除，已有 STA/MQTT 连接可以保留，但非 STOP 远程控制被拒绝。

若返回 `rejected_stop_before_local`，先发 `STOP`，等待停止回执，再切换；`rejected_restart_pending` 表示已安排重启，等重启完成再操作。`rejected_ap_stop_failed` 表示热点关闭失败，保留配网禁动状态，可重试 LOCAL 或检查启动日志。

先脱离动物、用清水验证一次完整链路。本机串口模式不要求发送网络 KEEPALIVE；SPI新鲜反馈和故障停机保护保持不变。

### 继电器与 SPI 现场联调（v6 新增）

串口监视器仍用 Arduino IDE 或 XCOM，115200、发送换行，同一个端口不能同时被两个工具占用。继电器是 GPIO 电平控制，没有 MQTT 主机配置，也不把泵的 12V 接进 ESP32。

**用户已确认模块为高电平触发：HIGH 吸合，LOW 释放。** 当前默认两路均为 HIGH 触发；启动加载默认/已保存极性后，先请求两路关闭。第一次烧录及配置前仍断开泵负载电源，通过 `RELAY CONFIG HIGH` 明确保存实物配置。旧的已保存极性优先于编译默认值，换模块时应重新确认。ROM/复位早期仍需要实际模块硬件偏置保证默认释放，软件和听到“咔哒”不能代替这项测试。

| 串口输入 | 效果 |
|---|---|
| `LOCAL` | 仅本机：停止状态下退出配网，进入不依赖网络的本地模式；须重新 PING，不自动运动或开泵 |
| `RELAY CONFIG LOW` | 两路均为低电平吸合，保存极性并请求关闭 |
| `RELAY CONFIG HIGH` | 两路均为高电平吸合，保存极性并请求关闭 |
| `RELAY CONFIG LOW HIGH` | 奶泵低电平、水泵高电平；相反组合也支持 |
| `RELAY MILK 500` | 奶泵输出开 500 ms 后自动关，清水泵保持关 |
| `RELAY WATER 500` | 清水泵输出开 500 ms 后自动关，奶泵保持关 |
| `RELAY OFF` | 立即请求两路关闭；FEED 活动时还会取消阶段并发送电机停止 |
| `RELAY STATUS` | 查看极性、GPIO 和请求输出；不是触点/流量反馈 |
| `SPITEST 10` | 连续 10 次 PING，不 ARM，不发运动、不自动开继电器 |
| `STATUS` | 原网关/SPI 遥测加继电器、探测及 FEED 阶段进度 |
| `FEED` | v8.2一键启动M1下降、接触停机供奶、关泵、反向至第二次新接触并停机 |
| `STOP` | 先关继电器，再取消探测、锁定并通知 FOC 停止 |

继电器单次时长只能是 **1–2000 ms 的整数**，没有无限时 ON；测试中的重复/另一通道开启返回 `rejected_relay_busy`，不会续时或同时开两路。联调输出开启期间不能 ARM；电机已经 ARM 时不能开始继电器或 SPITEST 测试。`RELAY`、`SPITEST` **仅限本机串口**，MQTT 不接受这些命令，也不新增网页按钮。

先独立测继电器，再测 SPI：

```text
RELAY CONFIG HIGH
RELAY MILK 500
RELAY WATER 500
RELAY STATUS
SPITEST 10
STATUS
```

上例 HIGH 对应本次用户确认的模块；更换模块先核对极性，再配置，不要接着泵负载试错。每个继电器命令间等待上一次输出自动关闭，再发下一条。

SPI 验收看：`SPITEST_DONE total=10 pong=10 failed=0 result=PASS`。只有匹配的 `foc_pong` 算成功；`foc_accepted`、错编号、重复快照不算。次数范围 1–50，任一次错误最终为 FAIL；重启/STOP 会取消，不伪装成完成。连续通过之后还要观察真实硬件稳定性，不能只凭主机测试结果判定接线合格。

组合测试先完成 PING 验证，再发 `RELAY MILK 500`/`RELAY WATER 500`，同时用 SPITEST 观察。已开始 SPI 轮询时，继电器测试绑定当前新鲜链路，失联、回执超时或 FOC 重启即请求关闭。尚未启动 SPI 的独立继电器测试不要求 FOC 在线；它仍有本地限时。所有限时依赖主循环继续执行，不替代独立泵电源切断。

极性保存于独立 NVS `rabbitrelay`，网络配置仍在 `rabbitnet`；更换触发类型通过串口重新配置，不需要重烧。保存成功返回 `accepted_relay_config`，保存失败不启用新设置。两路 GPIO 与最长测试时长集中在 [RelayControl.h](E:/养兔子/FOC_all/3_F407FOCtest4_new/build-gcc/esp32_spi/esp32_spi_bridge_mqtt_provisioned/RelayControl.h) 前部，中文注释已补齐。

### 原有手动电机接口（保持兼容）

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

纯文本只兼容 `PING` 和 `STOP`；运动、解锁和 FEED 必须发 JSON。例子中的 SESSION_FROM_STATUS 需替换成当前状态的 session_id。`request_id` 可省略；保留它时须使用不同值。SPI 内部回执序号仍由 ESP32 自动生成：

```json
{"session_id":"SESSION_FROM_STATUS","command":"FEED"}
```

FEED先检查单M1停止状态、链路有效及已确认的方向配置，再自动发起下降及回程循环；M0始终停用。循环中拒绝额外运动命令和重复FEED，STOP始终优先。远程运行全过程仍须维持KEEPALIVE；中断或失败后不自动重发FEED。

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

超过五秒未续约、检测到网络断线或 session 更换，ESP32 先关闭泵输出，再发停止帧并锁定，不自动恢复。提供 request_id 时，最近十六条去重，重复请求不会续约；省略时不去重。后端不得因超时自动重发 FEED，应先查状态并确认。

停止不需要 ARM 或 session：

```json
{"request_id":"stop-004","command":"STOP"}
```

允许 JSON 字段仅为 `command`、`request_id`、`session_id`；request_id 可选，除 PING/STOP 外当前 session_id 必须匹配。拒绝未知/重复字段、转义、截断、尾随内容。ID 使用字母、数字、短横线、下划线。远程不能执行 CONFIG 或 FACTORY_RESET。

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
失联保留最后值但`telemetry_valid=0`；前端应显示“过期/未知”。后端必须检查接收时间和递增sample，不能把历史retained心跳当成在线。`feed_phase`新增`wait_down_ack`、`wait_return_ack`、`wait_return_motion`、`wait_return_contact`、`wait_return_stop_ack`、`confirm_return_stopped`、`cycle_complete`；只有`cycle_complete`代表此次单M1下降/供奶/回程结束。旧的`contact_stage_complete`只是历史分段回归入口，不是新循环完成。尚未回传绝对位置，也未实现机械回零。
MQTT 发布回执使用 QoS0，断线时可能丢失；网站后端还应给请求设置超时，不能因收不到回执就自动重发运动。request_id 可省略；提供时每次使用唯一值。

网站后端仍负责HTTP API、登录授权、设备绑定、命令记录、续约和状态订阅；本版FEED覆盖单M1下降、供奶和反向接触返回，仍不包含M0横移或双笼调度，不接受未实现的CLEAN/机械RESET。RESET是机械回零，不等同网络恢复或MCU重启。停止控制后应发STOP并停止续约。

## 上传已编译固件（不重复编译）

成功构建后可用脚本直接上传，避免 Arduino IDE 再次多线程编译。先断开 FOC 连线、关闭串口监视器，确认 ESP32 COM 口对应的 Windows 端口：

```powershell
# 把 COM7 和发布目录替换为本机实际值；加 -WhatIf 仅校验文件，不连接板子。
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\upload_esp32_mqtt_provisioned.ps1' -Port COM7 -ReleaseDirectory '实际发布目录' -WhatIf
```

确认后去掉 `-WhatIf` 才会上传。脚本先核对应用、bootloader、分区文件的 SHA256；不得把此 ESP32 固件写入 STM32 FOC，也不要将仅应用 BIN 当成从地址 0 开始的完整镜像。
