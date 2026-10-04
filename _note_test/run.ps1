# NoteWidget regression tests (offscreen Qt).
# Configure once, build with ninja, run with QT_QPA_PLATFORM=offscreen.
# Exit code 0 = all PASS.
$ErrorActionPreference = "Stop"

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:PATH = "D:\software\QT\Tools\CMake_64\bin;D:\software\QT\Tools\Ninja;D:\software\QT\Tools\mingw1310_64\bin;D:\software\QT\6.11.2\mingw_64\bin;" + $env:PATH
$env:QT_QPA_PLATFORM = "offscreen"

Push-Location $here
try {
    if (-not (Test-Path "$here\build\build.ninja")) {
        cmake -S $here -B "$here\build" -G Ninja -DCMAKE_PREFIX_PATH="D:/software/QT/6.11.2/mingw_64" -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { exit 1 }
    }
    ninja -C "$here\build" notetest
    if ($LASTEXITCODE -ne 0) { exit 1 }

    & "$here\build\notetest.exe"
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
