param(
    [string]$Compiler = 'C:\msys64\mingw64\bin\g++.exe',
    [string]$PythonLauncher = 'py'
)
# 只做主机回归；不连接串口，不烧录，不操作电机或继电器。
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "主机编译器不存在：$Compiler" }
if (-not (Get-Command $PythonLauncher -ErrorAction SilentlyContinue)) { throw "Python启动器不存在：$PythonLauncher" }
$build = Join-Path ([IO.Path]::GetTempPath()) 'rabbit_gateway_host_tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$project = Split-Path -Parent $PSScriptRoot
$gateway = Join-Path $PSScriptRoot 'esp32_spi_bridge'
[string[]]$pythonArgs = @()
if ([IO.Path]::GetFileNameWithoutExtension($PythonLauncher) -eq 'py') { $pythonArgs += '-3' }
& $PythonLauncher @pythonArgs (Join-Path $PSScriptRoot 'verify_hal_wait_guard.py')
if ($LASTEXITCODE -ne 0) { throw 'HAL等待链主机回归失败' }
& $PythonLauncher @pythonArgs (Join-Path $PSScriptRoot 'verify_as5600_velocity.py')
if ($LASTEXITCODE -ne 0) { throw 'AS5600采样/测速主机回归失败' }
& $PythonLauncher @pythonArgs (Join-Path $PSScriptRoot 'verify_m1_only.py')
if ($LASTEXITCODE -ne 0) { throw '单M1跨控制器主机回归失败' }
& $PythonLauncher @pythonArgs (Join-Path $PSScriptRoot 'verify_encoder_recovery.py')
if ($LASTEXITCODE -ne 0) { throw '编码器停机重检回归失败' }
$pythonArgs += (Join-Path $gateway 'verify_duplex.py')

& (Join-Path $project 'build-gcc\esp32_spi\generate_feed_stage_config.ps1')
$exe = Join-Path $build 'feed_contact_stage_test.exe'
& $Compiler -std=c++17 -Wall -Wextra -Werror (Join-Path $PSScriptRoot 'feed_contact_stage_test.cpp') -o $exe
if ($LASTEXITCODE -ne 0) { throw '触碰供奶状态机测试编译失败' }
& $exe
if ($LASTEXITCODE -ne 0) { throw '触碰供奶状态机测试失败' }

& (Join-Path $gateway 'run_gateway.ps1') -Compiler $Compiler
& (Join-Path $gateway 'run.ps1') -Compiler $Compiler
& $PythonLauncher @pythonArgs
if ($LASTEXITCODE -ne 0) { throw 'SPI / STM32 / 接触保护回归失败' }
Write-Output 'PASS: 全部触碰供奶、网关、继电器与SPI主机回归；未操作硬件。'
