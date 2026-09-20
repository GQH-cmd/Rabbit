# GCC 启动回归测试

`startup_reset_test.py` 使用 Unicorn 执行实际烧录 BIN 中的 Cortex-M 启动指令，
使用同名 ELF 提供符号地址，并逐段核对 ELF/BIN 一致性。不会把 ELF 的 `.data`
预加载进 RAM，否则会掩盖启动文件漏复制的问题。

## 运行

在工程根目录使用 PowerShell（已安装 Python 3.12 和项目使用的 Arm GCC）：

```powershell
$deps = Join-Path $env:TEMP 'foc-startup-test-deps'
py -3.12 -m pip install --target $deps -r .\Tests\requirements-startup.txt
$env:PYTHONPATH = $deps
& .\build-gcc\esp32_spi\build.ps1
& .\build-gcc\esp32_uart\build.ps1
py -3.12 .\Tests\startup_reset_test.py
```

只测试一个配置：

```powershell
py -3.12 .\Tests\startup_reset_test.py --elf .\build-gcc\esp32_spi\F407FOCtest4_gcc_spi.elf
```

默认测试两个配置；每种配置使用 00/A5/5A/FF 四种初始 RAM 内容，各执行一次
启动和一次保留脏 RAM 的复位，共 16 次仿真启动。检查内容：

- BIN 与 ELF 的 Flash 内容、复位向量和栈指针一致。
- 进入 `__libc_init_array` 前，`.data` 等于 Flash 初值且 `.bss` 全零。
- 实际调用 `SystemInit` 和 C 运行时并到达 `main` 入口。
- `.bss` 后方保护区不被清零循环越界覆盖。

测试在 `main` 入口停止，不执行 STM32 外设或 FOC 控制，不访问串口或调试器。
所以通过不代表真实 I2C、SPI、USART、电机和机械动作已经通过验收。

## 本次缺陷的验证记录

- 旧版 `F407FOCtest4_stop65_both_linked_100ms.elf/.bin` 通过本测试。
- 修复前 SPI/UART 产物合计出现 30 个 RAM 初值断言失败。
- 修复两份 `startup_stm32f407xx_gcc.s` 并重新构建后，两配置全部通过。
- 原有 `contact_detect_test.c` 的 4 项主机测试也通过。
- Core、Hardware 和 `.ioc` 的 53 个文件修改前后 SHA-256 一致。
