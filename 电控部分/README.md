# 电控部分

这是兔子自动喂奶系统的 STM32 电控资料，单独放在 `电控部分/`，不覆盖仓库原有的 Python 兔子管理程序。

## 已上传内容

- `工程/F407FOCtest4/`：STM32F407VET6 FOC 主工程，包含 C 源码、CubeMX `.ioc`、Keil 工程、HAL/CMSIS 依赖和硬件驱动。
- `算法/2_Matlab_BLDCM/`：BLDC/FOC MATLAB 模型（已排除 Simulink 自动生成缓存）。
- `界面资源/3_ui/`：界面和开机 Logo 资源。
- `硬件资料/`：板卡图片、原理/PCB 资料、使用说明。
- `工具/serial_selftest/`：串口自测源代码（已排除编译输出）。
- `文档/`：阶段总结、设计记录和测试数据。
- `固件/`：只保留原始恢复固件、历史联动测试固件和当前配套 SPI v2 固件；其他历史控制 bin 全部排除。

## 当前硬件和烧录

- 主控：STM32F407VET6。
- STM32CubeProgrammer 连接 ST-LINK，接口 SWD。
- 烧录起始地址：`0x08000000`。
- `固件/F407FOCtest4_original.bin`：原始恢复基线。
- `固件/F407FOCtest4_stop65_both_linked_500ms.bin`：最后的双电机联动测试版。它不是单电机最终版，当前只有一个电机时不要直接拿它当最终程序。
- `固件/F407FOCtest4_gcc_spi_duplex.bin`：当前 STM32F407 SPI v2 配套固件，烧录地址为 `0x08000000`。它必须与同目录工程里的 ESP32-S3 网关源码配套使用。
- `固件/F407FOCtest4_gcc_spi_duplex.bin.sha256`：当前固件校验值。

## 当前状态

- 已实现 FOC 电机控制、编码器读取、串口控制和接触检测测试链路。
- 当前串口测试使用 USB/USART1，常用参数为 `115200 8N1`。
- 当前已加入 ESP32-S3 → SPI3 → STM32F407 的双向协议 v2：64 字节帧、CRC16、命令 ID、FOC ACK、转速/电流/接触状态遥测。
- 当前已加入 ESP32 MQTT 网关源码、配网页面、NVS 配置、租约停止和构建/上传脚本。ESP32 固件需要在 `工程/F407FOCtest4/build-gcc/esp32_spi/` 中重新编译生成，未把编译缓存和旧版 BIN 上传。
- `Tests/esp32_spi_bridge/` 包含协议、网关、STM32 通信模拟和异常帧测试；测试不等于真实电机、供电或接线验收。
- 计划中的定时喂奶流程仍是：定时启动 → 低速下降 → 接触检测 → 记录位置 → 执行喂奶 → 反向返回相同距离；网络下发和实机联调还需要继续验证。

## 当前通信接线

| ESP32-S3 | STM32 FOC |
|---|---|
| GPIO15 | PA15 / SPI3 CS |
| GPIO13 | PC10 / SPI3 SCK |
| GPIO14 | PC11 / SPI3 MISO |
| GPIO12 | PC12 / SPI3 MOSI |
| GND | GND |

SPI 为 Mode 0、100 kHz。两块板独立供电时只连接信号线和 GND，不把 ESP32 的电源脚接到 FOC 电源。

## 从源码重建

在 `工程/F407FOCtest4/` 下运行：

```powershell
.\build-gcc\esp32_spi\build.ps1
```

STM32 输出为 `build-gcc/esp32_spi/F407FOCtest4_gcc_spi_duplex.bin`；ESP32 网关使用同目录的 `build_esp32_mqtt_provisioned.ps1`，源码在 `esp32_spi_bridge_mqtt_provisioned/`。先编译、核对日志和校验值，再烧录；不要把 `.c` 文件直接交给 Programmer。
- **当前方案不接 USART2**：ESP32-S3 自己负责 Wi-Fi/MQTT，ESP32 与 FOC 通过 SPI3 通信，按上面的 SPI 接线表连接即可。USART1/USB1 仍保留给 XCOM 调试和手动控制。
- 旧的 USART2 接法（STM32 `PD5/TX` → 模块 `RX`、`PD6/RX` ← 模块 `TX`）只适用于改用 UART 无线模块的备用方案，不属于当前 ESP32-S3 + SPI 版本；不要把两套方案混接。
