param(
    [string]$ArduinoCli = '',
    [string]$BuildRoot = 'E:\RabbitBuild'
)
$ErrorActionPreference = 'Stop'
if (-not $ArduinoCli) {
    $ArduinoCli = 'E:\新建文件夹\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
}
if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "arduino-cli not found: $ArduinoCli"
}
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,FlashMode=dio,CDCOnBoot=default,USBMode=hwcdc,UploadMode=default,UploadSpeed=115200,CPUFreq=80'
$sketch = Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt_provisioned'
$canonical = Join-Path $PSScriptRoot '..\..\Hardware\FocLinkProtocol.h'
if ((Get-FileHash -LiteralPath $canonical).Hash -ne (Get-FileHash -LiteralPath (Join-Path $sketch 'FocLinkProtocol.h')).Hash) {
    throw 'Protocol header mismatch. Synchronize Hardware/FocLinkProtocol.h into the sketch first.'
}
$running = Get-CimInstance Win32_Process | Where-Object {
    $_.Name -eq 'arduino-cli.exe' -and $_.CommandLine -match '\bcompile\b'
}
if ($running) { throw 'Another Arduino compile is running. Wait for it to finish first.' }
$memory = Get-CimInstance Win32_OperatingSystem
if ($memory.FreeVirtualMemory -lt 1572864) {
    throw 'Available committed memory is below 1.5 GiB. Close unused apps before compiling.'
}
# 仅缩短输出路径；不覆盖 SDK 或编译器路径，避免破坏核心包的 Windows COPY 命令。
$build = Join-Path $BuildRoot 'verified-com'
New-Item -ItemType Directory -Path $build -Force | Out-Null
# 日志置于构建目录外；Arduino 清理缓存时不能删除正在写入的日志。
$logDir = Join-Path $BuildRoot 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('esp32-duplex-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
Write-Output "FQBN: $fqbn"
Write-Output "Build directory: $build"
# 单线程编译，降低电脑内存占用；不影响生成的固件功能。
& $ArduinoCli compile --fqbn $fqbn --jobs 1 --warnings all --verbose --build-path $build $sketch *> $log
$exitCode = $LASTEXITCODE
Get-Content -LiteralPath $log
if ($exitCode -ne 0) { throw "Arduino provisioned MQTT compile failed ($exitCode)" }
$app = Join-Path $build 'esp32_spi_bridge_mqtt_provisioned.ino.bin'
if (-not (Test-Path -LiteralPath $app)) { throw 'Compiler returned success but application BIN is missing.' }
# 只有成功编译才创建交付目录，防止把失败构建的旧 BIN 误当成最新版。
$release = Join-Path $PSScriptRoot ('release_mqtt_provisioned_com_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $release | Out-Null
Get-ChildItem -LiteralPath $build -File | Where-Object {
    $_.Extension -in @('.bin', '.elf') -or $_.Name -eq 'partitions.csv'
} | Copy-Item -Destination $release
Copy-Item -LiteralPath $log -Destination (Join-Path $release 'compile.log')
$files = Get-ChildItem -LiteralPath $release -File | Where-Object { $_.Extension -in @('.bin', '.elf') } | ForEach-Object {
    [pscustomobject]@{ name = $_.Name; bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
}
$sources = Get-ChildItem -LiteralPath $sketch -File | Where-Object { $_.Extension -in @('.ino', '.h') } | ForEach-Object {
    [pscustomobject]@{ name = $_.Name; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
}
[pscustomobject]@{
    built_at = (Get-Date -Format o); fqbn = $fqbn; connection = 'COM'; flashed = $false
    source = $sketch; files = @($files); sources = @($sources)
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $release 'manifest.json') -Encoding UTF8
Write-Output 'Compile completed. NO hardware was flashed.'
Write-Output "Release directory: $release"
