# PureRefCanvas — 里与由（LiyuYou）源码

这是「里与由」桌面应用的**主程序源码目录**（CMake 工程，产物名 `LiyuYou.exe`，对外发布名「里与由」）。

完整的软件介绍、功能清单、技术栈与构建说明见仓库根目录 **[README.md](../README.md)**。

本项目起源于一个 PureRef 风格的无限参考图画布，逐步演进为包含画布、书库阅读、笔记、音乐播放、素材库与空间系统的一体化知识工作台。画布模块至今仍是核心工作区之一。

## 画布模块（Canvas）

PureRef 风格无限画布，用于整理参考图：

- **Infinite canvas** — pan and zoom without boundaries
- **Image management** — drag-and-drop, open dialog, clipboard paste
- **Transform controls** — move, resize (corner handles), rotate (top handle)
- **Multi-select** — Ctrl+click, Ctrl+A, rubber-band
- **Z-order** — bring to front / send to back
- **Save / Load** — scene files (`.prc`) embed all images and transforms
- **Export** — render the whole canvas to PNG/JPEG/BMP
- **Space-specific canvas** — each space owns an isolated canvas

## Build

Requires Qt 6.x with MinGW or MSVC. The project targets **C++23**.

### Quick build (Windows, MinGW)

```
build.bat
```

Edit `build.bat` if your Qt is installed elsewhere.

### Manual build

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH=<path-to-qt> -DCMAKE_BUILD_TYPE=Release ..
ninja
windeployqt --release LiyuYou.exe
```

### Portable packaging

```powershell
./package.ps1
```

## Canvas controls

| Action | Shortcut |
|--------|----------|
| Pan | Drag empty area / Space+drag / Middle-drag |
| Zoom | Mouse wheel / `+` `-` |
| Actual size | `Ctrl+0` |
| Fit to view | `F` / `Ctrl+F` |
| Move image | Drag image |
| Resize | Drag corner handle (Shift = free aspect) |
| Rotate | Drag top handle |
| Nudge | Arrow keys (Shift = 10px) |
| Select all | `Ctrl+A` |
| Delete | `Delete` / `Backspace` |
| Copy / Paste | `Ctrl+C` / `Ctrl+V` |
| Duplicate | `Ctrl+D` |
| Bring to front | `Ctrl+]` |
| Send to back | `Ctrl+[` |

## Source layout

```
PureRefCanvas/
├── CMakeLists.txt      # CMake build config (C++23, Qt components, Win32 libs)
├── main.cpp            # Entry: settings migration, GPU pipeline, single-instance, login, splash
├── mainwindow.h/cpp    # Main window: workspace switching, menus, drag-and-drop, shortcuts
├── canvasview.h/cpp    # QGraphicsView: pan, zoom, drag-and-drop
├── canvasscene.h/cpp   # QGraphicsScene: items, selection, serialization
├── imageitem.h/cpp     # QGraphicsObject: image with resize/rotate handles
├── booklibrary.h/cpp   # Book library (SQLite WAL)
├── readerwidget.h/cpp  # Reader workspace
├── notewidget.h/cpp    # Notes: rich text, wiki-links, outline, cards
├── musicplayer.h/cpp   # Music player (gapless engine, CUE, library DB)
├── materiallibrary.h/cpp # Material / asset library
├── spacemanager.h/cpp  # Spaces system
├── userauth.h/cpp      # Account auth
├── settingsdialog.h/cpp# Settings center
├── build.bat           # One-click build script
├── package.ps1         # Portable packaging script
└── build/              # Build output (gitignored)
```
