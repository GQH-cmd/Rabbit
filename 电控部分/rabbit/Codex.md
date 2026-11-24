# Rabbit STM32 / ESP32 项目交接摘要

> 用途：新建 Codex 对话时，先完整阅读本文件，再检查实际工程与硬件状态。本文件记录截至 **2026-11-24** 的已完成工作、已验证范围和安全边界。

> 若新对话处理真实电机，请继续完整阅读 `MOTOR_INTEGRATION_HANDOFF.md`。其中记录了电机职责、FEED/CLEAN 时序、限位开关实测结果、现有状态机接口和接入真实执行器前必须补齐的安全项。

## 1. 工程位置与基本原则

- **实际工程目录**：`C:\Users\cphx\Desktop\rabbit`
- `C:\Users\cphx\Documents\ChatGPT\rabbit` 只是当前聊天的工作区/交接材料，不是完整嵌入式工程。
- MCU：STM32F407VET6；无 RTOS，裸机 HAL + CMake。
- ESP32-S3 保持旧项目中已验证的网站、配网、NVS、MQTT 行为；本轮只替换其下游 SPI 通信，不得擅自改写网站↔ESP32 协议。
- 工作树本来就有用户的未提交变更。开始改动前先执行 `git status --short`，严禁 `git reset --hard`、`git checkout --` 等覆盖操作。
- 未经用户明确授权，不要烧录固件；不要虚构或发送真实电机控制命令。

## 2. 当前系统架构

```text
网站 / MQTT
    ↓
ESP32-S3（联网、命令验证、MQTT ACK/status、SPI 主机）
    ↓ SPI2，通用任务协议 v3
STM32F407（状态机、安全控制、虚拟/真实执行器）
    ├─ PE2：真实泵继电器输出
    ├─ PA1：Simulation 中的虚拟泵指示灯
    ├─ PE8~PE13：六个常开位置开关输入
    └─ PA2/PA3：为未来 USART2 电机通信预留（协议尚未知）
```

ESP32 把上游任务转换为通用任务编号，经 SPI 下发；STM32 状态机决定安全执行。

| 值 | 任务 |
|---:|---|
| 1 | `FEED` |
| 2 | `CLEAN` |
| 3 | `STOP` |
| 4 | `RESET_FAULT` |
| 5 | `PING` |
| 6 | `ARM` |

## 3. STM32 状态机与 Simulation 行为

状态机已实现：

```text
BOOT_SELF_TEST → IDLE → FEED / CLEAN → STOPPING → IDLE
                     ↘ FAULT
```

- `App_Init()` 先确保泵安全关闭，再启动位置开关、链路、自检与 SPI。
- `STOP` 解除 armed 状态并进入安全收尾；`RESET_FAULT` 用于恢复故障状态；未 ARM 时不接受执行任务。
- `Debug`（生产语义）仍拒绝缺少真实电机定义的 `FEED`/`CLEAN`，不会猜测串口控制协议。
- `Simulation` 用于当前没有电机时的端到端本地演练：
  - `FEED` 依次请求电机1左行、等左限位、电机2下行、等左下限位、虚拟奶泵2秒、电机2上行、等左上限位；
  - `CLEAN` 依次请求电机1右行、等右限位、电机2下行、等右下限位、虚拟清洗液泵3秒、虚拟水泵5秒、电机2上行、等右上限位；
  - 每个限位等待最多30秒；超时停止请求并进入 `FAULT`；
  - 约 **300 ms** 的安全收尾；
  - **绝不驱动 PE2**，也**不发送真实电机控制命令**。
- 旧 CAN1 初始化、自检、中断和构建依赖已退出；PD0/PD1 已释放。USART2 尚未配置，PA2/PA3 保留给后续电机协议实现。

六个位置开关使用 PE8~PE13、输入上拉、触点闭合接地，软件将按下状态转换为 1，并做 20 ms 消抖。位定义和 J3 接线见 `LIMIT_SWITCH_WIRING.md`。
当前任务阶段、电机方向请求和泵通道请求记录在 `AppRuntime` 中；Simulation 只更新这些诊断请求并用 PA1 表示虚拟泵阶段，不驱动电机或泵。Production/Debug 仍拒绝 `FEED`/`CLEAN`，等待独立的串口电机和双泵驱动实现。

## 4. ESP32 网关现状

ESP32 文件在 `ESP32/`：

- `ESP32.ino`：Arduino 草图入口；
- `esp32_gateway.cpp`：实际网关实现；
- `FocLinkProtocol.h`：SPI v3 协议；
- `GatewayValidation.h`：保留旧网关验证/NVS/MQTT 命令结构；
- `README.md`：构建和串口测试说明。

保留并已编译验证的旧网关兼容内容：

- NVS `rabbitnet/config` blob 布局；
- 配网页面/AP 行为；
- MQTT topics：`rabbit/<device_id>/cmd`、`ack`、`status`；
- `command`、可选 `request_id` / `session_id`、会话/去重、5 秒 lease 与 LOCAL 行为；
- 保存诊断：`NET_SAVE_OK`、`NET_SAVE_FAIL stage=...`；
- 为兼容网站继续使用 ACK result：`foc_pong`、`foc_accepted`、`foc_rejected`、`foc_timeout`。

