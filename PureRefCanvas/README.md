# PureRef Canvas

A PureRef-style infinite canvas for organizing reference images, built with Qt 6 C++.

## Features

- **Infinite canvas** — pan and zoom without boundaries
- **Image management** — drag-and-drop, open dialog, clipboard paste
- **Transform controls** — move, resize (corner handles), rotate (top handle)
- **Multi-select** — Ctrl+click, Ctrl+A, rubber-band
- **Z-order** — bring to front / send to back
- **Save / Load** — scene files (`.prc`) embed all images and transforms
- **Export** — render the whole canvas to PNG/JPEG/BMP
- **Dark UI** — PureRef-inspired dark theme with grid background

## Build

Requires Qt 6.x with MinGW or MSVC.

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
windeployqt --release PureRefCanvas.exe
```

## Controls

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

## Project Structure

```
PureRefCanvas/
├── CMakeLists.txt      # CMake build config
├── main.cpp            # Entry point
├── mainwindow.h/cpp    # Main window, menus, toolbar, status bar
├── canvasview.h/cpp    # QGraphicsView: pan, zoom, drag-and-drop
├── canvasscene.h/cpp   # QGraphicsScene: items, selection, serialization
├── imageitem.h/cpp     # QGraphicsObject: image with resize/rotate handles
├── build.bat           # One-click build script
└── build/              # Build output (PureRefCanvas.exe)
```
