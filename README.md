# 里与由（LiyuYou）

> 个人知识工作台 · Windows 桌面 · Qt 6 / C++23 · 数据本地存储

从 PureRef 风格无限画布演进而来的一体化桌面套件，集画布、书库、笔记、音乐、素材库、空间系统于一身。当前版本 **v1.0.0**。

## 功能

**🖼 画布** — 无限平移/缩放、拖放/粘贴导入、移动/缩放/旋转、多选与层级、`.prc` 场景保存、导出 PNG/JPEG/BMP、空间专属画布。

**📚 书库与阅读** — 书架卡片、批量导入/扫描文件夹、SQLite WAL、自定义封面、进度持久化、阅读计时与番茄钟、随包 28 部公版经典。

**📝 笔记** — 富文本编辑、Logseq 双链与反链、大纲面板、全局搜索（Ctrl+K）、选段存为笔记、自动保存与损坏隔离恢复、间隔重复卡片（SM-2）。

**🎵 音乐** — miniaudio 无缝播放、CUE 分轨、ReplayGain 归一化、SQLite 曲库与播放统计、歌词/迷你/全屏播放器、FFmpeg 深度元数据。

**🗂 素材库** — Eagle 风格元数据（评分/标签/来源）、结构化搜索、剪贴板粘贴、拖放、筛选 Chips、毛玻璃侧栏。

**🧭 空间系统** — 顶部切换、内容按空间过滤、新建自动归类、重命名/删除、颜色主题、状态记忆与环境预设、欢迎浮层+专注计时、空间独立画布。

**👤 账户与体验** — 登录/注册/忘记密码、滑块验证、封面（图片/GIF）、签名、DPAPI 加密记住密码、Steam 风格加载页。

**⚡ 效率与专注** — 沉浸模式 F11、数字花园（Ctrl+Shift+G）、本地 RAG 问答（Ctrl+K `?问题`）、每周一自动周报、全局截图（Ctrl+Shift+A）、快捷键 Ctrl+N/O/S/1-5。

**🎨 UI/系统** — Material 浅色/深色主题、托盘、单实例、自动保存、文件日志、开机自启、更新检查、GPU 硬件加速、UI 缩放、多语言。

## 技术栈

C++23 · Qt 6（Widgets/Multimedia/Network/OpenGL/Concurrent/Sql/PrintSupport）· SQLite WAL · miniaudio · FFmpeg · CMake + Ninja（MinGW GCC 13.1）· DWM 毛玻璃 · DPAPI · windeployqt + `package.ps1` 打包便携版。

## 构建

依赖：Qt 6.x、CMake 3.16+、Ninja。

```bat
:: 一键构建（Qt 路径不同时编辑 build.bat）
build.bat
```

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH=<path-to-qt> -DCMAKE_BUILD_TYPE=Release ..
ninja
windeployqt --release LiyuYou.exe
# 打包便携版
./package.ps1
```

## 目录

```
PureRefCanvas/   主程序源码（产物 LiyuYou.exe，发布名「里与由」）
release/         发布产物（不入库）
_auth_test/ 等   各模块无头测试
```

## 快捷键

| 功能 | 快捷键 |
|------|--------|
| 沉浸模式 | `F11` |
| 数字花园 | `Ctrl+Shift+G` |
| 搜索 / RAG 问答 | `Ctrl+K`（`?问题`） |
| 全局截图 | `Ctrl+Shift+A` |
| 新建 / 打开 / 保存 | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` |
| 切换工作区 | `Ctrl+1` ~ `Ctrl+5` |
| 画布平移 / 缩放 | 拖拽 / 滚轮 |
| 适应视图 | `F` |

> 数据本地存储；密码经系统 DPAPI 保护。
