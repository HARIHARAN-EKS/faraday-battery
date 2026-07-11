# Faraday build helper — configure, build and test with the Qt/MinGW toolchain.
# Usage: powershell -ExecutionPolicy Bypass -File build.ps1 [-Config Release] [-Target all] [-Test]
param(
    [string]$Config = "Release",
    [string]$Target = "all",
    [switch]$Test
)

$ErrorActionPreference = "Stop"

$QtDir    = "C:\Qt\6.8.2\mingw_64"
$MinGWDir = "C:\Qt\Tools\mingw1310_64"

$env:Path = "$MinGWDir\bin;$QtDir\bin;" + [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')

$root  = $PSScriptRoot
$build = Join-Path $root "build"

cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Config" "-DCMAKE_PREFIX_PATH=$QtDir" "-DCMAKE_CXX_COMPILER=$MinGWDir\bin\g++.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $build --target $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    ctest --test-dir $build --output-on-failure
    exit $LASTEXITCODE
}
