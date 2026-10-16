param(
    [string]$ArduinoCli = '',
    [string]$BuildRoot = 'E:\RabbitBuild',
    [ValidateRange(1,8)][int]$Jobs = 1,
    [string]$ConfigFile = '',
    [string]$BuildDirectory = ''
)
$ErrorActionPreference = 'Stop'
if (-not $ArduinoCli) {
    $ArduinoCli = 'E:\新建文件夹\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
}
if (-not (Test-Path -LiteralPath $ArduinoCli)) {
    throw "arduino-cli not found: $ArduinoCli"
}
if ($ConfigFile -and -not (Test-Path -LiteralPath $ConfigFile)) { throw "Arduino CLI config not found: $ConfigFile" }
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,FlashMode=dio,CDCOnBoot=default,USBMode=hwcdc,UploadMode=default,UploadSpeed=115200,CPUFreq=80'
$sketch = Join-Path $PSScriptRoot 'esp32_spi_bridge_mqtt_provisioned'
# 业务参数从.env生成，MCU不直接读取电脑文件。0时长/未知下降方向保留禁用。
& (Join-Path $PSScriptRoot 'generate_feed_stage_config.ps1')
$parameterFile = Join-Path $sketch 'feed_stage.env'
$parameterHash = (Get-FileHash -LiteralPath $parameterFile -Algorithm SHA256).Hash
# 编译前冻结源码哈希；编译期间有改动则不交付，防止 BIN 与清单对应不上。
$sources = @(Get-ChildItem -LiteralPath $sketch -File | Where-Object { $_.Extension -in @('.ino', '.h') } | ForEach-Object {
    [pscustomobject]@{ name = $_.Name; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
$canonical = Join-Path $PSScriptRoot '..\..\Hardware\FocLinkProtocol.h'
if ((Get-FileHash -LiteralPath $canonical).Hash -ne (Get-FileHash -LiteralPath (Join-Path $sketch 'FocLinkProtocol.h')).Hash) {
    throw 'Protocol header mismatch. Synchronize Hardware/FocLinkProtocol.h into the sketch first.'
}
$axesConfig = Join-Path $PSScriptRoot '..\..\Hardware\MotorAxesConfig.h'
if ((Get-FileHash -LiteralPath $axesConfig).Hash -ne (Get-FileHash -LiteralPath (Join-Path $sketch 'MotorAxesConfig.h')).Hash) {
    throw 'Motor axes config mismatch. Synchronize Hardware/MotorAxesConfig.h into the sketch first.'
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
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $BuildRoot 'verified-com' }
New-Item -ItemType Directory -Path $build -Force | Out-Null
# 日志置于构建目录外；Arduino 清理缓存时不能删除正在写入的日志。
$logDir = Join-Path $BuildRoot 'logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$log = Join-Path $logDir ('esp32-duplex-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')
Write-Output "FQBN: $fqbn"
Write-Output "Build directory: $build"
# 默认仍为单线程；可按可用内存指定 1–8 个编译任务，不改变芯片运行参数。
$arguments = @('compile','--fqbn',$fqbn,'--jobs',"$Jobs",'--warnings','all','--verbose','--build-path',$build)
if ($ConfigFile) { $arguments += @('--config-file',$ConfigFile) }
$arguments += $sketch
& $ArduinoCli @arguments *> $log
$exitCode = $LASTEXITCODE
Get-Content -LiteralPath $log
if ($exitCode -ne 0) { throw "Arduino provisioned MQTT compile failed ($exitCode)" }
$app = Join-Path $build 'esp32_spi_bridge_mqtt_provisioned.ino.bin'
if (-not (Test-Path -LiteralPath $app)) { throw 'Compiler returned success but application BIN is missing.' }
if ((Get-FileHash -LiteralPath $parameterFile -Algorithm SHA256).Hash -ne $parameterHash) { throw 'Feed parameters changed during compilation. Rebuild first.' }
foreach ($source in $sources) {
    $path = Join-Path $sketch $source.name
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $source.sha256) {
        throw "Source changed during compilation. Rebuild before release: $($source.name)"
    }
    # 核对全部实际构建输入；INO 使用生成原型之前的合并副本。
    $compiledName = if ([IO.Path]::GetExtension($path) -eq '.ino') { $source.name + '.cpp.merged' } else { $source.name }
    $compiled = Join-Path (Join-Path $build 'sketch') $compiledName
    if (-not (Test-Path -LiteralPath $compiled)) { throw "Compiled input is missing: $($source.name)" }
    $content = [IO.File]::ReadAllText($compiled)
    # Arduino CLI 在副本前添加源路径 #line；只去掉这一条已核对路径的标记。
    $lineEnd = $content.IndexOf("`n")
    $expectedLine = '#line 1 "' + $path.Replace('\','\\') + '"'
    if ($lineEnd -ge 0 -and $content.Substring(0,$lineEnd).TrimEnd("`r") -ceq $expectedLine) {
        $content = $content.Substring($lineEnd + 1)
    }
    $original = [IO.File]::ReadAllText($path)
    # 合并 INO 时 CLI 额外补一个末尾换行；不忽略任何实际代码差异。
    $addedNewline = [IO.Path]::GetExtension($path) -eq '.ino' -and $content -ceq ($original + "`n")
    if ($content -cne $original -and -not $addedNewline) {
        throw "Compiled input does not match source. Rebuild before release: $($source.name)"
    }
}
# 只有成功编译才创建交付目录，防止把失败构建的旧 BIN 误当成最新版。
$release = Join-Path $PSScriptRoot ('release_mqtt_provisioned_com_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Path $release | Out-Null
Get-ChildItem -LiteralPath $build -File | Where-Object {
    $_.Extension -in @('.bin', '.elf') -or $_.Name -eq 'partitions.csv'
} | Copy-Item -Destination $release
Copy-Item -LiteralPath $log -Destination (Join-Path $release 'compile.log')
Copy-Item -LiteralPath $parameterFile -Destination (Join-Path $release 'feed_stage.env')
$files = Get-ChildItem -LiteralPath $release -File | Where-Object { $_.Extension -in @('.bin', '.elf') } | ForEach-Object {
    [pscustomobject]@{ name = $_.Name; bytes = $_.Length; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
}
[pscustomobject]@{
    built_at = (Get-Date -Format o); fqbn = $fqbn; connection = 'COM'; flashed = $false; jobs = $Jobs
    release_valid = $true; do_not_flash = $false
    source = $sketch; files = @($files); sources = @($sources)
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $release 'manifest.json') -Encoding UTF8
Write-Output 'Compile completed. NO hardware was flashed.'
Write-Output "Release directory: $release"
