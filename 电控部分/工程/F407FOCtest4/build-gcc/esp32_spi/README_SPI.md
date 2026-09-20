# ESP32-S3 ↔ FOC SPI 联调

## 文档状态

> 本页保留的是早期“电脑串口 → ESP32 → SPI3 → FOC”的手动联调记录，使用旧的 32 字节测试协议。
> 当前提交使用的是 `esp32_spi_bridge_mqtt_provisioned/README_MQTT.md`：
> 网站后端 → MQTT → ESP32-S3 → SPI3 → STM32 FOC，使用 SPI 双向协议 v2（64 字节、ACK、遥测）。
> 当前 ESP32-S3 无线方案不接 USART2；USART2 仅是改用 UART 无线模块时的备用方案。

## 2026-09-19：早期 ESP32-S3 手动 SPI 联调程序

本节记录早期手动联调程序；当前 MQTT 网关源码和配网流程见上方指定文档。

打开这个文件，旁边的 `FocSpiProtocol.h` 必须保留在同一文件夹：

```text
E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\esp32_spi_bridge\esp32_spi_bridge.ino
```

### Arduino IDE 设置与烧录

- 开发板包：`esp32 by Espressif Systems`，本机使用 **3.3.11**。
- Board：**ESP32S3 Dev Module**；Flash Size：**16MB**；PSRAM：**OPI PSRAM**。
- Flash Mode：**DIO 80MHz**；CPU Frequency：**80MHz**。
- Partition Scheme：**16M Flash (3MB APP/9.9MB FATFS)**（内部选项 `app3M_fat9M_16MB`）。
- Upload Speed：**115200**；Upload Mode：**UART0 / Hardware CDC**。
- 优先用板子标注 **COM** 的 USB 口：**USB CDC On Boot = Disabled**。
- 若用板子标注 **USB** 的原生 USB 口：**USB CDC On Boot = Enabled**，
  USB Mode 选 **Hardware CDC and JTAG**；换这个选项后需要重新编译上传。
- 选择拔插 ESP32 后出现/消失的 COM 端口，不固定写成 COM7。
- 烧录时 ESP32 单独接电脑，先拔掉与 FOC 的所有连接。一个 USB 口供电即可。
- 上传完成打开串口监视器，**115200 baud / New Line**；必要时按 RST。
  同一个串口不要同时被 Arduino 监视器和 XCOM 占用。

原生 USB 与 COM 的 CDC 设置区别见
[Espressif USB 指南](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html)。

### 第一次联调顺序

1. ESP32 单独上传后，串口发送 `STATUS`。这是本机状态，不会给 FOC 发电机指令。
2. 两块板全部断电，按下表核对 **SPI3** 插座的信号和针号；不能凭线颜色或上下方向接。
3. 只连接四根 SPI 信号线和 GND；两块板各自供电，FOC 的 3.3V/5V 不接 ESP32。
4. 先让 FOC 完成启动。FOC 应运行本页记录的 SPI 启动修复固件；若已经是这个固件，
   本次不要求重烧 FOC。当前板内究竟是哪一个固件仍需实际确认。
5. ESP32 监视器手动发送 `PING`。FOC 的 **USB1/XCOM** 打开另一个串口，115200、8N1。
   ESP32 显示 `SPI TX seq=0 ... ACK=UNAVAILABLE` 后，FOC 应出现 `SPI3 PONG seq=0`。
   **必须是 FOC 的匹配序号日志，才能说明该测试帧被接收并处理。**
6. 机构脱离动物和负载，确认 PING 正常后才发送 `ARM`，再输入此前已经实测适合电机的参数。
   `STOP` 会发一次 `o0,o0` 并关闭 ESP32 的输入放行；必须观察 FOC 和实际电机确认停止。

### 新版命令

| 输入 | 动作 |
|---|---|
| `HELP` / `?` | 显示帮助，不走 SPI |
| `STATUS` | 显示 ESP32 本机状态，不表示 FOC 在线 |
| `PING` | 发一次无电机动作的测试帧 |
| `ARM` | 本机允许后续电机命令，本身不发 SPI、不启动电机 |
| `STOP` / `DISARM` | 发一次 `o0,o0` 并关闭本机放行；不是硬件急停 |
| `v30,o0` | **仅格式示例**：M0 速度目标 30 RPM，M1 零开环参数；先 ARM |

