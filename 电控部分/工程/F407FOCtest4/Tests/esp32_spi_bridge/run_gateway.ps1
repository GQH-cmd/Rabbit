param([string]$Compiler='C:\msys64\mingw64\bin\g++.exe')
$ErrorActionPreference='Stop'
$build='E:\RabbitBuild\host-tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$source=Join-Path $PSScriptRoot '..\..\build-gcc\esp32_spi\esp32_spi_bridge_mqtt_provisioned'
$text=Get-Content -LiteralPath (Join-Path $source 'GatewayApp.h') -Raw -Encoding UTF8
$a=$text.IndexOf('static void finishPending(');$b=$text.IndexOf('static void reportStatus(')
$c=$text.IndexOf('static const char* execute(');$d=$text.IndexOf('// 以下对象')
if($a -lt 0 -or $b -le $a -or $c -lt 0 -or $d -le $c){throw 'Production function extraction failed'}
Set-Content -LiteralPath (Join-Path $build 'gateway_runtime_extracted.h') -Value ($text.Substring($a,$b-$a)+$text.Substring($c,$d-$c)) -Encoding utf8
foreach($name in @('gateway_test','gateway_runtime_test')){
  $exe=Join-Path $build ($name+'.exe')
  & $Compiler -std=c++17 -Wall -Wextra -Werror -I $source -I $build -I (Join-Path $PSScriptRoot 'stubs') (Join-Path $PSScriptRoot ($name+'.cpp')) -o $exe
  if($LASTEXITCODE -ne 0){throw "$name compile failed"}
  & $exe
  if($LASTEXITCODE -ne 0){throw "$name failed"}
}
