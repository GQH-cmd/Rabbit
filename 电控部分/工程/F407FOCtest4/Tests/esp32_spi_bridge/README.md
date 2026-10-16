# ESP32 SPI bridge 主机回归

运行实际草图 `esp32_spi_bridge.ino`，仅替换 Arduino 串口、时钟、GPIO 和 SPI 硬件 API。
测试不连接串口、不烧录、不操作电机。主机测试不能证明真实 SPI 时序、供电或接线正确。

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\Tests\esp32_spi_bridge\run.ps1' -Compiler 'C:\msys64\mingw64\bin\g++.exe'
```

2026-09-19 验证结果：19 passed; 0 failed。覆盖：

- 独立 CRC 参考实现和标准向量；帧头、长度、CRC 和零填充。
- 上电不自动发送；引脚和 SPI Mode 0 / 100 kHz。
- ARM 本机放行；STOP 发送零开环请求并锁定输入。
- 两电机字段显式模式；数字格式、NaN/Inf/超范围拒绝。
- CR/LF/CRLF、超时残包、过长输入、二进制字符丢弃。
- 28 字节载荷边界；序号及 millis 回绕；发送间隔。
- SPI 初始化失败阻止发送；空闲不重发电机命令。

产物写入 `%TEMP%\rabbit_esp32_spi_host_tests`。ESP32 交叉编译另用：

```powershell
& 'E:\养兔子\FOC_all\3_F407FOCtest4_new\build-gcc\esp32_spi\build_esp32.ps1' -Connection COM
```

COM 配置已通过 Espressif 3.3.11 的实际交叉编译；程序321892字节，静态RAM22848字节。
物理验收：先在 ESP32 串口发 PING，检查 FOC USB1 串口是否收到匹配序号的 SPI3 PONG。
SPI TX 日志只是发出记录，不是 FOC ACK。

## v7.1 LOCAL 模式回归（2026-10-14）

`run_gateway.ps1` 提取生产 `GatewayApp.h` 的真实执行函数，以主机时钟/GPIO/SPI及AP/HTTP边界夹具运行；不连接实物。

- 未配置网络时，配网模式仍拒绝 FEED；只有本机 LOCAL 可以显式退出。
- LOCAL 关闭 AP/HTTP，保持两路输出关闭、不发送运动指令，并要求重新 PING。
- 网络仍在线时也拒绝远程 ARM/FEED/速度/KEEPALIVE；有效远程 STOP 保持优先。
- 解锁、泵测试、SPI探测、未决回执和待重启状态拒绝切换；AP关闭失败不会错误放行 FEED。
- 无网络完整运行真实网关的下降→新触碰→STOP回执→连续停稳→奶泵3000ms→关闭流程。

统一回归入口：`E:\养兔子\FOC_all\3_F407FOCtest4_new\Tests\verify_feed_stage.ps1`。主机通过不代表已烧录或实机通过。

## v7.2 FEED 触发帧诊断回归（2026-10-14）

- 注入模式不符或横向转速超限的原始反馈，紧接着让 STOP 交换回传正常状态；确认诊断保留保护检查时的模式、转速、sample、时间戳和年龄，而不是被覆盖的停止帧。
- 分别覆盖 `wait_motion` 和 `milk_running`；运行中取消先关输出并请求双轴 STOP，再打印 `FEED_ABORT`，没有删保护或抬高65 RPM门槛。
- 运行真实生产 `serviceFeed()`，模拟数据只来自时钟/GPIO/SPI边界夹具，不连接实物、不发送真实电机或开泵命令。
