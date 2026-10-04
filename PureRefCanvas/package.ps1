<#
.SYNOPSIS
    里与由 (PureRefCanvas) 一键构建 + Qt 部署 + 免安装包压缩。

.DESCRIPTION
    做四件事，任何一步失败都会以非 0 退出码结束：
      1. 从 CMakeLists.txt 读取版本号（可用 -Version 覆盖）
      2. cmake + ninja 构建 Release
      3. 清理 release\<应用名> 并 windeployqt 部署 Qt 运行时
      4. 校验 exe 的每一个 DLL 依赖都在部署目录里，然后打成扁平结构的 zip

    校验步骤是为了防止"包看起来正常、拿到别的机器上起不来"：只要 exe 导入的
    DLL 有一个不在部署目录里，脚本就直接报错而不是打出坏包。

.EXAMPLE
    pwsh -File package.ps1
    构建当前源码、部署并生成 <输出根目录>\里与由_v1.0.0_portable.zip

.EXAMPLE
    pwsh -File package.ps1 -SkipBuild -SkipZip
    只重新部署 release 目录，不重新编译、不压缩（用于快速本地试运行）
#>
[CmdletBinding()]
param(
    # 覆盖版本号（默认从 CMakeLists.txt 的 project(... VERSION x.y.z) 读取）
    [string]$Version,

    # Qt / 工具链路径，换机器时改这里或作为参数传入
    [string]$QtDir    = "D:\software\QT\6.11.2\mingw_64",
    [string]$CMakeBin = "D:\software\QT\Tools\CMake_64\bin",
    [string]$NinjaDir = "D:\software\QT\Tools\Ninja",
    [string]$MinGWBin = "D:\software\QT\Tools\mingw1310_64\bin",

    # 可执行文件基础名（CMakeLists.txt 里 WIN32 下 OUTPUT_NAME 的值）
    [string]$AppName  = "里与由",

    # 产物根目录。默认放在项目上一级：<父目录>\release\<应用名> 与 <父目录>\<应用名>_v<版本>_portable.zip
    [string]$OutputRoot,

    [switch]$SkipBuild,
    [switch]$SkipZip
)

$ErrorActionPreference = "Stop"

function Write-Step([string]$text) {
    Write-Host ""
    Write-Host "=== $text ===" -ForegroundColor Cyan
}

