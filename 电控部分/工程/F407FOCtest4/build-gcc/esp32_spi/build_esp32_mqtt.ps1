param([string]$ArduinoCli = '')
$ErrorActionPreference = 'Stop'
if (-not $ArduinoCli) {
    $ArduinoCli = 'E:\新建文件夹\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
}
if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "arduino-cli not found: $ArduinoCli"
}
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,FlashMode=dio,CDCOnBoot=default,USBMode=hwcdc,UploadMode=default,UploadSpeed=115200,CPUFreq=80'
$sketch = Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt'
$build = Join-Path ([IO.Path]::GetTempPath()) 'rabbit_esp32_spi_mqtt_v1'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$log = Join-Path $build 'compile.log'
Write-Output "FQBN: $fqbn"
Write-Output "Build directory: $build"
& $ArduinoCli compile --fqbn $fqbn --jobs 4 --warnings all --build-path $build $sketch *> $log
$exitCode = $LASTEXITCODE
Get-Content -LiteralPath $log
if ($exitCode -ne 0) { throw "Arduino MQTT compile failed ($exitCode)" }
Write-Output 'Compile completed. NO hardware was flashed.'
