# 里与由（LiyuYou）

> 个人知识工作台 · Windows 桌面应用 · Qt 6 / C++23

「里与由」是一款从 PureRef 风格无限画布起步、逐步演进为**多工作区一体化**的桌面生产力套件：画布、书库阅读、笔记、音乐播放、素材库与空间系统，全部沉淀在同一个本地应用中，数据本地存储、离线可用。

当前版本：**v1.0.0**（便携版 `里与由_v1.0.0_portable.zip`）

---

## 功能概览

### 🖼 无限画布
PureRef 风格参考图画布，适合收集灵感图与参考资料：
- 无限平移 / 缩放，`Space`+拖拽、中键拖拽快速平移
- 拖放、文件对话框、剪贴板粘贴导入图片
- 移动 / 缩放（角柄）/ 旋转（顶柄）变换，`Shift` 自由比例
- 多选（`Ctrl`+点击、`Ctrl+A`、框选）、层级置顶/置底
- 场景保存 / 加载（`.prc` 内嵌全部图片与变换）、导出 PNG / JPEG / BMP
- 方形缩略图网格、空状态引导
- **空间专属画布**：每个空间独立画布，切换空间内容互不干扰

### 📚 书库与阅读
- 书架卡片式书库，批量导入与「扫描文件夹」批量导入
- SQLite WAL 持久化（含旧 JSON 数据自动迁移、追加式历史）
- 自定义封面（右键）、阅读进度持久化（拖动进度条即时落盘）
- 阅读计时（跨天归零）与番茄钟专注计时
- 硬换行书源自动重排、正文铺满视口
- 随包附赠 28 部公版中文经典（`books` 目录）

### 📝 笔记
- Word 式富文本编辑工具栏，纸白编辑器
- Logseq 风格日记入口与双链：`[[wiki-link]]` 自动补全 + 反链面板
- VNote 风格右侧大纲面板
- 卡片网格工作区、全局搜索（`Ctrl+K`）
- 阅读选段右键「存为笔记」，跨工作区联动
- 自动保存 + 损坏文件隔离（`notes.json.corrupt-*`）与 `.bak` 自动恢复
- 间隔重复记忆卡片（简化 SM-2 算法）

### 🎵 音乐播放
- 本地音乐播放，miniaudio **无缝播放引擎**（Gapless，预加载下一首）
- CUE 分轨解析、ReplayGain 音量归一化
- SQLite 曲库索引：播放统计、收藏、最近播放 / 最常播放视图
- 本地音频深度元数据解析（FFmpeg 解码）、封面图
- 歌词面板、迷你播放器、全屏播放器

### 🗂 素材库
- Eagle 风格元数据层：评分 / 标签 / 来源
- 结构化搜索、剪贴板粘贴、网格拖放、筛选 Chips、毛玻璃侧栏

### 🧭 空间系统
- 顶部空间切换，书库 / 笔记按空间过滤
- 空间重命名 / 删除管理，新建内容自动归入当前空间
- 空间颜色主题、状态记忆与环境预设
- 空间仪式欢迎浮层 + 专注计时，快捷键快速切换

### 👤 账户与体验
- 登录 / 注册 / 忘记密码，滑块验证，Google Material 风格登录页
- 账户封面（支持图片 / GIF 动图）、个性签名、头像
- 密码「记住我」经 Windows DPAPI 加密保护
- Steam 风格启动加载页（头像 + 旋转进度环 + 封面背景）

### ⚡ 效率与专注
- **沉浸模式**：`F11` 一键全屏专注，`Esc` 退出
- **数字花园**：`Ctrl+Shift+G` 花园视图
- **本地 RAG 问答**：`Ctrl+K` 输入 `?问题`，在全部笔记中检索相关段落作答
- **每周报告**：每周一启动自动生成
- **全局截图**：`Ctrl+Shift+A`
- 生产力快捷键：`Ctrl+N` / `Ctrl+O` / `Ctrl+S` / `Ctrl+1~5`

### 🎨 UI / 系统集成
- Google Material 浅色 / 深色主题，Fusion 风格，全局毛玻璃与圆角菜单
- 托盘图标（跟随系统浅色 / 深色主题）、单实例（重复启动自动前置已有窗口）
- 自动保存、文件日志（可在设置中开关）、自动启动开关
- 在线更新检查（版本对比 + 下载安装）、UI 缩放、多语言框架
- GPU 硬件加速渲染管线（桌面 OpenGL，ANGLE/D3D11 回退，4x MSAA）

