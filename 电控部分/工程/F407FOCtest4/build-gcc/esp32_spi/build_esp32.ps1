param(
    [ValidateSet('COM', 'USB')][string]$Connection = 'COM',
    [string]$ArduinoCli = ''
)
$ErrorActionPreference = 'Stop'
# Compile only. This script NEVER flashes a device or opens a serial port.
if (-not $ArduinoCli) {
    $found = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($found) { $ArduinoCli = $found.Source }
}
if (-not $ArduinoCli) {
    $keys = @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
              'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*')
    $apps = Get-ItemProperty $keys -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -like 'Arduino IDE*' }
    foreach ($app in $apps) {
        if (-not $app.DisplayIcon) { continue }
        $icon = ($app.DisplayIcon -replace ',\d+$', '').Trim('"')
        $candidate = Join-Path (Split-Path $icon) 'resources\app\lib\backend\resources\arduino-cli.exe'
        if (Test-Path -LiteralPath $candidate) { $ArduinoCli = $candidate; break }
    }
}
if (-not $ArduinoCli -or -not (Test-Path -LiteralPath $ArduinoCli)) {
    throw 'arduino-cli not found. Pass -ArduinoCli with its full executable path.'
}
$cdc = if ($Connection -eq 'COM') { 'default' } else { 'cdc' }
$fqbn = "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,FlashMode=dio,CDCOnBoot=$cdc,USBMode=hwcdc,UploadMode=default,UploadSpeed=115200,CPUFreq=80"
$build = Join-Path ([IO.Path]::GetTempPath()) ("rabbit_esp32_spi_v2\" + $Connection.ToLowerInvariant())
New-Item -ItemType Directory -Path $build -Force | Out-Null
$log = Join-Path $build 'compile.log'
# Replace stale output at the start, even while Arduino compiles silently.
Set-Content -LiteralPath $log -Value "Building $fqbn" -Encoding UTF8
Write-Output "FQBN: $fqbn"
Write-Output "Build directory: $build"
& $ArduinoCli compile --fqbn $fqbn --jobs 4 --warnings all --build-path $build (Join-Path $PSScriptRoot 'esp32_spi_bridge') 2>&1 |
    Tee-Object -FilePath $log
if ($LASTEXITCODE -ne 0) { throw "Arduino compile failed ($LASTEXITCODE); see $log" }
Write-Output 'Compile completed. NO hardware was flashed.'
