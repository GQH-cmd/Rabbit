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
