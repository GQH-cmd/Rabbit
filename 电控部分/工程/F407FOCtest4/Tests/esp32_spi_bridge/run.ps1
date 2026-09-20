param([string]$Compiler = 'g++')
$ErrorActionPreference = 'Stop'
$build = Join-Path ([System.IO.Path]::GetTempPath()) 'rabbit_esp32_spi_host_tests'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$exe = Join-Path $build 'bridge_test.exe'
& $Compiler '-std=c++17' '-Wall' '-Wextra' '-Werror' '-I' (Join-Path $PSScriptRoot 'stubs') (Join-Path $PSScriptRoot 'bridge_test.cpp') '-o' $exe
if ($LASTEXITCODE -ne 0) { throw "Host compilation failed: $LASTEXITCODE" }
& $exe
if ($LASTEXITCODE -ne 0) { throw "Bridge regression tests failed: $LASTEXITCODE" }
