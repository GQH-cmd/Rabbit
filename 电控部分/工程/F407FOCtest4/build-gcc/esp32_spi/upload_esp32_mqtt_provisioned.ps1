[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^COM[1-9][0-9]*$')][string]$Port,
    [Parameter(Mandatory = $true)][string]$ReleaseDirectory,
    [string]$ArduinoCli = 'E:\新建文件夹\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe',
    [string]$ConfigFile = ''
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw "arduino-cli not found: $ArduinoCli" }
if ($ConfigFile -and -not (Test-Path -LiteralPath $ConfigFile)) { throw "Arduino CLI config not found: $ConfigFile" }
$release = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$manifest = Get-Content -LiteralPath (Join-Path $release 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.do_not_flash -eq $true -or ($manifest.PSObject.Properties['release_valid'] -and $manifest.release_valid -ne $true)) {
    throw 'This intermediate release was marked invalid. Select the final verified release.'
}
if ($manifest.connection -ne 'COM' -or $manifest.fqbn -notlike 'esp32:esp32:esp32s3:*') {
    throw 'This upload script requires an ESP32-S3 COM release manifest.'
}
$required = @(
    'esp32_spi_bridge_mqtt_provisioned.ino.bin',
    'esp32_spi_bridge_mqtt_provisioned.ino.bootloader.bin',
    'esp32_spi_bridge_mqtt_provisioned.ino.partitions.bin',
    'boot_app0.bin'
)
foreach ($name in $required) {
    $entry = @($manifest.files | Where-Object { $_.name -eq $name })
    if ($entry.Count -ne 1) { throw "Missing or duplicate manifest entry: $name" }
}
foreach ($entry in $manifest.files) {
    if ([IO.Path]::GetFileName($entry.name) -ne $entry.name) { throw 'Invalid manifest filename.' }
    $file = Join-Path $release $entry.name
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Firmware hash mismatch: $($entry.name)"
    }
}
# 只上传已验证的文件，不再次编译。务必先断开 FOC，并关闭占用端口的串口监视器。
$sketch = Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt_provisioned'
Write-Output "ESP32-S3 COM firmware: $release"
Write-Output 'Disconnect FOC wiring and close serial monitors before upload.'
if ($PSCmdlet.ShouldProcess($Port, 'Upload verified ESP32-S3 gateway firmware (no compilation)')) {
    $arguments = @('upload','--fqbn',$manifest.fqbn,'--port',$Port,'--input-dir',$release,'--verify')
    if ($ConfigFile) { $arguments += @('--config-file',$ConfigFile) }
    $arguments += $sketch
    & $ArduinoCli @arguments
    if ($LASTEXITCODE -ne 0) { throw "Upload failed: $LASTEXITCODE" }
    Write-Output 'Upload completed. Open the COM serial monitor at 115200 baud, then press RST.'
}
