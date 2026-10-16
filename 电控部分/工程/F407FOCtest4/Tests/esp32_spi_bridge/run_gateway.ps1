param([string]$Compiler='C:\msys64\mingw64\bin\g++.exe')
$ErrorActionPreference='Stop'
$build=Join-Path ([IO.Path]::GetTempPath()) 'rabbit_gateway_host_tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$source=Join-Path $PSScriptRoot '..\..\build-gcc\esp32_spi\esp32_spi_bridge_mqtt_provisioned'
$text=Get-Content -LiteralPath (Join-Path $source 'GatewayApp.h') -Raw -Encoding UTF8
$a=$text.IndexOf('static void applyRelayOutputs(');$b=$text.IndexOf('static bool fromAp(')
$c=$text.IndexOf('static const char* execute(');$d=$text.IndexOf('// 以下对象')
$e=$text.IndexOf('static void handleLocal(');$f=$text.IndexOf('static void begin()')
if($a -lt 0 -or $b -le $a -or $c -lt 0 -or $d -le $c -or $e -lt 0 -or $f -le $e){throw 'Production function extraction failed'}
Set-Content -LiteralPath (Join-Path $build 'gateway_runtime_extracted.h') -Value ($text.Substring($a,$b-$a)+$text.Substring($c,$d-$c)+$text.Substring($e,$f-$e)) -Encoding utf8
# Compile the real final MQTT feed_busy argument, rather than a copied expectation.
$busy=[regex]::Match($text,'feed::name\(snapshot\.feedState\),snapshot\.feedConfigured,\s*(?<expr>[^;]+?)\);')
if(-not $busy.Success){throw 'Production feed_busy extraction failed'}
$busyCode='inline bool publishedFeedBusy(feed::State value) { struct Snapshot { feed::State feedState; }; Snapshot snapshot{value}; return '+$busy.Groups['expr'].Value+'; }'
Set-Content -LiteralPath (Join-Path $build 'gateway_feed_busy_extracted.h') -Value $busyCode -Encoding utf8
foreach($name in @('gateway_test','gateway_runtime_test','relay_test','m1_auto_feed_cycle_test')){
  $exe=Join-Path $build ($name+'.exe')
  & $Compiler -std=c++17 -Wall -Wextra -Werror -I $source -I $build -I (Join-Path $PSScriptRoot 'stubs') (Join-Path $PSScriptRoot ($name+'.cpp')) -o $exe
  if($LASTEXITCODE -ne 0){throw "$name compile failed"}
  & $exe
  if($LASTEXITCODE -ne 0){throw "$name failed"}
}