`o/c/v/p` 仍是原 FOC 的开环/电流/速度/位置命令；逗号前后都必须写模式。
旧例子 `v30,0` 的后半段并不是明确停止 M1，新版直接拒绝这种写法。
**65 RPM 是原接触检测阈值，不是最高转速；30 RPM 示例可能触发原停机逻辑，不能拿它
作为“必定能转”的验收参数。** `p0` 也不是通用机械回零，当前未实现找原点功能。

### 验证与局限

- 2026-09-19，使用 Espressif 3.3.11、S3 N16R8、COM/CDC Disabled 配置实际编译通过：
  程序 321892 / 3145728 字节，静态 RAM 22848 / 327680 字节；构建退出码为 0。
  原生 USB 的设置列作切换说明，本次验收的构建是 **COM 版本**。
- 本机 C++ 回归测试目前 **19 项通过**：CRC、固定帧、ARM、输入校验、边界长度、
  CR/LF、超时丢弃、序号回绕、帧间隔、初始化失败、上电/空闲不自动发包。
- SPI Mode 0，100 kHz，32 字节定长，发送间隔至少 100 ms，无自动重发。
- 本节旧版程序的 FOC 只有 SPI 接收逻辑，**没有 SPI 回传 ACK 协议**；当前 v2 固件已经加入 ACK 和遥测，不能把本节旧结论套到新版。
  `SPI3 OK` 表示接收后调用了解析，不等于电机一定按目标运行。
- FOC 使用单接收缓冲区，降速和限频只是减少覆盖风险；不是完善的双向可靠协议。
- 上电不自动 PING、不自动启动电机、不重放旧命令；ARM 不是硬件安全联锁。
  拔掉 ESP32 **不会自动使 FOC 停机**，STOP 丢包也没有自动重试，必须保留独立停机手段。
- 不启动 Wi-Fi/蓝牙，循环让出 CPU；这不证明供电正常或板子完好。单独 USB 供电仍迅速
  烫手时停止上电，先排查硬件，不要继续接回 FOC。
- 本次没有自动烧录、发送实机电机命令或完成板级验收。

