param([switch]$Clean)
$ErrorActionPreference = 'Stop'

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ToolBin = 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin'
$CC = Join-Path $ToolBin 'arm-none-eabi-gcc.exe'
$OBJCOPY = Join-Path $ToolBin 'arm-none-eabi-objcopy.exe'
$SIZE = Join-Path $ToolBin 'arm-none-eabi-size.exe'
$OutDir = $PSScriptRoot
$ObjDir = Join-Path $OutDir 'obj'
$Elf = Join-Path $OutDir 'F407FOCtest4_gcc_spi_duplex.elf'
$Bin = Join-Path $OutDir 'F407FOCtest4_gcc_spi_duplex.bin'
$Hex = Join-Path $OutDir 'F407FOCtest4_gcc_spi_duplex.hex'
$Map = Join-Path $OutDir 'F407FOCtest4_gcc_spi_duplex.map'
$Linker = Join-Path $OutDir 'STM32F407VETx_FLASH_gcc.ld'

if (-not (Test-Path -LiteralPath $CC)) { throw "找不到 GCC: $CC" }
if ($Clean) {
    if (Test-Path -LiteralPath $ObjDir) { Remove-Item -LiteralPath $ObjDir -Recurse -Force }
    foreach ($f in @($Elf,$Bin,$Hex,$Map)) { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }
}
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$Defines = @(
    '-DUSE_HAL_DRIVER', '-DSTM32F407xx', '-DARM_MATH_CM4', '-D__TARGET_FPU_VFP', '-DESP32_SPI_LINK'
)
$IncludeDirs = @(
    'Core\Inc',
    'Drivers\STM32F4xx_HAL_Driver\Inc',
    'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
    'Drivers\CMSIS\Device\ST\STM32F4xx\Include',
    'Drivers\CMSIS\Include',
    'Drivers\CMSIS\DSP\Include',
    'Drivers\CMSIS\DSP\PrivateInclude',
    'Hardware',
    'Middlewares\ST\ARM\DSP\Inc'
) | ForEach-Object { Join-Path $ProjectRoot $_ }
$Includes = @($IncludeDirs | ForEach-Object { '-I' + $_ })
$CpuFlags = @('-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard')
$CFlags = @('-std=gnu11','-O2','-g3','-ffunction-sections','-fdata-sections','-fno-common') + $CpuFlags + $Defines + $Includes

# This is the exact C source set from the Keil target. The Keil-only .lib and main_example.c are intentionally excluded.
$SourceRel = @(
    'Core\Src\main.c',
    'Core\Src\gpio.c',
    'Core\Src\adc.c',
    'Core\Src\can.c',
    'Core\Src\dma.c',
    'Core\Src\i2c.c',
    'Core\Src\spi.c',
    'Core\Src\tim.c',
    'Core\Src\usart.c',
    'Core\Src\usb_otg.c',
    'Core\Src\stm32f4xx_it.c',
    'Core\Src\stm32f4xx_hal_msp.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_adc.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_adc_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_ll_adc.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_rcc_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_flash_ramfunc.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_gpio.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_dma_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_dma.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pwr_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_cortex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_exti.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_can.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_i2c.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_i2c_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_spi.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_tim.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_tim_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_uart.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pcd.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pcd_ex.c',
    'Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_ll_usb.c',
    'Core\Src\system_stm32f4xx.c',
    'Hardware\lcd.c',
    'Hardware\FOC.c',
    'Hardware\AS5600.c',
    'Hardware\Filter.c',
    'Hardware\Controller.c',
    'Hardware\Motor.c',
    'Hardware\ContactDetect.c',
    'Hardware\sin_cos.c',
    'Hardware\led.c',
    'Hardware\AS5047.c',
    # GCC-only startup support kept inside this build profile.
    'build-gcc\esp32_spi\syscalls_gcc.c'
)

$Objects = @()
foreach ($rel in $SourceRel) {
    $src = Join-Path $ProjectRoot $rel
    if (-not (Test-Path -LiteralPath $src)) { throw "源文件不存在: $src" }
    $safe = ($rel -replace '[\\/]','_') -replace '\.c$','.o'
    $obj = Join-Path $ObjDir $safe
    $dep = [IO.Path]::ChangeExtension($obj, '.d')
    $Objects += $obj
    Write-Host "CC  $rel"
    & $CC @CFlags '-MMD' '-MP' ('-MF' + $dep) '-c' $src '-o' $obj
    if ($LASTEXITCODE -ne 0) { throw "GCC 编译失败: $rel (exit=$LASTEXITCODE)" }
}

$Startup = Join-Path $OutDir 'startup_stm32f407xx_gcc.s'
$StartupObj = Join-Path $ObjDir 'startup_stm32f407xx_gcc.o'
Write-Host 'AS  startup_stm32f407xx_gcc.s'
& $CC @CpuFlags '-c' $Startup '-o' $StartupObj
if ($LASTEXITCODE -ne 0) { throw "启动文件汇编失败 (exit=$LASTEXITCODE)" }
$Objects += $StartupObj

Write-Host 'LD  F407FOCtest4_gcc_spi_duplex.elf'
$LinkArgs = @(
    '-nostartfiles', '--specs=nano.specs', '--specs=nosys.specs'
) + $CpuFlags + @(
    ('-T' + $Linker), '-Wl,--gc-sections', '-Wl,--print-memory-usage', ('-Wl,-Map=' + $Map),
    '-Wl,--start-group'
) + $Objects + @(
    '-lc', '-lm', '-lnosys', '-lgcc', '-Wl,--end-group', '-o', $Elf
)
& $CC @LinkArgs
if ($LASTEXITCODE -ne 0) { throw "链接失败 (exit=$LASTEXITCODE)" }

& $OBJCOPY '-O' 'binary' $Elf $Bin
if ($LASTEXITCODE -ne 0) { throw "BIN 转换失败 (exit=$LASTEXITCODE)" }
& $OBJCOPY '-O' 'ihex' $Elf $Hex
if ($LASTEXITCODE -ne 0) { throw "HEX 转换失败 (exit=$LASTEXITCODE)" }

Write-Host 'SIZE'
& $SIZE $Elf
Write-Host "ELF : $Elf"
Write-Host "BIN : $Bin"
Write-Host "HEX : $Hex"
Write-Host "MAP : $Map"