---

## 技术栈

| 层面 | 选型 |
|------|------|
| 语言 / 标准 | C++23（`-std=c++23`，禁用 GNU 扩展） |
| GUI | Qt 6.x（Widgets / Multimedia / Network / OpenGL / OpenGLWidgets / Concurrent / Sql / PrintSupport） |
| 存储 | SQLite WAL（书库、曲库、素材库、笔记） |
| 音频 | miniaudio 无缝播放引擎、CUE 解析、ReplayGain、FFmpeg（libav*）解码与元数据 |
| 构建 | CMake 3.16+ + Ninja，MinGW GCC 13.1（Qt 6.11.2 mingw_64） |
| Windows 集成 | DWM 亚克力毛玻璃 / 圆角（dwmapi）、DPAPI 密码保护（crypt32）、WASAPI（winmm/ole32）、托盘、单实例、开机自启 |
| 部署 | windeployqt + `package.ps1` 打包便携版 |

## 构建

依赖：Qt 6.x（MinGW 或 MSVC），CMake 3.16+，Ninja。

### 一键构建（Windows / MinGW）

```bat
build.bat
```

Qt 安装路径不同时，编辑 `build.bat` 中的路径即可。

### 手动构建

```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_PREFIX_PATH=<path-to-qt> -DCMAKE_BUILD_TYPE=Release ..
ninja
windeployqt --release LiyuYou.exe
```

### 打包便携版

```powershell
./package.ps1
```

产物为 `里与由_v1.0.0_portable.zip`（内含 `里与由.exe` 及全部 Qt / FFmpeg 运行库）。

## 目录结构

```
.
├── PureRefCanvas/          # 主程序源码（CMake 工程，产物名 LiyuYou.exe，发布名「里与由」）
│   ├── CMakeLists.txt      # 构建配置（C++23、Qt 组件、Windows 链接库）
│   ├── main.cpp            # 入口：设置迁移、硬件加速、单实例、登录、加载页
│   ├── mainwindow.*        # 主窗口：工作区切换、菜单、拖放、快捷键
│   ├── canvasview.*        # QGraphicsView 画布：平移 / 缩放 / 拖放
│   ├── canvasscene.*       # QGraphicsScene 场景：元素、选择、序列化
│   ├── imageitem.*         # 图片元素：缩放 / 旋转手柄
│   ├── booklibrary.*       # 书库（SQLite WAL）
│   ├── readerwidget.*      # 阅读器
│   ├── notewidget.*        # 笔记（富文本、双链、大纲、卡片）
│   ├── musicplayer.*       # 音乐播放（Gapless 引擎、CUE、曲库）
│   ├── materiallibrary.*   # 素材库
│   ├── spacemanager.*      # 空间系统
│   ├── userauth.*          # 账户认证
│   ├── settingsdialog.*    # 设置中心
│   ├── themeapplier.*      # 主题
│   ├── traycontroller.*    # 托盘
│   ├── autosavecontroller.*# 自动保存
│   ├── updatechecker.*     # 更新检查
│   ├── package.ps1         # 便携版打包脚本
│   └── build/              # 构建输出（不入库）
├── release/                # 发布部署产物（里与由.exe + 运行库，不入库）
├── _auth_test/             # 账户模块无头测试
├── _note_test/             # 笔记模块无头测试
├── _reader_test/           # 阅读模块无头测试
├── _tray_test/             # 托盘模块无头测试
├── _ui_test/               # UI 无头测试
└── AGENTS.md               # 项目协作规范
```

## 主要快捷键

| 功能 | 快捷键 |
|------|--------|
| 画布平移 | 拖拽空白 / `Space`+拖拽 / 中键拖拽 |
| 画布缩放 | 滚轮 / `+` `-` |
| 适应视图 | `F` / `Ctrl+F` |
| 沉浸模式 | `F11`（`Esc` 退出） |
| 数字花园 | `Ctrl+Shift+G` |
| 全局搜索 / RAG 问答 | `Ctrl+K`（`?问题` 触发本地问答） |
| 全局截图 | `Ctrl+Shift+A` |
| 快速新建 / 打开 / 保存 | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` |
| 快速切换工作区 | `Ctrl+1` ~ `Ctrl+5` |

完整快捷键见应用内「设置 → 快捷键」。

---

> 本项目数据全部本地存储；账户系统用于多设备体验，密码等敏感信息经系统 DPAPI 保护。
