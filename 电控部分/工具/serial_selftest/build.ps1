$ErrorActionPreference = 'Stop'

$gccRoot = 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin'
$gcc = Join-Path $gccRoot 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $gccRoot 'arm-none-eabi-objcopy.exe'
$size = Join-Path $gccRoot 'arm-none-eabi-size.exe'

$proj = 'E:\rabbit\serial_selftest'
$src = @(
  "$proj\main.c",
  "$proj\gpio.c",
  "$proj\usart.c",
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Core\Src\system_stm32f4xx.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_cortex.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash_ex.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash_ramfunc.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_gpio.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr_ex.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc_ex.c',
  'E:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_uart.c',
  'E:\rabbit\ARM\PACK\Keil\STM32F4xx_DFP\2.9.0\Drivers\CMSIS\Device\ST\STM32F4xx\Source\Templates\gcc\startup_stm32f407xx.S'
)

$includes = @(
  "-I$proj",
  '-IE:\rabbit\FOC_all\3_F407FOCtest4_new\Core\Inc',
  '-IE:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Inc',
  '-IE:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
  '-IE:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\CMSIS\Device\ST\STM32F4xx\Include',
  '-IE:\rabbit\FOC_all\3_F407FOCtest4_new\Drivers\CMSIS\Include'
)

$common = @(
  '-mcpu=cortex-m4',
  '-mthumb',
  '-mfpu=fpv4-sp-d16',
  '-mfloat-abi=hard',
  '-DSTM32F407xx',
  '-DUSE_HAL_DRIVER',
  '-ffunction-sections',
  '-fdata-sections',
  '-Wall',
  '-Wextra',
  '-O1',
  '-g3'
)

$elf = Join-Path $proj 'serial_selftest.elf'
$bin = Join-Path $proj 'serial_selftest.bin'
$map = Join-Path $proj 'serial_selftest.map'
$ld = Join-Path $proj 'stm32f407vetx_flash.ld'

& $gcc @common @includes $src '-T' $ld '--specs=nano.specs' '--specs=nosys.specs' '-Wl,--gc-sections' "-Wl,-Map,$map" '-o' $elf
& $objcopy -O binary $elf $bin
& $size $elf
Write-Host "ELF=$elf"
Write-Host "BIN=$bin"
