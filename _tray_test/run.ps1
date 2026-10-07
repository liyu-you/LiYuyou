<#
  Build and run the tray-menu UI test (traytest).

  Notes:
    * Qt / MinGW bins must be on PATH or the exe dies with 0xC0000135
      before producing any output.
    * QT_QPA_PLATFORM=offscreen keeps the run headless: no window flashes.
    * The test renders the tray menu in dark + light schemes into
      tray_preview.png next to this script.

  Exit code mirrors traytest: 0 = all checks passed.
#>
[CmdletBinding()]
param(
    [string]$QtDir    = "E:\QT\6.11.2\mingw_64",
    [string]$CMakeBin = "E:\QT\Tools\CMake_64\bin",
    [string]$NinjaDir = "E:\QT\Tools\Ninja",
    [string]$MinGWBin = "E:\QT\Tools\mingw1310_64\bin"
)

$ErrorActionPreference = "Stop"
$root  = $PSScriptRoot
$build = Join-Path $root "build"

$env:Path = "$QtDir\bin;$MinGWBin;$NinjaDir;$CMakeBin;" + $env:Path

if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    & cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_PREFIX_PATH=$QtDir" "-DCMAKE_MAKE_PROGRAM=$NinjaDir/ninja.exe" `
        "-DCMAKE_CXX_COMPILER=$MinGWBin/g++.exe"
    if ($LASTEXITCODE -ne 0) { Write-Host "!!! cmake configure failed" -ForegroundColor Red; exit 1 }
}

& cmake --build $build
if ($LASTEXITCODE -ne 0) { Write-Host "!!! build failed" -ForegroundColor Red; exit 1 }

$env:QT_FORCE_STDERR_LOGGING = "1"
# NOTE: unlike the other UI tests this one must run on the real "windows"
# platform — the offscreen QPA has no font database, so every glyph in the
# rendered preview would come out as tofu boxes. Nothing is ever show()n,
# so the run is still headless (no window flashes).
$env:QT_QPA_PLATFORM = "windows"

$preview = Join-Path $root "tray_preview.png"
$log = Join-Path $build "traytest_log.txt"
& cmd /c "`"$build\traytest.exe`" `"$preview`" > `"$log`" 2>&1"
$code = $LASTEXITCODE

Get-Content $log
Write-Host ""
Write-Host "traytest exit code: $code" -ForegroundColor $(if ($code -eq 0) { "Green" } else { "Red" })
exit $code
