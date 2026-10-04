<#
  Build and run the account-store test suite (authtest).

  Notes:
    * Qt / MinGW bins must be on PATH or authtest.exe dies with 0xC0000135
      before producing any output.
    * Qt logging writes to stderr only when it detects a console, so the run
      is redirected through cmd into a log file and printed afterwards.
    * Test data lives in build\scratch via LIYIYOU_TEST_DIR; the real
      account store is never touched.

  Exit code mirrors authtest: 0 = all checks passed.
#>
[CmdletBinding()]
param(
    [string]$QtDir    = "D:\software\QT\6.11.2\mingw_64",
    [string]$CMakeBin = "D:\software\QT\Tools\CMake_64\bin",
    [string]$NinjaDir = "D:\software\QT\Tools\Ninja",
    [string]$MinGWBin = "D:\software\QT\Tools\mingw1310_64\bin"
)

$ErrorActionPreference = "Stop"
$root  = $PSScriptRoot
$build = Join-Path $root "build"

$env:Path = "$QtDir\bin;$MinGWBin;$NinjaDir;$CMakeBin;" + $env:Path

if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    & cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtDir" "-DCMAKE_MAKE_PROGRAM=$NinjaDir/ninja.exe"
    if ($LASTEXITCODE -ne 0) { Write-Host "!!! cmake configure failed" -ForegroundColor Red; exit 1 }
}

& cmake --build $build
if ($LASTEXITCODE -ne 0) { Write-Host "!!! build failed" -ForegroundColor Red; exit 1 }

$env:QT_FORCE_STDERR_LOGGING = "1"
$env:LIYIYOU_TEST_DIR = Join-Path $build "scratch"

$log = Join-Path $build "authtest_log.txt"
& cmd /c "`"$build\authtest.exe`" > `"$log`" 2>&1"
$code = $LASTEXITCODE

Get-Content $log
Write-Host ""
Write-Host "authtest exit code: $code" -ForegroundColor $(if ($code -eq 0) { "Green" } else { "Red" })
exit $code
