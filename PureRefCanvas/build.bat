@echo off
REM ============================================
REM  PureRefCanvas - build entry point
REM
REM  The real work lives in package.ps1, which is the single source of
REM  truth for the toolchain paths, the executable name and the deploy step.
REM  (The old version of this script hard-coded PureRefCanvas.exe while CMake
REM   names the binary after OUTPUT_NAME, so its windeployqt step never ran.)
REM
REM  Usage:
REM    build.bat                build + deploy + portable zip
REM    build.bat -SkipZip       build + deploy only
REM    build.bat -SkipBuild     deploy the existing build output only
REM ============================================

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
exit /b %errorlevel%