function Fail([string]$text) {
    Write-Host ""
    Write-Host "!!! $text" -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------- 路径准备
$root      = $PSScriptRoot
$sourceDir = $root
$buildDir  = Join-Path $root "build"

if (-not $OutputRoot) {
    $OutputRoot = Split-Path $root -Parent
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$stageDir   = Join-Path $OutputRoot ("release\" + $AppName)

if (-not (Test-Path (Join-Path $root "CMakeLists.txt"))) {
    Fail "在 $root 下找不到 CMakeLists.txt，脚本似乎放错了位置。"
}

# 删除前的安全校验：只允许删掉 <OutputRoot>\release\<AppName> 这个精确路径
$expectedStage = [System.IO.Path]::GetFullPath((Join-Path $OutputRoot ("release\" + $AppName)))
if ($stageDir -ne $expectedStage -or (Split-Path $stageDir -Leaf) -ne $AppName) {
    Fail "部署目录解析异常（$stageDir），已中止以免误删文件。"
}

# ---------------------------------------------------------------- 版本号
if (-not $Version) {
    $cmakeText = Get-Content -LiteralPath (Join-Path $root "CMakeLists.txt") -Raw
    $m = [regex]::Match($cmakeText, 'project\s*\(\s*\w+\s+VERSION\s+([0-9]+(?:\.[0-9]+)+)')
    if (-not $m.Success) {
        Fail "无法从 CMakeLists.txt 解析版本号，请用 -Version x.y.z 指定。"
    }
    $Version = $m.Groups[1].Value
}
Write-Host "里与由 打包脚本  v$Version" -ForegroundColor Green
Write-Host "  源码目录 : $sourceDir"
Write-Host "  构建目录 : $buildDir"
Write-Host "  部署目录 : $stageDir"
Write-Host "  输出根目录: $OutputRoot"

# ---------------------------------------------------------------- 工具链检查
$toolPaths = @{
    "cmake"       = Join-Path $CMakeBin "cmake.exe"
    "ninja"       = Join-Path $NinjaDir "ninja.exe"
    "c++ (MinGW)" = Join-Path $MinGWBin "c++.exe"
    "windeployqt" = Join-Path $QtDir    "bin\windeployqt.exe"
}
foreach ($k in $toolPaths.Keys) {
    if (-not (Test-Path $toolPaths[$k])) {
        Fail "找不到 $k ：$($toolPaths[$k])`n请用参数指定正确路径（-QtDir / -CMakeBin / -NinjaDir / -MinGWBin）。"
    }
}

# 本机 PATH 上可能有另一套 cmake/ninja（例如 WinLibs 的），必须让 Qt 自带的排在前面
$env:PATH = "$CMakeBin;$NinjaDir;$MinGWBin;$(Join-Path $QtDir 'bin');$env:PATH"

# ---------------------------------------------------------------- 1. 构建
$exeName = "$AppName.exe"
$builtExe = Join-Path $buildDir $exeName

if (-not $SkipBuild) {
    Write-Step "配置 (cmake)"
    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }
    Push-Location $buildDir
    try {
        & cmake -G Ninja -DCMAKE_PREFIX_PATH="$QtDir" -DCMAKE_BUILD_TYPE=Release "$sourceDir"
        if ($LASTEXITCODE -ne 0) { Fail "cmake 配置失败（退出码 $LASTEXITCODE）。" }

        Write-Step "编译 (ninja)"
        & ninja
        if ($LASTEXITCODE -ne 0) { Fail "编译失败（退出码 $LASTEXITCODE）。" }
    } finally {
        Pop-Location
    }
} else {
    Write-Step "跳过编译 (-SkipBuild)"
}

if (-not (Test-Path $builtExe)) {
    $candidates = @(Get-ChildItem -LiteralPath $buildDir -Filter "*.exe" -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -notmatch '^(a|cmake|CMake)' } |
                    Select-Object -ExpandProperty Name)
    Fail "构建产物不存在：$builtExe`n构建目录里的 exe 有：$($candidates -join ', ')`n如果 OUTPUT_NAME 改过，请用 -AppName 指定。"
}

$built = Get-Item -LiteralPath $builtExe
Write-Host ("  产物: {0}  ({1:N0} 字节, {2})" -f $built.Name, $built.Length, $built.LastWriteTime)

# ---------------------------------------------------------------- 2. 部署
Write-Step "清理并部署到 release"
if (Test-Path $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
Copy-Item -LiteralPath $builtExe -Destination (Join-Path $stageDir $exeName) -Force

$deployExe = Join-Path $stageDir $exeName
& (Join-Path $QtDir "bin\windeployqt.exe") --release --no-translations --no-system-d3d-compiler --no-opengl-sw $deployExe
if ($LASTEXITCODE -ne 0) { Fail "windeployqt 失败（退出码 $LASTEXITCODE）。" }

# ------------------------------------------- 2b. D3D12 着色器编译器（可选组件）
# Qt6 的 D3D12 渲染后端在运行时用 LoadLibrary 加载 dxcompiler.dll / dxil.dll，
# 所以它们不会出现在 exe 的导入表里，windeployqt 只在 Qt 安装目录里找得到时才拷贝。
# 本机这套 Qt 6.11.2 并未附带它们，但此前发布出去的包里是有的：为了保持发布内容一致、
# 避免在会走到 D3D12 后端的机器上出现回归，这里显式补齐一次（找不到只警告，不算失败）。
$d3dDlls = @("dxcompiler.dll", "dxil.dll")
$d3dSearchDirs = @((Join-Path $QtDir "bin"), $buildDir)
foreach ($dll in $d3dDlls) {
    $dest = Join-Path $stageDir $dll
    if (Test-Path $dest) { continue }
    $from = $null
    foreach ($dir in $d3dSearchDirs) {
        $candidate = Join-Path $dir $dll
        if (Test-Path $candidate) { $from = $candidate; break }
    }
    if ($from) {
        Copy-Item -LiteralPath $from -Destination $dest -Force
        Write-Host ("  已补充 {0}  (来自 {1})" -f $dll, $from)
    } else {
        Write-Host ("  注意: 找不到 {0} — 程序不使用 D3D12 后端时无影响" -f $dll) -ForegroundColor Yellow
    }
}

# ------------------------------------------- 2c. 图片格式插件（WebP / TIFF 等）
# 缩略图缓存在 WebP 可写时优先存 WebP（体积约为 PNG 的三分之一），而 WebP 支持来自
# Qt 安装器的 "Qt Image Formats" 组件（qwebp.dll）。程序里所有"这个文件算不算图片"
# 的判断都是运行时探测的，所以插件装没装都不会崩——但发出去的包里缺了它，用户就会
# 遇到"开发机上能看、装到别的机器上打不开"的经典问题，所以这里核对一次。
$qtImageFormats = Join-Path $QtDir "plugins\imageformats"
if (Test-Path $qtImageFormats) {
    $missingPlugins = @()
    foreach ($plugin in (Get-ChildItem -LiteralPath $qtImageFormats -Filter "*.dll" -File)) {
        if (-not (Test-Path (Join-Path $stageDir ("imageformats\" + $plugin.Name)))) {
            $missingPlugins += $plugin.Name
        }
    }
    if ($missingPlugins.Count -gt 0) {
        Write-Host ("  注意: Qt 里有、包里没有的图片格式插件: " + ($missingPlugins -join ", ")) -ForegroundColor Yellow
    }
}
if (-not (Test-Path (Join-Path $stageDir "imageformats\qwebp.dll"))) {
    Write-Host "  注意: 包里没有 qwebp.dll — WebP 图片无法解码，缩略图缓存退回 PNG。" -ForegroundColor Yellow
    Write-Host "        需要 WebP 支持时: 用 MaintenanceTool.exe 给 Qt 勾选 'Qt Image Formats' 组件后重新打包。"
} else {
    Write-Host "  WebP 插件已包含: 缩略图缓存将使用 WebP（体积约为 PNG 的 1/3）" -ForegroundColor Green
}

# ---------------------------------------------------------------- 3. 依赖校验
Write-Step "校验 exe 的 DLL 依赖是否齐全"

# 只关心随包分发的运行时 DLL；Windows 系统 DLL 不匹配这些前缀，天然被排除
$depPattern = 'Qt6[A-Za-z0-9_]*\.dll|libgcc[A-Za-z0-9_+.-]*\.dll|libstdc[A-Za-z0-9_+.-]*\.dll|libwinpthread[A-Za-z0-9_+.-]*\.dll|avcodec[A-Za-z0-9_-]*\.dll|avformat[A-Za-z0-9_-]*\.dll|avutil[A-Za-z0-9_-]*\.dll|swresample[A-Za-z0-9_-]*\.dll|swscale[A-Za-z0-9_-]*\.dll|dxcompiler\.dll|dxil\.dll'

function Get-RuntimeImports([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $text  = [System.Text.Encoding]::ASCII.GetString($bytes)
    [regex]::Matches($text, $depPattern) | ForEach-Object { $_.Value } | Sort-Object -Unique
}

$missing = @()
foreach ($dep in Get-RuntimeImports $deployExe) {
    if (-not (Test-Path (Join-Path $stageDir $dep))) { $missing += $dep }
}
if ($missing.Count -gt 0) {
    Fail ("部署目录缺少 exe 依赖的 DLL，这个包在别的机器上会启动失败：`n  " + ($missing -join "`n  "))
}
Write-Host "  依赖校验通过" -ForegroundColor Green

# 插件目录也要在，否则 Qt 会因为找不到平台插件而无法启动
if (-not (Test-Path (Join-Path $stageDir "platforms\qwindows.dll"))) {
    Fail "缺少 platforms\qwindows.dll，程序会报 'could not find or load the Qt platform plugin windows'。"
}

$stagedFiles = @(Get-ChildItem -LiteralPath $stageDir -Recurse -File)
$stagedBytes = ($stagedFiles | Measure-Object -Property Length -Sum).Sum
Write-Host ("  部署完成: {0} 个文件, {1:N1} MB" -f $stagedFiles.Count, ($stagedBytes / 1MB))

# ---------------------------------------------------------------- 4. 压缩
if ($SkipZip) {
    Write-Step "跳过压缩 (-SkipZip)"
    Write-Host "  部署目录: $stageDir"
    exit 0
}

Write-Step "生成免安装 zip"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$zipPath = Join-Path $OutputRoot ("{0}_v{1}_portable.zip" -f $AppName, $Version)
if (Test-Path $zipPath) { Remove-Item -LiteralPath $zipPath -Force }

# 与现有发布包保持一致：zip 内是平铺结构（没有顶层文件夹），解压即用
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $stageDir, $zipPath,
    [System.IO.Compression.CompressionLevel]::Optimal,
    $false)

$zip = Get-Item -LiteralPath $zipPath
Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host " 打包完成!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ("  版本     : v{0}" -f $Version)
Write-Host ("  程序     : {0}  ({1:N0} 字节, 构建于 {2})" -f $exeName, $built.Length, $built.LastWriteTime)
Write-Host ("  部署目录 : {0}  ({1} 个文件, {2:N1} MB)" -f $stageDir, $stagedFiles.Count, ($stagedBytes / 1MB))
Write-Host ("  压缩包   : {0}  ({1:N1} MB)" -f $zip.FullName, ($zip.Length / 1MB))