注意：这些 `foc_*` 文本只是为了保持网站兼容，ESP32 下游不再发送 FOC 专用 SPI 命令。状态消息新增 STM32 状态、任务、自检与 fault 字段，**不伪造旧 FOC RPM/编码器遥测**。ESP32 GPIO4/5 的旧继电器控制没有保留，泵始终由 STM32 PE2 管理。

## 5. STM32 ↔ ESP32 接线与协议

ESP32 是 SPI 主机，STM32 是 SPI2 从机。

| ESP32-S3 | STM32F407 | 信号 |
|---|---|---|
| GPIO15 | PB12 | NSS / CS |
| GPIO13 | PB13 | SCK |
| GPIO12 | PB15 | MOSI |
| GPIO14 | PB14 | MISO |
| GND | GND | 必须共地 |

- SPI Mode 0，100 kHz，MSB first。
- 固定 64 字节帧，CRC16，协议版本 **v3**。
- 两板分别 USB 供电时，只接上表信号和 GND；**不要互接 3.3 V 或 5 V**。
- STM32 从机不会自行产生时钟，SPI 自检不能以本机回环替代真实 ESP32 主机。

## 6. 构建与烧录状态

已通过以下构建与检查：

```powershell
cmake --preset Debug
cmake --build --preset Debug
cmake --preset SelfTest
cmake --build --preset SelfTest
cmake --preset Simulation
cmake --build --preset Simulation
git diff --check
```

STM32 ELF：

```text
build\Debug\rabbit.elf
build\SelfTest\rabbit.elf
build\Simulation\rabbit.elf
```

ESP32 使用 Arduino CLI：

```text
D:\arduino-cli_1.5.2-rc.1_Windows_64bit\arduino-cli.exe
ESP32 core: esp32:esp32 3.3.4-cn
Library: PubSubClient 2.8
```

成功编译的 ESP32 镜像：`build\ESP32\ESP32.ino.bin`（约 952 KB，应用分区约 30%，RAM 约 14%）。

截至本次交接，实际硬件烧录状态为：

- ESP32：已烧录到 **COM11**（CH343 USB-Enhanced-SERIAL），MAC `7c:4f:ad:21:19:ec`；串口 `STATUS` 曾确认 `config=1`，即旧 NVS 网络配置保留。
- STM32：已用 ST-Link 成功烧录并校验 **Simulation** 固件；目标为 F405/F407 family，Device ID `0x413`，目标电压约 3.18 V。当前不是 Debug 固件。

## 7. 已完成的真实端到端验证

在 ESP32 COM11（115200）和已连接的两板 SPI 上，已发送：

```text
PING
ARM
FEED
```

收到：

```text
PING → foc_pong
ARM → foc_accepted
FEED → foc_accepted
```

等待约 4.3 秒后，状态为：

```text
state=IDLE
fault=0x00000000
```

随后发送 `STOP`、`STATUS`，最终确认：

```text
armed=0
link=verified
state=1 (IDLE)
task=0
fault=0x00000000
```

这证明 ESP32 本地串口模拟上游任务 → SPI → STM32 Simulation 状态机 → 应答/回到 IDLE 的链路可用。未以视觉方式确认 PA1 灯的亮灭；真实泵未接入，也没有被驱动。

## 8. 当前可重复的本地测试

确保两板按第 5 节接好且 STM32 为 Simulation 后，在 ESP32 的 COM11、115200 串口依次发送：

```text
STATUS
PING
ARM
FEED
STATUS
STOP
```

可用 `CLEAN` 替代 `FEED` 重复验证。发送 `FEED` 后约 1.5–3.5 秒观察 PA1 的虚拟泵灯。当前阶段不要给真实泵供电，也不要接入/尝试驱动真实电机。

## 9. 未完成工作与注意事项

1. 电机改用串口控制，但 USART、波特率、帧格式与反馈策略尚未在本工程实现；不得猜测。
2. `FEED` / `CLEAN` 的 Simulation 业务时序已经定义并完成 FEED 实测，但真实电机停止确认、限位组合、各轴超时、互锁和故障策略仍未补齐；详见 `MOTOR_INTEGRATION_HANDOFF.md`。
3. 实际泵继电器/泵的供电、极性与现场安全性尚未验证；未来应先单独检查 PE2/继电器，再接泵。
4. 网站↔ESP32 历史上已能通信；若当前 Wi-Fi/MQTT 显示未连接，只代表当时网络状态，不能据此改动既有网关逻辑。
5. `SelfTest`、`Debug`、`Simulation` 的安全策略不同。演示必须使用 Simulation；准备真实执行器前必须重新审查并使用生产语义固件。
6. 更新 ESP32 代码时优先对照旧仓库的新版本，只合并网站/配置/MQTT 相关的确认改动，不得回退 SPI v3 或恢复 FOC 专用下游命令。

## 10. 新对话的推荐起点

1. 打开 `C:\Users\cphx\Desktop\rabbit`，检查本文件、`git status --short`、当前 CMake preset 和实际连接硬件。
2. 若继续本地演练：按第 8 节测试，记录串口 `STATUS` 与 PA1 现象。
3. 实现电机时：完整阅读 `MOTOR_INTEGRATION_HANDOFF.md`，取得驱动器串口完整文档，再制定 STM32 的真实电机适配层，绝不直接套用 FOC 命令。
4. 每一次改动后至少重新构建目标 preset；凡涉及硬件必须区分“编译通过”“串口/SPI 联调通过”“执行器实测通过”。