可重复验证命令（只编译，不烧录）：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\Tests\esp32_spi_bridge\run.ps1' -Compiler 'C:\msys64\mingw64\bin\g++.exe'
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\build_esp32.ps1' -Connection COM
# 原生 USB 版本：将上面的 -Connection COM 改为 -Connection USB
```

编译产物写在 `%TEMP%\rabbit_esp32_spi_v2`，不会在项目里堆叠试验 BIN。
旧 ESP32 程序和旧说明保存在本目录 `backups` 下，时间标记 `20260919_140135`。

## 2026-09-17：GCC 启动初始化修复

本次联调使用 `F407FOCtest4_gcc_spi_startup_fix.bin`，烧录起始地址为
`0x08000000`，文件大小 267372 字节，SHA-256：

```text
6A5C07AB93E8E4756CCD89A7105DA69BC0F032E6817295A6BBF46D90DA017771
```

- 修复 `Reset_Handler` 遗漏的 `.data` Flash→RAM 复制和 `.bss` 清零；
  两步都在 C 构造函数和 `main()` 之前完成。
- USART1/XCOM、SPI3、65 RPM 阈值及原比较表达式保持不变；没有修改
  Core、Hardware 或 CubeMX `.ioc` 配置。
- SPI/UART 两种配置均已重新编译；真实 BIN 的启动指令通过了 4 种初始
  RAM 内容 × 冷启动/保留 RAM 复位检查（合计 16 次启动仿真）。原接触检测
  的 4 项主机单元测试通过。这不是完整板级仿真，也不是实机验收。
- 之前已存在于 `main.c` 的分阶段 BOOT 提示现在已编入 BIN。
- 旧的 `*_single_motor_fix.bin`、`*_fixed.bin`、`*_before_motorfix.bin`
  保留为历史文件，**不要当作这次的启动修复版烧录**。

本次未自动烧录，也未发送电机指令。实机验证时先断开 ESP32，机构脱离动物和
负载；在 XCOM 以 115200、8N1 打开 FOC 的 USB1 串口。烧录校验通过后检查
一次断电重启、一次 RST，记录完整 BOOT 日志。若仍卡住，保留最后一条日志，
不要只靠反复烧录判断。回归测试运行方式见 `../../Tests/README_STARTUP.md`。

## 1. FOC 固件构建

在 PowerShell 执行：

```powershell
Set-Location 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi'
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Clean
```

生成的烧录文件：

```text
E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\F407FOCtest4_gcc_spi.bin
```

用 STM32CubeProgrammer 烧录到 `0x08000000`。这是 SPI 版本，原来的
`build-gcc\esp32_uart\F407FOCtest4_gcc_uart.bin` 不要和它混用。

## 2. 接线

**更正旧说明：使用 M1_SPI3，不是 USB1 侧的 M0_SPI2。**

已重新核对本地 `E:\养兔子\FOC_all\硬件\1_排线_0402版\AD\Altium_FOC12.zip`
内的 `控制板/PCB2.pcbdoc`：SPI3 对应 **FPC5/U9**（USB2 一侧），SPI2 对应
FPC4/U8（USB1 一侧）。若实物板版本/连接器与 CAD 不同，应以实物连通测量确认。
**用户此前说的上下各三个、共 2×3 的 U4 插针是 BOOT 选择，不能接 SPI。**

| SPI3 连接器逻辑针号 / FOC 网络 | ESP32-S3 | 方向 |
|---|---|---|
| 1 / DGND | GND | 共地 |
| 2 / MOSI / PC12 | GPIO12 | ESP32 输出 |
| 3 / MISO / PC11 | GPIO14 | ESP32 输入 |
| 4 / SCK / PC10 | GPIO13 | ESP32 输出 |
| 5 / CS / PA15 | GPIO15 | ESP32 输出 |
| 6 / +3.3V_1 | **不接** | 两块板分别供电 |

这是 CAD 逻辑针号，**不是看照片从左往右/从上往下的顺序**；插座和排线朝向会改变
观察顺序。FPC5 为 1.0 mm 6P FPC，U9 为 1.25 mm GH6 备选封装，实际焊哪种就用
匹配的转接线/转接板再接杜邦线，不要把杜邦线硬塞插座。没找到针 1 前不要通电试接。

两边使用 3.3 V GPIO 逻辑，信号脚不能接 5 V。先不要给电机上高压电。
M1 是接口所属通道的标记，不代表只能控制 M1 电机；帧内仍有 M0、M1 两个字段。

## 3. SPI 帧格式

ESP32 每次 CS 拉低后固定发送 32 字节，SPI mode 0、MSB first：

```text
[0]     0xA5
[1]     sequence
[2]     payload length，1..28
[3..]   ASCII 命令，例如 "v30,o0" 或 "PING"
[3+len] CRC8，计算范围为 [0..2+len]
[后面]  0 填充到 32 字节
```

STM32 校验通过后复用原有 `Parse_Command()`，所以命令格式仍然是：

```text
v30,o0
o0.2,o0
p1.0,o0
```

## 4. ESP32 端

按本页最上方步骤用 Arduino IDE 上传整个 `esp32_spi_bridge` 草图目录。
新版上电**不发任何 SPI 帧**，手动输入 `PING` 联调；电机命令先 `ARM`。
这些参数是协议格式示例，不是适合所有电机的运行参数。

## 注意

当前 SPI 固件已修正两处通信问题：

1. FOC 收到 `PING` 后在 **USB1 串口**打印 `SPI3 PONG`，不会当作电机指令；
   这不是 SPI 回传，新版 ESP32 也不再上电自动 PING。
2. USB1/USART1 的 XCOM 接收改为逐字节中断，不再依赖原来的 DMA 空闲中断路径；原有“低于 65 RPM 判定接触并停机”的逻辑保持不变。

当前可烧录文件：

```text
E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\F407FOCtest4_gcc_spi_startup_fix.bin
```

地址仍为 `0x08000000`。STM32 的 USB1/USART1 控制通道仍保留，可先不接 ESP32 用 XCOM 验证电机。

本版本保留此前的单电机适配代码：启动时逐路探测 AS5600，未接的电机标记为
`ENCODER_DISABLED`。单电机实际运行仍需上板验证，不能仅凭编译通过认定正常。
USB1 的启动标记依次包括 `BOOT USART1`、`BOOT SPI2`、`BOOT USART2`、
`BOOT USB`、`BOOT SPI1`、`BOOT SPI3`、`BOOT TIMERS`、`BOOT UART2RX`、
`BOOT SPI3RX`、`BOOT PRESTART`、`BOOT POSTSTART`、`BOOT CONTACT TEST`。
最后两条仅在对应初始化实际完成后才出现，不能保证每块板都会走完。

当前 FOC 工程配置的 M0/M1 编码器是 AS5600，因此 SPI3 可用于通信。以后如果
把 M1 改成 AS5047，SPI3 会与编码器复用，需要重新分配 CS 或改用 USART。
