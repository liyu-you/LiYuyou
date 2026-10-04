#include "mainwindow.h"
#include "updatechecker.h"
#include "canvasscene.h"
#include "canvasview.h"
#include "notewidget.h"
#include "imageitem.h"
#include "imageformats.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QImageReader>
#include <QPixmap>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QCloseEvent>
#include <QShortcut>
#include <QProcess>
#include <QWidgetAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTextStream>
#include <QTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QGraphicsItem>
#include <QStandardPaths>
#include <QColorDialog>
#include <QComboBox>
#include <QActionGroup>
#include <QToolButton>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include "materialwidget.h"
#include "contentbrowserpanel.h"
#include "materiallibrary.h"
#include "userauth.h"
#include "imageio.h"

#include <QSaveFile>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include "accountdialog.h"
#include "readerwidget.h"
#include "musicplayer.h"
#include "settingsdialog.h"
#include <QSettings>

// ========== Windows Acrylic Blur (毛玻璃效果) ==========
#ifdef Q_OS_WIN
enum AccentState {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
};

struct AccentPolicy {
    AccentState AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WindowCompositionAttributeData {
    DWORD Attribute;
    PVOID Data;
    ULONG DataSize;
};

typedef BOOL (WINAPI *PFN_SetWindowCompositionAttribute)(HWND, WindowCompositionAttributeData*);

static void enableAcrylicBlur(HWND hwnd, bool enable, DWORD gradientColor = 0xAA1B2838)
{
    HMODULE hUser = GetModuleHandle(L"user32.dll");
    if (!hUser) return;
    PFN_SetWindowCompositionAttribute SetWindowCompositionAttribute =
        (PFN_SetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!SetWindowCompositionAttribute) return;

    AccentPolicy accent = {enable ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_DISABLED, 0, gradientColor, 0};
    WindowCompositionAttributeData data = {19, &accent, sizeof(accent)};
    SetWindowCompositionAttribute(hwnd, &data);
}

// Windows 11: rounded window corners via DWM (silent no-op on Windows 10)
static void enableRoundedCorners(HWND hwnd)
{
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    PFN_DwmSetWindowAttribute DwmSetWindowAttribute =
        (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
    if (!DwmSetWindowAttribute) return;
    const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33; // window corner preference
    const DWORD DWMWCP_ROUND = 2;                    // round the corners
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));
}
#endif

// ========== Global Search Dialog (Ctrl+K) ==========
class GlobalSearchDialog : public QDialog {
    Q_OBJECT
public:
    GlobalSearchDialog(MainWindow *mw, QWidget *parent = nullptr)
        : QDialog(parent), m_mainWindow(mw)
    {
        setWindowTitle("全局搜索");
        setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(560, 420);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Container with rounded corners
        QWidget *container = new QWidget;
        container->setStyleSheet("background: #16202d; border: 1px solid #2a475e; border-radius: 10px;");
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(12, 12, 12, 12);
        containerLayout->setSpacing(8);

        // Search input
        m_searchEdit = new QLineEdit;
        m_searchEdit->setPlaceholderText("搜索笔记、素材、书籍...  (Ctrl+K)");
        m_searchEdit->setStyleSheet(R"(
            QLineEdit { background: #0f1923; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 6px; padding: 10px 14px; font-size: 14px; }
            QLineEdit:focus { border-color: #66c0f4; }
        )");
        containerLayout->addWidget(m_searchEdit);

        // Results list
        m_resultList = new QListWidget;
        m_resultList->setStyleSheet(R"(
            QListWidget { background: transparent; border: none; outline: none; }
            QListWidget::item { padding: 8px 12px; border-radius: 5px; margin: 1px 0; }
            QListWidget::item:selected { background: #2a475e; }
            QListWidget::item:hover:!selected { background: rgba(42,71,94,0.4); }
        )");
        m_resultList->setSpacing(2);
        containerLayout->addWidget(m_resultList, 1);

        // Hint label
        QLabel *hint = new QLabel("↑↓ 选择  Enter 打开  Esc 关闭");
        hint->setStyleSheet("color: #5a6b7a; font-size: 11px; padding: 2px 4px;");
        hint->setAlignment(Qt::AlignCenter);
        containerLayout->addWidget(hint);

        mainLayout->addWidget(container);

        connect(m_searchEdit, &QLineEdit::textChanged, this, &GlobalSearchDialog::doSearch);
        connect(m_resultList, &QListWidget::itemDoubleClicked, this, &GlobalSearchDialog::openResult);
        connect(m_resultList, &QListWidget::itemActivated, this, &GlobalSearchDialog::openResult);

        m_searchEdit->setFocus();
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape) { accept(); return; }
        if (e->key() == Qt::Key_Down && m_searchEdit->hasFocus()) {
            m_resultList->setFocus();
            if (m_resultList->count() > 0) m_resultList->setCurrentRow(0);
            return;
        }
        QDialog::keyPressEvent(e);
    }

private slots:
    void doSearch(const QString &text) {
        m_resultList->clear();
        if (text.trimmed().isEmpty()) return;
        QString query = text.toLower();

        // Search notes
        if (m_mainWindow->m_noteWidget) {
            for (const NoteData &note : m_mainWindow->m_noteWidget->allNotes()) {
                if (note.title.toLower().contains(query) || note.content.toLower().contains(query)) {
                    QListWidgetItem *item = new QListWidgetItem("📝  " + note.title);
                    item->setData(Qt::UserRole, "note");
                    item->setData(Qt::UserRole + 1, note.id);
                    item->setToolTip("笔记: " + note.title);
                    item->setForeground(QColor("#c7d5e0"));
                    m_resultList->addItem(item);
                }
            }
        }

        // Search materials
        if (m_mainWindow->m_sharedLibrary) {
            for (const MaterialItem &mat : m_mainWindow->m_sharedLibrary->allItems()) {
                if (mat.fileName.toLower().contains(query)) {
                    QString typeLabel = mat.type == AssetImage ? "🖼" : mat.type == AssetVideo ? "🎬" : "📄";
                    QListWidgetItem *item = new QListWidgetItem(typeLabel + "  " + mat.baseName());
                    item->setData(Qt::UserRole, "material");
                    item->setData(Qt::UserRole + 1, mat.fullPath(m_mainWindow->m_sharedLibrary->assetsPath()));
                    item->setToolTip("素材: " + mat.fileName);
                    item->setForeground(QColor("#c7d5e0"));
                    m_resultList->addItem(item);
                }
            }
        }

        // Search books (scan directory)
        QString bookRoot = m_mainWindow->m_booksDir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PureRefBooks"
            : m_mainWindow->m_booksDir;
        QDir bookDir(bookRoot);
        if (bookDir.exists()) {
            for (const QFileInfo &fi : bookDir.entryInfoList(QDir::Files)) {
                if (fi.fileName().toLower().contains(query)) {
                    QListWidgetItem *item = new QListWidgetItem("📖  " + fi.completeBaseName());
                    item->setData(Qt::UserRole, "book");
                    item->setData(Qt::UserRole + 1, fi.absoluteFilePath());
                    item->setToolTip("书籍: " + fi.fileName());
                    item->setForeground(QColor("#c7d5e0"));
                    m_resultList->addItem(item);
                }
            }
        }

        if (m_resultList->count() == 0) {
            QListWidgetItem *empty = new QListWidgetItem("  未找到匹配结果");
            empty->setForeground(QColor("#5a6b7a"));
            empty->setFlags(Qt::NoItemFlags);
            m_resultList->addItem(empty);
        }
    }

    void openResult(QListWidgetItem *item) {
        if (!item) return;
        QString type = item->data(Qt::UserRole).toString();
        QString data = item->data(Qt::UserRole + 1).toString();

        if (type == "note" && m_mainWindow->m_noteWidget) {
            m_mainWindow->switchToPage(1);
            m_mainWindow->m_noteWidget->openNote(data);
        } else if (type == "material") {
            m_mainWindow->switchToPage(2);
        } else if (type == "book") {
            m_mainWindow->switchToPage(3);
        }
        accept();
    }

private:
    MainWindow *m_mainWindow;
    QLineEdit *m_searchEdit;
    QListWidget *m_resultList;
};

// ========== Command Palette (Ctrl+Shift+P) ==========
class CommandPaletteDialog : public QDialog {
    Q_OBJECT
public:
    struct Command {
        QString icon;
        QString name;
        QString shortcut;
        std::function<void()> action;
    };

    CommandPaletteDialog(MainWindow *mw, QWidget *parent = nullptr)
        : QDialog(parent), m_mainWindow(mw)
    {
        setWindowTitle("命令面板");
        setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(520, 480);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        QWidget *container = new QWidget;
        container->setStyleSheet("background: #16202d; border: 1px solid #2a475e; border-radius: 10px;");
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(12, 12, 12, 12);
        containerLayout->setSpacing(8);

        // Title
        QLabel *title = new QLabel("⚡ 命令面板");
        title->setStyleSheet("color: #66c0f4; font-size: 13px; font-weight: 600; padding: 0 4px;");
        containerLayout->addWidget(title);

        // Search input
        m_searchEdit = new QLineEdit;
        m_searchEdit->setPlaceholderText("输入命令名称...  (Ctrl+Shift+P)");
        m_searchEdit->setStyleSheet(R"(
            QLineEdit { background: #0f1923; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 6px; padding: 10px 14px; font-size: 14px; }
            QLineEdit:focus { border-color: #66c0f4; }
        )");
        containerLayout->addWidget(m_searchEdit);

        // Command list
        m_commandList = new QListWidget;
        m_commandList->setStyleSheet(R"(
            QListWidget { background: transparent; border: none; outline: none; }
            QListWidget::item { padding: 9px 12px; border-radius: 5px; margin: 1px 0; }
            QListWidget::item:selected { background: #2a475e; }
            QListWidget::item:hover:!selected { background: rgba(42,71,94,0.4); }
        )");
        m_commandList->setSpacing(2);
        containerLayout->addWidget(m_commandList, 1);

        // Hint
        QLabel *hint = new QLabel("↑↓ 选择  Enter 执行  Esc 关闭");
        hint->setStyleSheet("color: #5a6b7a; font-size: 11px; padding: 2px 4px;");
        hint->setAlignment(Qt::AlignCenter);
        containerLayout->addWidget(hint);

        mainLayout->addWidget(container);

        connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::filterCommands);
        connect(m_commandList, &QListWidget::itemDoubleClicked, this, &CommandPaletteDialog::executeCommand);
        connect(m_commandList, &QListWidget::itemActivated, this, &CommandPaletteDialog::executeCommand);

        buildCommands();
        filterCommands("");
        m_searchEdit->setFocus();
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape) { accept(); return; }
        if (e->key() == Qt::Key_Down && m_searchEdit->hasFocus()) {
            m_commandList->setFocus();
            if (m_commandList->count() > 0) m_commandList->setCurrentRow(0);
            return;
        }
        if (e->key() == Qt::Key_Up && m_commandList->hasFocus() && m_commandList->currentRow() == 0) {
            m_searchEdit->setFocus();
            return;
        }
        QDialog::keyPressEvent(e);
    }

private slots:
    void filterCommands(const QString &text) {
        m_commandList->clear();
        QString query = text.toLower().trimmed();
        for (int i = 0; i < m_commands.size(); ++i) {
            const Command &cmd = m_commands[i];
            if (query.isEmpty() || cmd.name.toLower().contains(query)) {
                QListWidgetItem *item = new QListWidgetItem(cmd.icon + "  " + cmd.name);
                item->setData(Qt::UserRole, i);
                item->setForeground(QColor("#c7d5e0"));
                item->setFont(QFont("Microsoft YaHei", 12));
                if (!cmd.shortcut.isEmpty()) {
                    item->setToolTip("快捷键: " + cmd.shortcut);
                }
                m_commandList->addItem(item);
            }
        }
        if (m_commandList->count() > 0) m_commandList->setCurrentRow(0);
    }

    void executeCommand(QListWidgetItem *item) {
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_commands.size()) {
            accept();
            m_commands[idx].action();
        }
    }

private:
    void buildCommands() {
        MainWindow *mw = m_mainWindow;
        m_commands = {
            {"🎨", "切换到画布", "Ctrl+1", [mw]() { mw->switchToPage(0); }},
            {"📝", "切换到笔记", "Ctrl+2", [mw]() { mw->switchToPage(1); }},
            {"📁", "切换到库", "Ctrl+3", [mw]() { mw->switchToPage(2); }},
            {"📖", "切换到阅读", "Ctrl+4", [mw]() { mw->switchToPage(3); }},
            {"🎵", "切换到音乐", "Ctrl+5", [mw]() { mw->switchToPage(4); }},
            {"🔍", "全局搜索", "Ctrl+K", [mw]() { mw->openGlobalSearch(); }},
            {"⚙", "偏好设置", "Ctrl+,", [mw]() { mw->openSettings(); }},
            {"📥", "导入素材到库", "", [mw]() { if (mw->m_materialWidget) mw->m_materialWidget->importFiles(); }},
            {"📄", "新建笔记", "", [mw]() { if (mw->m_noteWidget) mw->m_noteWidget->newNote(); }},
            {"▶", "播放/暂停音乐", "Ctrl+P", [mw]() { if (mw->m_musicWidget) mw->m_musicWidget->togglePlay(); }},
            {"⏮", "上一首", "Ctrl+←", [mw]() { if (mw->m_musicWidget) mw->m_musicWidget->playPrev(); }},
            {"⏭", "下一首", "Ctrl+→", [mw]() { if (mw->m_musicWidget) mw->m_musicWidget->playNext(); }},
            {"👤", "账户信息", "", [mw]() { mw->showAccountDialog(); }},
            {"🚪", "退出程序", "Ctrl+Q", [mw]() { qApp->quit(); }},
        };
    }

    MainWindow *m_mainWindow;
    QLineEdit *m_searchEdit;
    QListWidget *m_commandList;
    QList<Command> m_commands;
};

// ========== Shortcut Panel (Ctrl+/) ==========
class ShortcutPanelDialog : public QDialog {
    Q_OBJECT
public:
    ShortcutPanelDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("快捷键参考");
        setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(560, 560);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        QWidget *container = new QWidget;
        container->setStyleSheet("background: #16202d; border: 1px solid #2a475e; border-radius: 10px;");
        QVBoxLayout *cl = new QVBoxLayout(container);
        cl->setContentsMargins(16, 14, 16, 14);
        cl->setSpacing(6);

        // Title
        QLabel *title = new QLabel("⌨  快捷键参考");
        title->setStyleSheet("color: #66c0f4; font-size: 15px; font-weight: 700; padding: 0 0 6px 0;");
        cl->addWidget(title);

        // Search filter
        m_filterEdit = new QLineEdit;
        m_filterEdit->setPlaceholderText("搜索快捷键...");
        m_filterEdit->setStyleSheet(R"(
            QLineEdit { background: #0f1923; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 6px; padding: 8px 12px; font-size: 13px; }
            QLineEdit:focus { border-color: #66c0f4; }
        )");
        cl->addWidget(m_filterEdit);

        // Shortcut list
        m_list = new QListWidget;
        m_list->setStyleSheet(R"(
            QListWidget { background: transparent; border: none; outline: none; }
            QListWidget::item { padding: 5px 8px; border-radius: 4px; }
            QListWidget::item:selected { background: #2a475e; }
        )");
        m_list->setSpacing(1);
        cl->addWidget(m_list, 1);

        // Hint
        QLabel *hint = new QLabel("Esc 关闭");
        hint->setStyleSheet("color: #5a6b7a; font-size: 11px; padding: 4px 0 0 0;");
        hint->setAlignment(Qt::AlignCenter);
        cl->addWidget(hint);

        mainLayout->addWidget(container);

        connect(m_filterEdit, &QLineEdit::textChanged, this, &ShortcutPanelDialog::filterList);
        buildList();
        m_filterEdit->setFocus();
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape) { accept(); return; }
        QDialog::keyPressEvent(e);
    }

private slots:
    void filterList(const QString &text) {
        QString q = text.toLower().trimmed();
        for (int i = 0; i < m_list->count(); ++i) {
            QListWidgetItem *item = m_list->item(i);
            bool isHeader = item->data(Qt::UserRole).toBool();
            if (isHeader) { item->setHidden(false); continue; }
            bool match = q.isEmpty() || item->text().toLower().contains(q);
            item->setHidden(!match);
        }
    }

private:
    void addHeader(const QString &name) {
        QListWidgetItem *h = new QListWidgetItem("  " + name);
        h->setData(Qt::UserRole, true);
        h->setForeground(QColor("#66c0f4"));
        h->setFont(QFont("Microsoft YaHei", 11, QFont::Bold));
        h->setFlags(Qt::NoItemFlags);
        h->setBackground(QColor(22, 32, 45));
        m_list->addItem(h);
    }

    void addShortcut(const QString &name, const QString &shortcut) {
        QListWidgetItem *item = new QListWidgetItem("    " + name);
        item->setData(Qt::UserRole, false);
        item->setForeground(QColor("#c7d5e0"));
        item->setFont(QFont("Microsoft YaHei", 12));
        // Right-aligned shortcut text via tooltip + custom widget
        QWidget *w = new QWidget;
        QHBoxLayout *hl = new QHBoxLayout(w);
        hl->setContentsMargins(0, 0, 8, 0);
        QLabel *nameLbl = new QLabel(name);
        nameLbl->setStyleSheet("color: #c7d5e0; font-size: 12px;");
        QLabel *scLbl = new QLabel(shortcut);
        scLbl->setStyleSheet("color: #66c0f4; font-size: 11px; font-family: Consolas; background: rgba(102,192,244,0.1); padding: 2px 8px; border-radius: 4px;");
        hl->addWidget(nameLbl);
        hl->addStretch();
        hl->addWidget(scLbl);
        m_list->addItem(item);
        m_list->setItemWidget(item, w);
        item->setSizeHint(QSize(0, 30));
    }

    void buildList() {
        addHeader("通用");
        addShortcut("命令面板", "Ctrl+Shift+P");
        addShortcut("全局搜索", "Ctrl+K");
        addShortcut("快捷键参考", "Ctrl+/");
        addShortcut("偏好设置", "Ctrl+,");
        addShortcut("退出程序", "Ctrl+Q");

        addHeader("工作区切换");
        addShortcut("切换到画布", "Ctrl+1");
        addShortcut("切换到笔记", "Ctrl+2");
        addShortcut("切换到库", "Ctrl+3");
        addShortcut("切换到阅读", "Ctrl+4");
        addShortcut("切换到音乐", "Ctrl+5");
addShortcut("切换侧边栏", "Ctrl+8");

        addHeader("画布操作");
        addShortcut("拖拽平移", "空格 + 拖拽 / 中键");
        addShortcut("缩放", "Ctrl + 滚轮");
        addShortcut("实际大小", "Ctrl+0");
        addShortcut("适应窗口", "Ctrl+F");
        addShortcut("全选", "Ctrl+A");
        addShortcut("删除选中", "Delete");
        addShortcut("复制", "Ctrl+C");
        addShortcut("粘贴", "Ctrl+V");
        addShortcut("复制选中项", "Ctrl+D");
        addShortcut("置于顶层", "Ctrl+]");
        addShortcut("置于底层", "Ctrl+[");
        addShortcut("锁定/解锁", "Ctrl+L");

        addHeader("音乐控制");
        addShortcut("播放/暂停", "Ctrl+P");
        addShortcut("上一首", "Ctrl+←");
        addShortcut("下一首", "Ctrl+→");
        addShortcut("音量+", "Ctrl+↑");
        addShortcut("音量-", "Ctrl+↓");
    }

    QLineEdit *m_filterEdit;
    QListWidget *m_list;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("里与由");
    resize(1280, 800);
    setAcceptDrops(true);

    loadSettings();

    setupNavigation();
    createStatusBar();
    statusBar()->hide();
    setupPages();
    qApp->processEvents();
    createActions();
    qApp->processEvents();
    createMenus();
    qApp->processEvents();
    createTrayIcon();
    qApp->processEvents();

    // Global search shortcut (Ctrl+K)
    QShortcut *searchSc = new QShortcut(QKeySequence("Ctrl+K"), this);
    searchSc->setContext(Qt::ApplicationShortcut);
    connect(searchSc, &QShortcut::activated, this, &MainWindow::openGlobalSearch);

    // Command palette shortcut (Ctrl+Shift+P)
    QShortcut *cmdSc = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    cmdSc->setContext(Qt::ApplicationShortcut);
    connect(cmdSc, &QShortcut::activated, this, &MainWindow::openCommandPalette);

// Shortcut panel shortcut (Ctrl+/)
QShortcut *scPanelSc = new QShortcut(QKeySequence("Ctrl+/"), this);
scPanelSc->setContext(Qt::ApplicationShortcut);
connect(scPanelSc, &QShortcut::activated, this, &MainWindow::openShortcutPanel);

// Sidebar toggle (Ctrl+8) — folds/unfolds the current page's left panel
QShortcut *sidebarSc = new QShortcut(QKeySequence("Ctrl+8"), this);
sidebarSc->setContext(Qt::ApplicationShortcut);
connect(sidebarSc, &QShortcut::activated, this, &MainWindow::toggleSidebar);

    applyTheme(m_currentTheme);
    applyFontSize(m_fontSize);
    applyThumbnailSize(m_thumbnailSize);
    applyNoteBgColor(m_noteBgColor);
    applyReaderColors(m_readerBgColor, m_readerTextColor);
    applyBgImage(m_bgImagePath);
    applyEditorSettings(m_noteDefaultFont, m_noteDefaultFontSize);
    applyTransparencySettings();
    applyMenuBarMode();
    setupAutoSaveTimer();

    // Startup restore
    if (m_startupRestore && m_lastPage >= 0 && m_lastPage < m_stack->count()) {
        switchToPage(m_lastPage);
    }

    // Enable acrylic blur (毛玻璃效果) — Windows 10/11
#ifdef Q_OS_WIN
    winId(); // force HWND creation
    applyAcrylic();
#endif

    // Auto-check for updates on startup (delayed to avoid impacting launch)
    QSettings settings("PureRefCanvas", "里与由");
    if (settings.value("update/autoCheck", true).toBool()) {
        QString updateUrl = settings.value("update/url", "").toString();
        if (!updateUrl.isEmpty()) {
            QTimer::singleShot(5000, this, [this, updateUrl]() {
                UpdateChecker *checker = new UpdateChecker(this);
                connect(checker, &UpdateChecker::updateAvailable, this, [this, checker](const UpdateChecker::UpdateInfo &info) {
                    QString msg = QString("发现新版本 v%1\n\n更新内容：\n%2\n\n是否立即下载更新？").arg(info.version.toString(), info.releaseNotes);
                    if (QMessageBox::question(this, "发现更新", msg) == QMessageBox::Yes) {
                        QString savePath = QDir::tempPath() + "/里与由_update.exe";
                        QProgressDialog *progress = new QProgressDialog("正在下载更新...", "取消", 0, 100, this);
                        progress->setWindowModality(Qt::WindowModal);
                        progress->setMinimumDuration(0);
                        connect(checker, &UpdateChecker::downloadProgress, progress, [progress](qint64 received, qint64 total) {
                            if (total > 0) progress->setValue(qRound(received * 100.0 / total));
                        });
                        connect(checker, &UpdateChecker::updateDownloaded, this, [this, progress](const QString &path) {
                            progress->close();
                            if (QMessageBox::question(this, "下载完成", "更新已下载完成，是否立即安装并重启？") == QMessageBox::Yes) {
                                UpdateChecker::installAndRestart(path);
                            }
                        });
                        connect(checker, &UpdateChecker::downloadFailed, this, [progress](const QString &err) {
                            progress->close();
                            QMessageBox::warning(nullptr, "下载失败", err);
                        });
                        connect(progress, &QProgressDialog::canceled, checker, &QObject::deleteLater);
                        checker->downloadUpdate(info.downloadUrl, savePath);
                    } else {
                        checker->deleteLater();
                    }
                });
                connect(checker, &UpdateChecker::noUpdateAvailable, checker, &QObject::deleteLater);
                connect(checker, &UpdateChecker::checkFailed, checker, &QObject::deleteLater);
                checker->checkForUpdates(updateUrl);
            });
        }
    }
}

void MainWindow::setupNavigation()
{
    QToolBar *navToolBar = addToolBar("导航");
    m_navToolBar = navToolBar;
    navToolBar->setMovable(false);
    navToolBar->setFixedHeight(56);
    navToolBar->setStyleSheet(R"(
        QToolBar {
            background: #FFFFFF;
            border: none;
            border-bottom: 1px solid #E5E7EB;
            padding: 0px;
            spacing: 0px;
        }
        QToolBar::separator { width: 0; }
    )");

    // Single container widget for all navbar content
    QWidget *navContainer = new QWidget;
    navContainer->setStyleSheet("background: transparent;");
    navContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *navLayout = new QHBoxLayout(navContainer);
    navLayout->setContentsMargins(16, 8, 16, 8);
    navLayout->setSpacing(8);

    // ===== Brand / Logo area =====
    QWidget *brandWidget = new QWidget;
    brandWidget->setStyleSheet("background: transparent;");
    QHBoxLayout *brandLayout = new QHBoxLayout(brandWidget);
    brandLayout->setContentsMargins(0, 0, 12, 0);
    brandLayout->setSpacing(8);
    // ☰ 半隐藏菜单按钮（开启"菜单栏半隐藏"后才可见）
    m_menuHamburger = new QToolButton(brandWidget);
    m_menuHamburger->setText("☰");
    m_menuHamburger->setToolTip("菜单（文件/编辑/视图/帮助）");
    m_menuHamburger->setAutoRaise(true);
    m_menuHamburger->setCursor(Qt::PointingHandCursor);
    m_menuHamburger->setFixedSize(30, 30);
    m_menuHamburger->setStyleSheet(
        "QToolButton { font-size: 16px; color: #374151; border: none; border-radius: 6px; background: transparent; }"
        "QToolButton:hover { background: rgba(0,0,0,0.06); }");
    connect(m_menuHamburger, &QToolButton::clicked, this, [this]() {
        QMenu popup;
        QMenu *f = popup.addMenu("文件(&F)");
        f->addAction(m_actions.value(0));  f->addSeparator();
        f->addAction(m_actions.value(1)); f->addAction(m_actions.value(2)); f->addAction(m_actions.value(3));
        f->addSeparator(); f->addAction(m_actions.value(4)); f->addSeparator(); f->addAction(m_actions.value(5));
        QMenu *e = popup.addMenu("编辑(&E)");
        e->addAction(m_actions.value(6)); e->addSeparator();
        e->addAction(m_actions.value(8)); e->addAction(m_actions.value(9)); e->addAction(m_actions.value(10));
        e->addSeparator(); e->addAction(m_actions.value(7)); e->addSeparator();
        e->addAction(m_actions.value(11)); e->addAction(m_actions.value(12)); e->addSeparator();
        e->addAction(m_actions.value(18)); e->addSeparator(); e->addAction(m_actions.value(19));
        QMenu *v = popup.addMenu("视图(&V)");
        v->addAction(m_actions.value(13)); v->addAction(m_actions.value(14));
        v->addAction(m_actions.value(15)); v->addAction(m_actions.value(16));
        QMenu *h = popup.addMenu("帮助(&H)");
        h->addAction(m_actions.value(17));
        popup.exec(m_menuHamburger->mapToGlobal(QPoint(0, m_menuHamburger->height())));
    });
    brandLayout->addWidget(m_menuHamburger);
    // Brand button — doubles as the sidebar toggle (ZCode-style: click the
    // logo to fold/unfold the current page's left panel, or press Ctrl+8).
    // Uses the same artwork as the desktop shortcut (app.ico): take the
    // largest frame and downscale it ourselves with smooth transformation —
    // QIcon::pixmap at small sizes picks a crisper-but-muddier path.
    m_brandBtn = new QToolButton(brandWidget);
    m_brandBtn->setFixedSize(32, 32);
    m_brandBtn->setIconSize(QSize(30, 30));
    m_brandBtn->setCursor(Qt::PointingHandCursor);
    m_brandBtn->setToolTip("切换侧边栏 (Ctrl+8)");
    const QPixmap brandArt = QIcon(":/app.ico").pixmap(64, 64);
    if (!brandArt.isNull()) {
        m_brandBtn->setIcon(QIcon(brandArt.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        m_brandBtn->setStyleSheet(
            "QToolButton { border: none; border-radius: 8px; background: transparent; }"
            "QToolButton:hover { background: rgba(0,0,0,0.08); }"
            "QToolButton:pressed { background: rgba(0,0,0,0.16); }");
    } else { // icon resource missing → branded letter chip fallback
        m_brandBtn->setText("里");
        m_brandBtn->setStyleSheet(
            "QToolButton { background: #2FA66A; color: #FFFFFF; border: none; border-radius: 8px;"
            " font-size: 14px; font-weight: 600; }"
            "QToolButton:hover { background: #27945D; }"
            "QToolButton:pressed { background: #1F7A4B; }");
    }
    connect(m_brandBtn, &QToolButton::clicked, this, &MainWindow::toggleSidebar);
    brandLayout->addWidget(m_brandBtn);
    // Brand name
    QLabel *brandName = new QLabel("里与由");
    brandName->setStyleSheet("color: #1F2933; font-size: 14px; font-weight: 500; letter-spacing: 1px;");
    brandLayout->addWidget(brandName);
    navLayout->addWidget(brandWidget);

    // Separator after brand
    QFrame *brandSep = new QFrame;
    brandSep->setFixedWidth(1);
    brandSep->setStyleSheet("background: #E5E7EB;");
    brandSep->setContentsMargins(0, 4, 0, 4);
    navLayout->addWidget(brandSep);
    navLayout->addSpacing(8);

    // ===== Navigation buttons =====
    QStringList names = {"画布", "笔记", "库", "阅读", "音乐"};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *btn = new QPushButton(names[i], navContainer);
        btn->setFixedHeight(32);
        btn->setMinimumWidth(72);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(R"(
            QPushButton {
                background: transparent;
                color: #6B7280;
                border: none;
                border-radius: 13px;
                padding: 6px 16px;
                font-size: 13px;
                font-weight: 500;
                qproperty-alignment: AlignCenter;
            }
            QPushButton:hover {
                background: #F3F4F6;
                color: #111827;
            }
            QPushButton:pressed {
                background: #E5E7EB;
                color: #111827;
            }
        )");
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchToPage(i); });
        m_navButtons.append(btn);
        navLayout->addWidget(btn);
    }

    // Spacer
    navLayout->addStretch();

    // User info widget (avatar + nickname + dropdown arrow)
    QWidget *userBtn = new QWidget(navContainer);
    userBtn->setCursor(Qt::PointingHandCursor);
    userBtn->setStyleSheet(R"(
        QWidget { background: transparent; border-radius: 20px; }
        QWidget:hover { background: #F3F4F6; }
    )");
    QHBoxLayout *userBtnLayout = new QHBoxLayout(userBtn);
    userBtnLayout->setContentsMargins(8, 3, 12, 3);
    userBtnLayout->setSpacing(8);

    m_userAvatar = new QLabel;
    m_userAvatar->setFixedSize(30, 30);
    m_userAvatar->setScaledContents(true);
    m_userAvatar->setStyleSheet("border-radius: 15px; border: 1px solid #2FA66A;");
    userBtnLayout->addWidget(m_userAvatar);

    m_userNickname = new QLabel;
    m_userNickname->setStyleSheet("color: #1F2933; font-size: 13px; font-weight: 500;");
    m_userNickname->setMinimumWidth(40);
    userBtnLayout->addWidget(m_userNickname);

    QLabel *arrow = new QLabel("▾");
    arrow->setStyleSheet("color: #6B7280; font-size: 10px;");
    userBtnLayout->addWidget(arrow);

    // User popup menu
    QMenu *userMenu = new QMenu(this);
    userMenu->setStyleSheet(R"(
        QMenu { background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 8px; padding: 6px; }
        QMenu::item { color: #1F2933; padding: 8px 20px; border-radius: 5px; font-size: 13px; }
        QMenu::item:hover { background: #F3F4F6; color: #2FA66A; }
        QMenu::separator { height: 1px; background: #E5E7EB; margin: 4px 8px; }
    )");
    QAction *accountAct = userMenu->addAction("账号信息");
    QAction *settingsAct = userMenu->addAction("偏好设置");
    userMenu->addSeparator();
    QAction *logoutAct = userMenu->addAction("退出登录");
    connect(accountAct, &QAction::triggered, this, &MainWindow::showAccountDialog);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    connect(logoutAct, &QAction::triggered, this, &MainWindow::logoutUser);

    userBtn->installEventFilter(this);
    m_userWidget = userBtn;
    m_userMenu = userMenu;

    navLayout->addWidget(userBtn);

    navToolBar->addWidget(navContainer);
}

void MainWindow::setupPages()
{
    // Shared library for all workspaces
    QString matRoot = m_materialDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PureRefLibrary"
        : m_materialDir;
    m_sharedLibrary = new MaterialLibrary(matRoot, this);
    qApp->processEvents();

    m_stack = new QStackedWidget(this);
    m_stack->setStyleSheet("background: transparent; border: none;");
    qApp->processEvents();

    // ===== Canvas page =====
    CanvasScene *scene = new CanvasScene(this);
    m_canvasView = new CanvasView(scene, this);
    // Auto-save serializes the entire canvas, so arm it only when something
    // actually changed; the timer then skips an untouched canvas entirely.
    connect(scene, &QGraphicsScene::changed, this, [this]() { m_canvasDirty = true; });
    if (!m_canvasBgPath.isEmpty())
        m_canvasView->setBackgroundImage(m_canvasBgPath);
    // Apply saved minimap opacity
    if (m_canvasView->minimap())
        m_canvasView->minimap()->setPanelOpacity(m_minimapOpacity / 100.0);
    // Apply saved zoom bar opacity
    if (m_canvasView)
        m_canvasView->setZoomBarOpacity(m_zoomBarOpacity / 100.0);
    // Apply saved minimap visibility & size
    if (m_canvasView) {
        m_canvasView->setMinimapVisible(m_minimapVisible);
        const qreal scale = (m_minimapSize == "small") ? 0.8 : (m_minimapSize == "large") ? 1.3 : 1.0;
        m_canvasView->setMinimapScale(scale);
        m_canvasView->setMinimapThrottleEnabled(m_minimapThrottle);
    }
    connect(m_canvasView, &CanvasView::zoomChanged, this, &MainWindow::updateZoomLabel);
    connect(m_canvasView, &CanvasView::fileDropped, this, &MainWindow::loadImages);
    connect(scene, &CanvasScene::itemCountChanged, this, &MainWindow::updateItemCount);
    connect(scene, &CanvasScene::selectionChangedInfo, this, &MainWindow::updateSelectionCount);
    m_stack->addWidget(m_canvasView);
    qApp->processEvents();

    // ===== Note page =====
    QString noteRoot = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PureRefNotes";
    m_noteWidget = new NoteWidget(noteRoot, this);
    if (!m_noteBgPath.isEmpty())
        m_noteWidget->setBackgroundImage(m_noteBgPath);
    connect(m_noteWidget, &NoteWidget::modificationChanged, this, &MainWindow::onNoteModified);
    // Keep the sidebar visible: it hosts search / categories / folders / sort / export
    // (mainstream note apps keep a navigation sidebar on the note page).
    m_stack->addWidget(m_noteWidget);
    qApp->processEvents();

    // ===== Material library page =====
    m_materialWidget = new MaterialWidget(matRoot, this);
    if (!m_libraryBgPath.isEmpty())
        m_materialWidget->setBackgroundImage(m_libraryBgPath);
    m_materialWidget->hideSidebar();
    connect(m_materialWidget, &MaterialWidget::addToCanvasRequested, this, [this](const QString &filePath) {
        if (!QFile::exists(filePath)) return;
        QPixmap pix(filePath);
        if (pix.isNull()) return;
        // Switch to canvas page
        switchToPage(0);
        // Add image to canvas center
        CanvasScene *s = canvasScene();
        if (s) {
            QPointF center = m_canvasView->mapToScene(m_canvasView->viewport()->rect().center());
            s->addImage(pix, center);
        }
    });
    m_stack->addWidget(m_materialWidget);
    qApp->processEvents();

    // ===== Reader page =====
    QString bookRoot = m_booksDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PureRefBooks"
        : m_booksDir;
    m_readerWidget = new ReaderWidget(bookRoot, this);
    if (!m_readerShelfBgPath.isEmpty())
        m_readerWidget->setShelfBackground(m_readerShelfBgPath);
    if (!m_readerBgPath.isEmpty())
        m_readerWidget->setBackgroundImage(m_readerBgPath);
    m_readerWidget->applyReaderSettings(m_readerTheme, m_readerFont,
                                        m_readerFontSize, m_readerLineSpacing, m_readerMargin);
    // Custom reader colors only override the theme when they differ from defaults.
    if (m_readerBgColor.isValid() && m_readerTextColor.isValid()
        && (m_readerBgColor != QColor(255, 250, 240) || m_readerTextColor != QColor(60, 50, 40)))
        m_readerWidget->setCustomColors(m_readerBgColor, m_readerTextColor);
    // Persist theme / background / custom-color changes made inside the reader.
    connect(m_readerWidget, &ReaderWidget::themeChanged, this, [this](const QString &key) {
        m_readerTheme = key;
        m_readerBgColor = QColor(255, 250, 240);   // preset theme replaces custom colors
        m_readerTextColor = QColor(60, 50, 40);
        saveSettings();
    });
    connect(m_readerWidget, &ReaderWidget::backgroundImageChanged, this, [this](const QString &path) {
        m_readerBgPath = path;
        saveSettings();
    });
    connect(m_readerWidget, &ReaderWidget::customColorsChanged, this, [this](const QColor &bg, const QColor &text) {
        m_readerBgColor = bg;
        m_readerTextColor = text;
        saveSettings();
    });
    connect(m_readerWidget, &ReaderWidget::fontFamilyChanged, this, [this](const QString &displayName) {
        m_readerFont = displayName;
        saveSettings();
    });
    connect(m_readerWidget, &ReaderWidget::lineSpacingChanged, this, [this](qreal spacing) {
        m_readerLineSpacing = spacing;
        saveSettings();
    });
    m_stack->addWidget(m_readerWidget);
    qApp->processEvents();

    // ===== Music page =====
    QString musicRoot = m_musicDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PureRefMusic"
        : m_musicDir;
    m_musicWidget = new MusicPlayerWidget(musicRoot, this);
    if (!m_musicBgPath.isEmpty())
        m_musicWidget->setBackgroundImage(m_musicBgPath);
    m_stack->addWidget(m_musicWidget);
    applyMusicShortcuts();
    qApp->processEvents();

    // ===== Global Content Browser (shared left panel) =====
    m_contentBrowser = new ContentBrowserPanel(m_sharedLibrary, this);
    qApp->processEvents();
    connect(m_contentBrowser, &ContentBrowserPanel::folderChanged, this, [this](const QString &folder) {
        if (m_materialWidget) m_materialWidget->setCurrentFolder(folder);
    });
    connect(m_contentBrowser, &ContentBrowserPanel::imageActivated, this, [this](const QString &filePath) {
        CanvasScene *s = canvasScene();
        if (s && QFile::exists(filePath)) {
            QPixmap pix(filePath);
            if (!pix.isNull()) {
                QPointF center = m_canvasView->mapToScene(m_canvasView->viewport()->rect().center());
                s->addImage(pix, center);
            }
        }
    });

    QWidget *central = new QWidget;
    QHBoxLayout *centralLayout = new QHBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_contentBrowser);
    centralLayout->addWidget(m_stack, 1);
    // The canvas page's QGraphicsView advertises a 1920x1080 size hint, which
    // inflates QStackedWidget's size hint and caps the reader/other pages at a
    // fixed width — the reader never grew with the window. Ignore the stack's
    // hint so it always fills the remaining central space.
    m_stack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    setCentralWidget(central);
    qApp->processEvents();

    // Apply startup page
    int initialPage = 0;
    if (m_startupPage == "last") initialPage = m_lastPage;
    else if (m_startupPage == "canvas") initialPage = 0;
    else if (m_startupPage == "note") initialPage = 1;
    else if (m_startupPage == "library") initialPage = 2;
    else if (m_startupPage == "reader") initialPage = 3;
    else if (m_startupPage == "music") initialPage = 4;
    switchToPage(qBound(0, initialPage, 4));

    // Apply default music volume and play mode
    if (m_musicWidget) {
        m_musicWidget->setVolume(m_defaultVolume);
        m_musicWidget->setInitialPlayMode(m_defaultPlayMode);
    }
}

// 折叠/展开当前页的左侧栏（ZCode 风格 Ctrl+8）：
// 画布/库页折叠全局素材浏览器，笔记页折叠内容浏览器侧栏；阅读/音乐页布局
// 不同（无独立左侧栏），按键不动作。
void MainWindow::toggleSidebar()
{
    switch (currentPage()) {
    case CanvasPage:
    case MaterialPage:
        if (m_contentBrowser) {
            if (m_contentBrowser->isVisible())
                m_contentBrowser->hide();
            else
                m_contentBrowser->show();
        }
        break;
    case NotePage:
        if (m_noteWidget)
            m_noteWidget->toggleSidebar();
        break;
    case BookPage:
    case MusicPage:
        break;
    }
}

void MainWindow::switchToPage(int index)
{
    if (index < 0 || index >= m_stack->count()) return;
    if (m_stack->currentIndex() == index) return;

    m_stack->setCurrentIndex(index);

    // The reader, music and notes pages have their own dedicated sidebars; hide
    // the material browser (left panel) so those shelves get the full width.
    if (m_contentBrowser) {
        m_contentBrowser->setShowImages(index == 0);
        if (index == BookPage || index == MusicPage || index == NotePage)
            m_contentBrowser->hide();
        else
            m_contentBrowser->show();
    }

    updateNavButtons();
    updateWindowTitle();
    updateStatusForPage();
}

void MainWindow::updateNavButtons()
{
    int cur = m_stack->currentIndex();
    for (int i = 0; i < m_navButtons.size(); ++i) {
        bool selected = (i == cur);
        m_navButtons[i]->setProperty("selected", selected);
        m_navButtons[i]->setStyleSheet(selected ? R"(
            QPushButton {
                background: #EAF3DE;
                color: #27500A;
                border: 1px solid #2FA66A;
                border-radius: 13px;
                padding: 6px 16px;
                font-size: 13px;
                font-weight: 500;
                qproperty-alignment: AlignCenter;
            }
            QPushButton:pressed {
                background: #C0DD97;
                color: #27500A;
            }
        )" : R"(
            QPushButton {
                background: transparent;
                color: #6B7280;
                border: 1px solid transparent;
                border-radius: 13px;
                padding: 6px 16px;
                font-size: 13px;
                font-weight: 500;
                qproperty-alignment: AlignCenter;
            }
            QPushButton:hover {
                background: #F3F4F6;
                color: #111827;
            }
            QPushButton:pressed {
                background: #E5E7EB;
                color: #111827;
            }
        )");
    }
}

void MainWindow::updateStatusForPage()
{
    if (!m_pageTypeLabel) return;
    PageType page = currentPage();
    switch (page) {
    case CanvasPage:
        m_pageTypeLabel->setText("画布");
        break;
    case NotePage:
        m_pageTypeLabel->setText("笔记");
        m_zoomLabel->setText("");
        m_itemCountLabel->setText("");
        m_selectionLabel->setText("");
        break;
    case MaterialPage:
        m_pageTypeLabel->setText("库");
        m_zoomLabel->setText("");
        m_itemCountLabel->setText("");
        m_selectionLabel->setText("");
        break;
    case BookPage:
        m_pageTypeLabel->setText("阅读");
        m_zoomLabel->setText("");
        m_itemCountLabel->setText("");
        m_selectionLabel->setText("");
        break;
    case MusicPage:
        m_pageTypeLabel->setText("音乐");
        m_zoomLabel->setText("");
        m_itemCountLabel->setText("");
        m_selectionLabel->setText("");
        break;
    }
}

MainWindow::PageType MainWindow::currentPage() const
{
    return (PageType)m_stack->currentIndex();
}

CanvasView *MainWindow::canvasView() const
{
    return (currentPage() == CanvasPage) ? m_canvasView : nullptr;
}

NoteWidget *MainWindow::noteWidget() const
{
    return (currentPage() == NotePage) ? m_noteWidget : nullptr;
}

CanvasScene *MainWindow::canvasScene() const
{
    CanvasView *v = canvasView();
    return v ? qobject_cast<CanvasScene*>(v->scene()) : nullptr;
}

void MainWindow::updateWindowTitle()
{
    QStringList names = {"画布", "笔记", "库", "阅读", "音乐"};
    QString title = QString("里与由 - %1").arg(names[m_stack->currentIndex()]);
    if (!m_currentUser.username.isEmpty()) {
        QString displayName = m_currentUser.nickname.isEmpty() ? m_currentUser.username : m_currentUser.nickname;
        title += QString("  [%1]").arg(displayName);
    }
    setWindowTitle(title);
}

// ========== File Operations ==========

void MainWindow::openImages()
{
    if (currentPage() == CanvasPage) {
        QStringList formats;
        for (const auto &fmt : QImageReader::supportedImageFormats())
            formats << QString("*.%1").arg(QString::fromUtf8(fmt));
        QString filter = "图片 (" + formats.join(" ") + ");;所有文件 (*.*)";
        QStringList files = QFileDialog::getOpenFileNames(this, "导入图片", QString(), filter);
        if (!files.isEmpty()) loadImages(files);
    } else if (currentPage() == NotePage) {
        loadFile();
    }
}

// Atomic file write: QSaveFile writes a temporary and renames it into place, so a
// crash mid-save can no longer leave a truncated canvas or note behind.
static bool writeAtomically(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

void MainWindow::saveCurrent()
{
    if (currentPage() == CanvasPage) {
        if (!m_canvasFilePath.isEmpty()) {
            CanvasScene *s = canvasScene();
            if (s) {
                if (writeAtomically(m_canvasFilePath, s->saveState())) {
                    m_canvasDirty = false;
                    statusBar()->showMessage("已保存", 2000);
                }
            }
        } else {
            saveAsCurrent();
        }
    } else if (currentPage() == NotePage) {
        if (!m_noteFilePath.isEmpty()) {
            QFile file(m_noteFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                if (m_noteFilePath.endsWith(".html", Qt::CaseInsensitive))
                    out << m_noteWidget->editor()->toHtml();
                else
                    out << m_noteWidget->editor()->toPlainText();
                file.close();
                m_noteWidget->setModified(false);
                statusBar()->showMessage("已保存", 2000);
            }
        } else {
            saveAsCurrent();
        }
    }
}

void MainWindow::saveAsCurrent()
{
    if (currentPage() == CanvasPage) {
        QString fn = QFileDialog::getSaveFileName(this, "保存画布", QString(), "里与由 画布 (*.prc);;所有文件 (*.*)");
        if (!fn.isEmpty()) {
            CanvasScene *s = canvasScene();
            if (s) {
                if (writeAtomically(fn, s->saveState())) {
                    m_canvasDirty = false;
                    m_canvasFilePath = fn;
                    statusBar()->showMessage("已保存到 " + fn, 3000);
                }
            }
        }
    } else if (currentPage() == NotePage) {
        QString fn = QFileDialog::getSaveFileName(this, "保存笔记", QString(), "HTML (*.html);;Markdown (*.md);;文本 (*.txt)");
        if (!fn.isEmpty()) {
            QFile file(fn);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                if (fn.endsWith(".html", Qt::CaseInsensitive))
                    out << m_noteWidget->editor()->toHtml();
                else
                    out << m_noteWidget->editor()->toPlainText();
                file.close();
                m_noteFilePath = fn;
                m_noteWidget->setModified(false);
                statusBar()->showMessage("已保存到 " + fn, 3000);
            }
        }
    }
}

void MainWindow::loadFile()
{
    if (currentPage() == CanvasPage) {
        QString fn = QFileDialog::getOpenFileName(this, "加载画布", QString(), "里与由 画布 (*.prc);;所有文件 (*.*)");
        if (!fn.isEmpty()) {
            CanvasScene *s = canvasScene();
            if (s) {
                QFile file(fn);
                if (file.open(QIODevice::ReadOnly)) {
                    s->loadState(file.readAll());
                    file.close();
                    m_canvasDirty = false; // what is in memory now matches a file on disk
                    m_canvasFilePath = fn;
                    statusBar()->showMessage("已加载 " + fn, 3000);
                }
            }
        }
    } else if (currentPage() == NotePage) {
        QString fn = QFileDialog::getOpenFileName(this, "打开笔记", QString(), "HTML (*.html);;Markdown (*.md);;文本 (*.txt);;所有文件 (*.*)");
        if (!fn.isEmpty()) {
            QFile file(fn);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                QString content = in.readAll();
                file.close();
                if (fn.endsWith(".html", Qt::CaseInsensitive))
                    m_noteWidget->editor()->setHtml(content);
                else
                    m_noteWidget->editor()->setPlainText(content);
                m_noteFilePath = fn;
                m_noteWidget->setModified(false);
                statusBar()->showMessage("已打开 " + fn, 3000);
            }
        }
    }
}

void MainWindow::exportImage()
{
    if (currentPage() == CanvasPage) {
        QString fn = QFileDialog::getSaveFileName(this, "导出图片", QString(), "PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*.*)");
        if (!fn.isEmpty()) {
            CanvasScene *s = canvasScene();
            if (s) {
                QRectF rect = s->itemsBoundingRect();
                QPixmap pix(rect.size().toSize());
                pix.fill(Qt::transparent);
                QPainter painter(&pix);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setRenderHint(QPainter::SmoothPixmapTransform);
                s->render(&painter, QRectF(), rect);
                painter.end();
                pix.save(fn);
                statusBar()->showMessage("已导出到 " + fn, 3000);
            }
        }
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, "关于 里与由",
        "<h2>里与由 工作区</h2>"
        "<p>一个集成多种工具的创意工作区</p>"
        "<p><b>画布：</b>无限画布，支持图片导入、移动、缩放、旋转、多选、层级、保存/加载</p>"
        "<p><b>笔记：</b>富文本编辑器，支持字体、颜色、列表、引用、代码块、图片、链接，保存 HTML/MD/TXT</p>"
        "<p><b>库：</b>本地素材管理，文件夹+标签，缩略图网格，搜索，双击预览大图</p>"
        "<p><b>阅读：</b>电子书阅读器，分页阅读，字体调节，4种主题，目录，书签，进度自动保存</p>"
        "<p><b>音乐：</b>音乐播放器，歌单管理，播放控制，音量，进度，4种播放模式</p>"
        "<p>顶部导航切换工作区 · 滚轮缩放画布 · 拖拽导入图片</p>");
}

// ========== Edit Operations ==========

void MainWindow::deleteSelected()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->removeSelectedItems();
    } else if (currentPage() == NotePage) {
        m_noteWidget->editor()->textCursor().removeSelectedText();
    }
}

void MainWindow::selectAll()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->selectAllItems();
    } else if (currentPage() == NotePage) {
        m_noteWidget->editor()->selectAll();
    }
}

void MainWindow::copySelected()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->copySelected();
    } else if (currentPage() == NotePage) {
        m_noteWidget->editor()->copy();
    }
}

void MainWindow::pasteContent()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->paste();
    } else if (currentPage() == NotePage) {
        m_noteWidget->editor()->paste();
    }
}

void MainWindow::duplicateSelected()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->duplicateSelected();
    }
}

void MainWindow::bringToFront()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->bringSelectedToFront();
    }
}

void MainWindow::sendToBack()
{
    if (currentPage() == CanvasPage) {
        CanvasScene *s = canvasScene();
        if (s) s->sendSelectedToBack();
    }
}

void MainWindow::toggleLock()
{
    if (currentPage() != CanvasPage) return;
    CanvasScene *s = canvasScene();
    if (!s) return;
    auto items = s->selectedImageItems();
    if (items.isEmpty()) return;
    // If any selected item is unlocked, lock all; otherwise unlock all
    bool anyUnlocked = false;
    for (auto *item : items) {
        if (!item->isLocked()) { anyUnlocked = true; break; }
    }
    for (auto *item : items) {
        item->setLocked(anyUnlocked);
    }
    statusBar()->showMessage(anyUnlocked ? "已锁定选中图片" : "已解锁选中图片", 2000);
}

// ========== View ==========

void MainWindow::zoomIn()
{
    CanvasView *v = canvasView();
    if (v) v->zoomIn();
}

void MainWindow::zoomOut()
{
    CanvasView *v = canvasView();
    if (v) v->zoomOut();
}

void MainWindow::zoomActual()
{
    CanvasView *v = canvasView();
    if (v) v->zoomActual();
}

void MainWindow::zoomToFit()
{
    CanvasView *v = canvasView();
    if (v) v->zoomToFit();
}

// ========== Status ==========

void MainWindow::updateZoomLabel(qreal zoom)
{
    m_zoomLabel->setText(QString("缩放: %1%").arg(qRound(zoom * 100)));
}

void MainWindow::updateItemCount(int count)
{
    m_itemCountLabel->setText(QString("图片: %1").arg(count));
}

void MainWindow::updateSelectionCount(int count)
{
    m_selectionLabel->setText(count > 0 ? QString("选中: %1").arg(count) : "");
}

void MainWindow::onNoteModified(bool modified)
{
    Q_UNUSED(modified);
}

// ========== Drag & Drop ==========

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) paths << path;
    }
    if (!paths.isEmpty()) handleFileDrop(paths);
}

void MainWindow::handleFileDrop(const QStringList &paths)
{
    QStringList images;
    for (const QString &p : paths) {
        QFileInfo fi(p);
        if (ImageFormats::isReadableExtension(fi.suffix()))
            images << p;
    }

    if (currentPage() == CanvasPage && !images.isEmpty()) {
        loadImages(images);
    } else if (currentPage() == MaterialPage && !images.isEmpty()) {
        m_materialWidget->library()->importFiles(images, "未分类");
    }
}

void MainWindow::loadImages(const QStringList &paths)
{
    CanvasScene *s = canvasScene();
    if (!s || paths.isEmpty()) return;

    // Decoding used to happen here, on the UI thread: measured 145 ms per 12 MP
    // photo, so dropping 20 photos froze the window for ~2.8 s. The pixels are
    // now produced in a worker thread; only the final conversion to QPixmap (which
    // must happen on the GUI thread) and the item insertion run here.
    statusBar()->showMessage(QString("正在导入 %1 张图片…").arg(paths.size()));

    auto *watcher = new QFutureWatcher<QList<QImage>>(this);
    connect(watcher, &QFutureWatcher<QList<QImage>>::finished, this,
            [this, watcher, paths]() {
        watcher->deleteLater();
        CanvasScene *scene = canvasScene();
        const QList<QImage> images = watcher->result();
        if (!scene) return;

        int added = 0, x = 0, y = 0;
        for (const QImage &img : images) {
            if (img.isNull()) continue;
            if (scene->addImage(QPixmap::fromImage(img), QPointF(x, y))) {
                x += 20;
                y += 20;
                ++added;
            }
        }
        statusBar()->showMessage(QString("已导入 %1 张图片").arg(added), 3000);
    });
    watcher->setFuture(QtConcurrent::run([paths]() {
        QList<QImage> images;
        images.reserve(paths.size());
        for (const QString &path : paths)
            images.append(ImageIO::decodeFull(path));
        return images;
    }));
}

// ========== Actions / Menus / Toolbar / StatusBar ==========

void MainWindow::createActions()
{
    QAction *openAct = new QAction("导入图片", this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openImages);

    QAction *saveAct = new QAction("保存", this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveCurrent);

    QAction *saveAsAct = new QAction("另存为", this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::saveAsCurrent);

    QAction *loadAct = new QAction("加载画布", this);
    connect(loadAct, &QAction::triggered, this, &MainWindow::loadFile);

    QAction *exportAct = new QAction("导出图片", this);
    connect(exportAct, &QAction::triggered, this, &MainWindow::exportImage);

    QAction *quitAct = new QAction("退出", this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QAction *selectAllAct = new QAction("全选", this);
    selectAllAct->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAct, &QAction::triggered, this, &MainWindow::selectAll);

    QAction *deleteAct = new QAction("删除", this);
    deleteAct->setShortcut(QKeySequence::Delete);
    connect(deleteAct, &QAction::triggered, this, &MainWindow::deleteSelected);

    QAction *copyAct = new QAction("复制", this);
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::copySelected);

    QAction *pasteAct = new QAction("粘贴", this);
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::pasteContent);

    QAction *duplicateAct = new QAction("复制副本", this);
    duplicateAct->setShortcut(QKeySequence("Ctrl+D"));
    connect(duplicateAct, &QAction::triggered, this, &MainWindow::duplicateSelected);

    QAction *frontAct = new QAction("置于顶层", this);
    connect(frontAct, &QAction::triggered, this, &MainWindow::bringToFront);

    QAction *backAct = new QAction("置于底层", this);
    connect(backAct, &QAction::triggered, this, &MainWindow::sendToBack);

    QAction *zoomInAct = new QAction("放大", this);
    zoomInAct->setShortcut(QKeySequence("Ctrl++"));
    connect(zoomInAct, &QAction::triggered, this, &MainWindow::zoomIn);

    QAction *zoomOutAct = new QAction("缩小", this);
    zoomOutAct->setShortcut(QKeySequence("Ctrl+-"));
    connect(zoomOutAct, &QAction::triggered, this, &MainWindow::zoomOut);

    QAction *actualSizeAct = new QAction("实际大小", this);
    actualSizeAct->setShortcut(QKeySequence("Ctrl+0"));
    connect(actualSizeAct, &QAction::triggered, this, &MainWindow::zoomActual);

    QAction *fitAct = new QAction("适应窗口", this);
    fitAct->setShortcut(QKeySequence("Ctrl+F"));
    connect(fitAct, &QAction::triggered, this, &MainWindow::zoomToFit);

    QAction *aboutAct = new QAction("关于", this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    QAction *lockAct = new QAction("锁定/解锁", this);
    lockAct->setShortcut(QKeySequence("Ctrl+L"));
    connect(lockAct, &QAction::triggered, this, &MainWindow::toggleLock);

    QAction *settingsAct = new QAction("偏好设置", this);
    settingsAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);

    m_actions = {
        openAct, saveAct, saveAsAct, loadAct, exportAct, quitAct,
        selectAllAct, deleteAct, copyAct, pasteAct, duplicateAct, frontAct, backAct,
        zoomInAct, zoomOutAct, actualSizeAct, fitAct, aboutAct, lockAct, settingsAct
    };
}

void MainWindow::createMenus()
{
    auto &a = m_actions;

    QMenu *fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(a[0]);  // Open
    fileMenu->addSeparator();
    fileMenu->addAction(a[1]);  // Save
    fileMenu->addAction(a[2]);  // Save As
    fileMenu->addAction(a[3]);  // Load
    fileMenu->addSeparator();
    fileMenu->addAction(a[4]);  // Export
    fileMenu->addSeparator();
    fileMenu->addAction(a[5]);  // Quit

    QMenu *editMenu = menuBar()->addMenu("编辑(&E)");
    editMenu->addAction(a[6]);  // Select All
    editMenu->addSeparator();
    editMenu->addAction(a[8]);  // Copy
    editMenu->addAction(a[9]);  // Paste
    editMenu->addAction(a[10]); // Duplicate
    editMenu->addSeparator();
    editMenu->addAction(a[7]);  // Delete
    editMenu->addSeparator();
    editMenu->addAction(a[11]); // Front
    editMenu->addAction(a[12]); // Back
    editMenu->addSeparator();
    editMenu->addAction(a[18]); // Lock
    editMenu->addSeparator();
    editMenu->addAction(a[19]); // Settings

    QMenu *viewMenu = menuBar()->addMenu("视图(&V)");
    viewMenu->addAction(a[13]); // Zoom In
    viewMenu->addAction(a[14]); // Zoom Out
    viewMenu->addAction(a[15]); // Actual Size
    viewMenu->addAction(a[16]); // Fit

    QMenu *helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(a[17]); // About
}

void MainWindow::createStatusBar()
{
    m_pageTypeLabel = new QLabel("画布");
    m_zoomLabel = new QLabel("缩放: 100%");
    m_itemCountLabel = new QLabel("图片: 0");
    m_selectionLabel = new QLabel("");

    statusBar()->addWidget(m_pageTypeLabel);
    statusBar()->addWidget(m_zoomLabel);
    statusBar()->addWidget(m_itemCountLabel);
    statusBar()->addPermanentWidget(m_selectionLabel);
}

void MainWindow::createTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("系统托盘不可用");
        return;
    }

    m_tray = new QSystemTrayIcon(this);

    // Load icon: try app icon, fallback to standard icon
    QIcon icon = windowIcon();
    if (icon.isNull()) {
        icon = style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    if (icon.isNull()) {
        QPixmap pm(32, 32);
        pm.fill(QColor(102, 192, 244));
        icon = QIcon(pm);
    }
    m_tray->setIcon(icon);
    m_tray->setToolTip("里与由 工作区");

    QMenu *trayMenu = new QMenu(this);
    trayMenu->setStyleSheet(R"(
        QMenu {
            background: #16202d;
            color: #c7d5e0;
            border: 1px solid #2a475e;
            border-radius: 8px;
            padding: 6px;
            font-size: 13px;
        }
        QMenu::item {
            padding: 8px 28px 8px 12px;
            border-radius: 5px;
            margin: 1px 2px;
        }
        QMenu::item:selected {
            background: #2a475e;
            color: #66c0f4;
        }
        QMenu::separator {
            height: 1px;
            background: #2a475e;
            margin: 5px 10px;
        }
        QMenu::icon {
            padding-left: 8px;
        }
    )");

    // Header label
    QWidgetAction *headerAction = new QWidgetAction(trayMenu);
    QWidget *headerWidget = new QWidget;
    headerWidget->setFixedHeight(36);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 4, 12, 4);
    QLabel *headerIcon = new QLabel;
    headerIcon->setFixedSize(20, 20);
    QPixmap headerPm = icon.pixmap(20, 20);
    if (!headerPm.isNull()) headerIcon->setPixmap(headerPm);
    headerLayout->addWidget(headerIcon);
    QLabel *headerLabel = new QLabel("里与由 工作区");
    headerLabel->setStyleSheet("color: #66c0f4; font-size: 13px; font-weight: 600;");
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    headerAction->setDefaultWidget(headerWidget);
    trayMenu->addAction(headerAction);
    trayMenu->addSeparator();

    QAction *showAct = trayMenu->addAction("📂  显示主窗口");
    showAct->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(showAct, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });

    QAction *hideAct = trayMenu->addAction("📥  隐藏到托盘");
    hideAct->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(hideAct, &QAction::triggered, this, [this]() {
        hide();
    });

    trayMenu->addSeparator();

    QAction *settingsAct = trayMenu->addAction("⚙  偏好设置...");
    settingsAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAct, &QAction::triggered, this, [this]() {
        showNormal(); raise(); activateWindow();
        QTimer::singleShot(100, this, &MainWindow::openSettings);
    });

    QAction *accountAct = trayMenu->addAction("👤  账户信息");
    connect(accountAct, &QAction::triggered, this, [this]() {
        showNormal(); raise(); activateWindow();
        showAccountDialog();
    });

    trayMenu->addSeparator();

    QAction *quitAct = trayMenu->addAction("🚪  退出");
    quitAct->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAct, &QAction::triggered, this, [this]() {
        m_trayInitialized = false;
        QApplication::quit();
    });

    m_tray->setContextMenu(trayMenu);

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (isVisible()) {
                hide();
            } else {
                showNormal();
                raise();
                activateWindow();
            }
        }
    });

    m_tray->show();
    m_trayInitialized = true;
    qDebug("托盘图标已创建");
}

// ========== Close ==========

bool MainWindow::maybeSave()
{
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_closeBehavior == "tray" && m_trayInitialized && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        m_tray->showMessage("里与由 工作区", "程序已最小化到托盘，点击托盘图标恢复。", QSystemTrayIcon::Information, 3000);
        event->ignore();
        return;
    }
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

// ========== Settings ==========

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    dlg.setTheme(m_currentTheme);
    dlg.setFontSize(m_fontSize);
    dlg.setCloseBehavior(m_closeBehavior);
    dlg.setStartupRestore(m_startupRestore);
    dlg.setAutoSave(m_autoSave);
    dlg.setAutoSaveInterval(m_autoSaveInterval);
    dlg.setThumbnailSize(m_thumbnailSize);
    dlg.setUiScale(m_uiScale);
    dlg.setLanguage(m_language);
    dlg.setHardwareAcceleration(m_hardwareAcceleration);
    dlg.setSingleInstance(m_singleInstance);
    dlg.setAcrylicEnabled(m_acrylicEnabled);
    dlg.setAcrylicOpacity(m_acrylicOpacity);
    dlg.setMenuBarTransparent(m_menuBarTransparent);
    dlg.setMenuBarAutoHide(m_menuBarAutoHide);
    dlg.setNavToolbarTransparent(m_navToolbarTransparent);
    dlg.setLibraryStatusBarTransparent(m_libraryStatusBarTransparent);
    dlg.setLibraryGridTransparent(m_libraryGridTransparent);
    dlg.setContentBrowserTransparent(m_contentBrowserTransparent);
    dlg.setMusicPanelsTransparent(m_musicPanelsTransparent);
    dlg.setNotePanelsTransparent(m_notePanelsTransparent);
    dlg.setReaderPanelsTransparent(m_readerPanelsTransparent);
    dlg.setMaterialDir(m_materialDir);
    dlg.setBooksDir(m_booksDir);
    dlg.setMusicDir(m_musicDir);
    dlg.setNoteBgColor(m_noteBgColor);
    dlg.setReaderBgColor(m_readerBgColor);
    dlg.setReaderTextColor(m_readerTextColor);
    dlg.setBgImagePath(m_bgImagePath);
    dlg.setReaderShelfBgPath(m_readerShelfBgPath);
    dlg.setReaderBgPath(m_readerBgPath);
    dlg.setCanvasBgPath(m_canvasBgPath);
    dlg.setMinimapOpacity(m_minimapOpacity);
    dlg.setZoomBarOpacity(m_zoomBarOpacity);
    dlg.setMinimapEnabled(m_minimapVisible);
    dlg.setMinimapSize(m_minimapSize);
    dlg.setMinimapThrottle(m_minimapThrottle);
    dlg.setNoteBgPath(m_noteBgPath);
    dlg.setLibraryBgPath(m_libraryBgPath);
    dlg.setMusicBgPath(m_musicBgPath);
    dlg.setEditorFont(m_noteDefaultFont);
    dlg.setEditorFontSize(m_noteDefaultFontSize);
    dlg.setShortcutPlayPause(m_scPlayPause);
    dlg.setShortcutPrev(m_scPrev);
    dlg.setShortcutNext(m_scNext);
    dlg.setShortcutVolumeUp(m_scVolUp);
    dlg.setShortcutVolumeDown(m_scVolDown);
    dlg.setTimerShutdownEnabled(m_timerShutdownEnabled);
    dlg.setTimerShutdownHours(m_timerShutdownHours);
    dlg.setTimerShutdownMinutes(m_timerShutdownMinutes);
    dlg.setShutdownPCEnabled(m_shutdownPCEnabled);
    dlg.setStartupPage(m_startupPage);
    dlg.setDefaultSort(m_defaultSort);
    dlg.setShowFileExtension(m_showFileExt);
    dlg.setDoubleClickBehavior(m_doubleClickBehavior);
    dlg.setReaderTheme(m_readerTheme);
    dlg.setReaderFont(m_readerFont);
    dlg.setReaderLineSpacing(m_readerLineSpacing);
    dlg.setReaderFontSize(m_readerFontSize);
    dlg.setReaderMargin(m_readerMargin);
    dlg.setDefaultVolume(m_defaultVolume);
    dlg.setDefaultPlayMode(m_defaultPlayMode);
    dlg.setShowLyrics(m_showLyrics);

    if (dlg.exec() == QDialog::Accepted) {
        m_currentTheme = dlg.theme();
        m_fontSize = dlg.fontSize();
        m_closeBehavior = dlg.closeBehavior();
        m_startupRestore = dlg.startupRestore();
        m_autoSave = dlg.autoSave();
        m_autoSaveInterval = dlg.autoSaveInterval();
        m_thumbnailSize = dlg.thumbnailSize();
        m_noteBgColor = dlg.noteBgColor();
        m_readerBgColor = dlg.readerBgColor();
        m_readerTextColor = dlg.readerTextColor();
        m_bgImagePath = dlg.bgImagePath();
        m_readerShelfBgPath = dlg.readerShelfBgPath();
        m_readerBgPath = dlg.readerBgPath();
        m_canvasBgPath = dlg.canvasBgPath();
        m_minimapOpacity = dlg.minimapOpacity();
        m_zoomBarOpacity = dlg.zoomBarOpacity();
        m_minimapVisible = dlg.minimapEnabled();
        m_minimapSize = dlg.minimapSize();
        m_minimapThrottle = dlg.minimapThrottle();
        m_noteBgPath = dlg.noteBgPath();
        m_libraryBgPath = dlg.libraryBgPath();
        m_musicBgPath = dlg.musicBgPath();
        m_noteDefaultFont = dlg.editorFont();
        m_noteDefaultFontSize = dlg.editorFontSize();

        // Immediate-apply settings
        applyTheme(m_currentTheme);
        applyFontSize(m_fontSize);
        applyThumbnailSize(m_thumbnailSize);
        applyNoteBgColor(m_noteBgColor);
        applyReaderColors(m_readerBgColor, m_readerTextColor);
        applyBgImage(m_bgImagePath);
        if (m_readerWidget) m_readerWidget->setShelfBackground(m_readerShelfBgPath);
        if (m_readerWidget) m_readerWidget->setBackgroundImage(m_readerBgPath);
        if (m_canvasView) m_canvasView->setBackgroundImage(m_canvasBgPath);
        if (m_canvasView && m_canvasView->minimap())
            m_canvasView->minimap()->setPanelOpacity(m_minimapOpacity / 100.0);
        if (m_canvasView)
            m_canvasView->setZoomBarOpacity(m_zoomBarOpacity / 100.0);
        if (m_canvasView) {
            m_canvasView->setMinimapVisible(m_minimapVisible);
            const qreal scale = (m_minimapSize == "small") ? 0.8 : (m_minimapSize == "large") ? 1.3 : 1.0;
            m_canvasView->setMinimapScale(scale);
            m_canvasView->setMinimapThrottleEnabled(m_minimapThrottle);
        }
        if (m_noteWidget) m_noteWidget->setBackgroundImage(m_noteBgPath);
        if (m_materialWidget) m_materialWidget->setBackgroundImage(m_libraryBgPath);
        if (m_musicWidget) m_musicWidget->setBackgroundImage(m_musicBgPath);
        applyEditorSettings(m_noteDefaultFont, m_noteDefaultFontSize);

        // Music shortcuts
        m_scPlayPause = dlg.shortcutPlayPause();
        m_scPrev = dlg.shortcutPrev();
        m_scNext = dlg.shortcutNext();
        m_scVolUp = dlg.shortcutVolumeUp();
        m_scVolDown = dlg.shortcutVolumeDown();
        applyMusicShortcuts();

        // Timer shutdown
        m_timerShutdownEnabled = dlg.timerShutdownEnabled();
        m_timerShutdownHours = dlg.timerShutdownHours();
        m_timerShutdownMinutes = dlg.timerShutdownMinutes();
        m_shutdownPCEnabled = dlg.shutdownPCEnabled();
        applyTimerShutdown();

        // New settings
        m_startupPage = dlg.startupPage();
        m_defaultSort = dlg.defaultSort();
        m_showFileExt = dlg.showFileExtension();
        m_doubleClickBehavior = dlg.doubleClickBehavior();
        m_readerTheme = dlg.readerTheme();
        m_readerFont = dlg.readerFont();
        m_readerLineSpacing = dlg.readerLineSpacing();
        m_readerFontSize = dlg.readerFontSize();
        m_readerMargin = dlg.readerMargin();
        if (m_readerWidget) {
            m_readerWidget->applyReaderSettings(m_readerTheme, m_readerFont,
                                               m_readerFontSize, m_readerLineSpacing, m_readerMargin);
        }
        m_defaultVolume = dlg.defaultVolume();
        m_defaultPlayMode = dlg.defaultPlayMode();
        m_showLyrics = dlg.showLyrics();

        // Apply music settings immediately
        if (m_musicWidget) {
            m_musicWidget->setVolume(m_defaultVolume);
            m_musicWidget->setInitialPlayMode(m_defaultPlayMode);
        }
        setupAutoSaveTimer();

        // Restart-required settings (just save, prompt user)
        bool needRestart = (m_uiScale != dlg.uiScale())
            || (m_language != dlg.language())
            || (m_hardwareAcceleration != dlg.hardwareAcceleration())
            || (m_materialDir != dlg.materialDir())
            || (m_booksDir != dlg.booksDir())
            || (m_musicDir != dlg.musicDir());

        m_uiScale = dlg.uiScale();
        m_language = dlg.language();
        m_hardwareAcceleration = dlg.hardwareAcceleration();
        m_singleInstance = dlg.singleInstance();
        m_acrylicEnabled = dlg.acrylicEnabled();
        m_acrylicOpacity = dlg.acrylicOpacity();
        applyAcrylic();
        m_menuBarTransparent = dlg.menuBarTransparent();
        m_menuBarAutoHide = dlg.menuBarAutoHide();
        m_navToolbarTransparent = dlg.navToolbarTransparent();
        m_libraryStatusBarTransparent = dlg.libraryStatusBarTransparent();
        m_libraryGridTransparent = dlg.libraryGridTransparent();
        m_contentBrowserTransparent = dlg.contentBrowserTransparent();
        m_musicPanelsTransparent = dlg.musicPanelsTransparent();
        m_notePanelsTransparent = dlg.notePanelsTransparent();
        m_readerPanelsTransparent = dlg.readerPanelsTransparent();
        applyTransparencySettings();
        applyMenuBarMode();
        m_materialDir = dlg.materialDir();
        m_booksDir = dlg.booksDir();
        m_musicDir = dlg.musicDir();

        // Thumbnail cache cleared from settings → also drop in-memory caches and rebuild views
        if (dlg.cacheCleared()) {
            if (m_contentBrowser) { m_contentBrowser->clearThumbCache(); m_contentBrowser->refresh(); }
            if (m_materialWidget) m_materialWidget->clearThumbCache();
        }

        // Save update settings directly
        {
            QSettings us("PureRefCanvas", "里与由");
            us.setValue("update/url", dlg.updateUrl());
            us.setValue("update/autoCheck", dlg.autoUpdate());
        }

        saveSettings();

        if (needRestart) {
            QMessageBox::information(this, "设置已保存",
                "部分设置（界面缩放、语言、硬件加速、数据目录）需要重启程序后生效。");
        }
    }
}

void MainWindow::applyTheme(const QString &theme)
{
    // Theme transition overlay (safe method: fade overlay instead of screenshot)
    QWidget *overlay = nullptr;
    if (centralWidget() && isVisible() && m_currentTheme != theme) {
        overlay = new QWidget(centralWidget());
        overlay->setGeometry(centralWidget()->rect());
        overlay->setStyleSheet(m_currentTheme == "dark"
            ? "background: rgba(22,32,45,0.9);"
            : "background: rgba(240,242,245,0.9);");
        overlay->setWindowOpacity(0.0);
        overlay->show();
        overlay->raise();
        // Fade in overlay
        QPropertyAnimation *fadeIn = new QPropertyAnimation(overlay, "windowOpacity", this);
        fadeIn->setDuration(120);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start();
    }

    m_currentTheme = theme;
    if (theme == "light") {
        setStyleSheet(R"(
            QMainWindow { background-color: #f0f2f5; }
            QWidget { background-color: #f0f2f5; color: #1a1a1a; }
            QMenuBar { background-color: #ffffff; color: #333; border: none; border-bottom: 1px solid #e0e0e0; padding: 2px; font-size: 13px; }
            QMenuBar::item { background: transparent; padding: 6px 14px; border-radius: 3px; }
            QMenuBar::item:selected { background-color: #e8f0fe; color: #1a73e8; }
            QMenu { background-color: #ffffff; color: #333; border: 1px solid #e0e0e0; padding: 4px; font-size: 13px; }
            QMenu::item { padding: 6px 28px; border-radius: 3px; }
            QMenu::item:selected { background-color: #e8f0fe; color: #1a73e8; }
            QMenu::separator { height: 1px; background-color: #e0e0e0; margin: 4px 8px; }
            QToolBar { background-color: #ffffff; border: none; border-bottom: 1px solid #e0e0e0; padding: 4px 8px; spacing: 4px; }
            QToolBar QToolButton { background-color: transparent; color: #555; border: none; padding: 6px 12px; border-radius: 3px; font-size: 13px; }
            QToolBar QToolButton:hover { background-color: #e8f0fe; color: #1a73e8; }
            QToolBar QToolButton:pressed { background-color: #1a73e8; color: #ffffff; }
            QToolBar::separator { width: 1px; background-color: #e0e0e0; margin: 4px 6px; }
            QStatusBar { background-color: #ffffff; color: #666; border-top: 1px solid #e0e0e0; font-size: 12px; }
            QStatusBar QLabel { color: #666; padding: 0 10px; background-color: transparent; }
            QPushButton { background-color: #e8eaed; color: #333; border: none; padding: 6px 16px; border-radius: 3px; font-size: 13px; }
            QPushButton:hover { background-color: #d2d6db; color: #1a73e8; }
            QPushButton:pressed { background-color: #1a73e8; color: #ffffff; }
            QLineEdit, QTextEdit, QPlainTextEdit, QTextBrowser {
                background-color: #ffffff; color: #333;
                border: 1px solid #dadce0; border-radius: 3px; padding: 5px 8px; font-size: 13px;
                selection-background-color: #1a73e8; selection-color: #ffffff;
            }
            QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus { border-color: #1a73e8; }
            QListWidget, QTableWidget, QTreeWidget {
                background-color: #ffffff; color: #333;
                border: 1px solid #dadce0; border-radius: 3px; font-size: 13px; outline: none;
            }
            QListWidget::item:selected, QTableWidget::item:selected, QTreeWidget::item:selected {
                background-color: #e8f0fe; color: #1a73e8;
            }
            QHeaderView::section {
                background-color: #f8f9fa; color: #666;
                border: none; border-bottom: 1px solid #e0e0e0; padding: 8px; font-size: 12px; font-weight: normal;
            }
            QComboBox {
                background-color: #ffffff; color: #333;
                border: 1px solid #dadce0; border-radius: 3px; padding: 4px 10px; font-size: 13px;
            }
            QComboBox:hover { border-color: #1a73e8; }
            QComboBox QAbstractItemView {
                background-color: #ffffff; color: #333;
                border: 1px solid #dadce0; selection-background-color: #e8f0fe; selection-color: #1a73e8; outline: none;
            }
            QScrollBar:vertical { background-color: #f1f3f4; width: 10px; border: none; }
            QScrollBar::handle:vertical { background-color: #dadce0; border-radius: 5px; min-height: 30px; margin: 2px; }
            QScrollBar::handle:vertical:hover { background-color: #bdc1c6; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QScrollBar:horizontal { background-color: #f1f3f4; height: 10px; border: none; }
            QScrollBar::handle:horizontal { background-color: #dadce0; border-radius: 5px; min-width: 30px; margin: 2px; }
            QScrollBar::handle:horizontal:hover { background-color: #bdc1c6; }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
            QSplitter::handle { background-color: #e0e0e0; }
            QLabel { background-color: transparent; color: #333; }
            QToolTip { background-color: #ffffff; color: #333; border: 1px solid #dadce0; padding: 4px 8px; font-size: 12px; }
            QDialog, QInputDialog, QMessageBox, QFileDialog { background-color: #f0f2f5; }
            QToolTip { background-color: #ffffff; color: #333; border: 1px solid #dadce0; padding: 4px 8px; font-size: 12px; }
            QDialog, QInputDialog, QMessageBox, QFileDialog { background-color: #f0f2f5; }
            QGroupBox { border: 1px solid #dadce0; border-radius: 4px; margin-top: 12px; padding-top: 8px; color: #666; font-size: 13px; }
            QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
            QCheckBox, QRadioButton { color: #333; spacing: 8px; font-size: 13px; }
            QProgressBar { background-color: #f1f3f4; border: 1px solid #dadce0; border-radius: 3px; text-align: center; color: #333; font-size: 12px; }
            QProgressBar::chunk { background-color: #1a73e8; border-radius: 2px; }
            QSlider::groove:horizontal { height: 4px; background-color: #dadce0; border-radius: 2px; }
            QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; background-color: #1a73e8; border-radius: 6px; }
            QSlider::handle:horizontal:hover { background-color: #1557b0; }
            QSlider::sub-page:horizontal { background-color: #1a73e8; border-radius: 2px; }
        )");
    } else {
        setStyleSheet(R"(
            QMainWindow { background-color: #1b2838; }
            QWidget { background-color: #1b2838; color: #c7d5e0; }
            QMenuBar { background-color: transparent; color: #c7d5e0; border: none; padding: 2px; font-size: 13px; }
            QMenuBar::item { background: transparent; padding: 6px 14px; border-radius: 3px; }
            QMenuBar::item:selected { background-color: #2a475e; color: #ffffff; }
            QMenu { background-color: #16202d; color: #c7d5e0; border: 1px solid #2a475e; padding: 4px; font-size: 13px; }
            QMenu::item { padding: 6px 28px; border-radius: 3px; }
            QMenu::item:selected { background-color: #2a475e; color: #ffffff; }
            QMenu::separator { height: 1px; background-color: #2a475e; margin: 4px 8px; }
            QToolBar { background-color: #16202d; border: none; border-bottom: 1px solid #2a475e; padding: 4px 8px; spacing: 4px; }
            QToolBar QToolButton { background-color: transparent; color: #8f98a0; border: none; padding: 6px 12px; border-radius: 3px; font-size: 13px; }
            QToolBar QToolButton:hover { background-color: #2a475e; color: #66c0f4; }
            QToolBar QToolButton:pressed { background-color: #1a9fff; color: #ffffff; }
            QToolBar::separator { width: 1px; background-color: #2a475e; margin: 4px 6px; }
            QStatusBar { background-color: #16202d; color: #8f98a0; border-top: 1px solid #2a475e; font-size: 12px; }
            QStatusBar QLabel { color: #8f98a0; padding: 0 10px; background-color: transparent; }
            QPushButton { background-color: #2a475e; color: #c7d5e0; border: none; padding: 6px 16px; border-radius: 3px; font-size: 13px; }
            QPushButton:hover { background-color: #3d6b8a; color: #ffffff; }
            QPushButton:pressed { background-color: #1a9fff; color: #ffffff; }
            QLineEdit, QTextEdit, QPlainTextEdit, QTextBrowser {
                background-color: #16202d; color: #c7d5e0;
                border: 1px solid #2a475e; border-radius: 3px; padding: 5px 8px; font-size: 13px;
                selection-background-color: #1a9fff; selection-color: #ffffff;
            }
            QListWidget, QTableWidget, QTreeWidget {
                background-color: #16202d; color: #c7d5e0;
                border: 1px solid #2a475e; border-radius: 3px; font-size: 13px; outline: none;
            }
            QListWidget::item:selected, QTableWidget::item:selected, QTreeWidget::item:selected {
                background-color: #2a475e; color: #66c0f4;
            }
            QHeaderView::section {
                background-color: #1b2838; color: #8f98a0;
                border: none; border-bottom: 1px solid #2a475e; padding: 8px; font-size: 12px; font-weight: normal;
            }
            QComboBox {
                background-color: #16202d; color: #c7d5e0;
                border: 1px solid #2a475e; border-radius: 3px; padding: 4px 10px; font-size: 13px;
            }
            QComboBox:hover { border-color: #66c0f4; }
            QComboBox QAbstractItemView {
                background-color: #16202d; color: #c7d5e0;
                border: 1px solid #2a475e; selection-background-color: #2a475e; selection-color: #66c0f4; outline: none;
            }
            QScrollBar:vertical { background-color: #16202d; width: 10px; border: none; }
            QScrollBar::handle:vertical { background-color: #2a475e; border-radius: 5px; min-height: 30px; margin: 2px; }
            QScrollBar::handle:vertical:hover { background-color: #3d6b8a; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QScrollBar:horizontal { background-color: #16202d; height: 10px; border: none; }
            QScrollBar::handle:horizontal { background-color: #2a475e; border-radius: 5px; min-width: 30px; margin: 2px; }
            QScrollBar::handle:horizontal:hover { background-color: #3d6b8a; }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
            QSplitter::handle { background-color: #2a475e; }
            QLabel { background-color: transparent; color: #c7d5e0; }
            QToolTip { background-color: #16202d; color: #c7d5e0; border: 1px solid #2a475e; padding: 4px 8px; font-size: 12px; }
            QDialog, QInputDialog, QMessageBox, QFileDialog { background-color: #1b2838; }
            QGroupBox { border: 1px solid #2a475e; border-radius: 4px; margin-top: 12px; padding-top: 8px; color: #8f98a0; font-size: 13px; }
            QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }
            QCheckBox, QRadioButton { color: #c7d5e0; spacing: 8px; font-size: 13px; }
            QProgressBar { background-color: #16202d; border: 1px solid #2a475e; border-radius: 3px; text-align: center; color: #c7d5e0; font-size: 12px; }
            QProgressBar::chunk { background-color: #66c0f4; border-radius: 2px; }
            QSlider::groove:horizontal { height: 4px; background-color: #2a475e; border-radius: 2px; }
            QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; background-color: #66c0f4; border-radius: 6px; }
            QSlider::handle:horizontal:hover { background-color: #1a9fff; }
            QSlider::sub-page:horizontal { background-color: #66c0f4; border-radius: 2px; }
        )");
    }

    // Apply custom background image if set
    if (!m_bgImagePath.isEmpty() && QFile::exists(m_bgImagePath)) {
        setStyleSheet(styleSheet() + QString(R"(
            QMainWindow { background-image: url(%1); background-repeat: no-repeat; background-position: center; background-attachment: fixed; }
        )").arg(QDir::toNativeSeparators(m_bgImagePath).replace("\\", "/")));
    }

    // Fade out transition overlay after theme applied
    if (overlay) {
        QTimer::singleShot(50, this, [overlay]() {
            if (!overlay) return;
            QPropertyAnimation *fadeOut = new QPropertyAnimation(overlay, "windowOpacity");
            fadeOut->setDuration(200);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            fadeOut->setEasingCurve(QEasingCurve::OutCubic);
            connect(fadeOut, &QPropertyAnimation::finished, overlay, &QWidget::deleteLater);
            connect(fadeOut, &QPropertyAnimation::finished, fadeOut, &QPropertyAnimation::deleteLater);
            fadeOut->start();
        });
    }
}

void MainWindow::applyFontSize(int size)
{
    m_fontSize = size;
    QFont f = font();
    f.setPointSize(size);
    QApplication::setFont(f);
}

void MainWindow::loadSettings()
{
    QSettings settings("PureRefCanvas", "里与由");
    m_currentTheme = settings.value("theme", "dark").toString();
    m_fontSize = settings.value("fontSize", 13).toInt();
    m_closeBehavior = settings.value("closeBehavior", "tray").toString();
    m_startupRestore = settings.value("startupRestore", true).toBool();
    m_autoSave = settings.value("autoSave", true).toBool();
    m_autoSaveInterval = settings.value("autoSaveInterval", 5).toInt();
    m_thumbnailSize = settings.value("thumbnailSize", "medium").toString();
    m_lastPage = settings.value("lastPage", 0).toInt();
    m_uiScale = settings.value("uiScale", 100).toInt();
    m_language = settings.value("language", "zh").toString();
    m_hardwareAcceleration = settings.value("hardwareAcceleration", true).toBool();
    m_singleInstance = settings.value("singleInstance", false).toBool();
    m_acrylicEnabled = settings.value("acrylicEnabled", false).toBool();
    m_acrylicOpacity = settings.value("acrylicOpacity", 73).toInt();
    m_menuBarTransparent = settings.value("menuBarTransparent", true).toBool();
    m_menuBarAutoHide = settings.value("menuBarAutoHide", true).toBool();
    m_navToolbarTransparent = settings.value("navToolbarTransparent", true).toBool();
    m_libraryStatusBarTransparent = settings.value("libraryStatusBarTransparent", true).toBool();
    m_libraryGridTransparent = settings.value("libraryGridTransparent", true).toBool();
    m_contentBrowserTransparent = settings.value("contentBrowserTransparent", true).toBool();
    m_musicPanelsTransparent = settings.value("musicPanelsTransparent", true).toBool();
    m_notePanelsTransparent = settings.value("notePanelsTransparent", true).toBool();
    m_readerPanelsTransparent = settings.value("readerPanelsTransparent", true).toBool();
    m_materialDir = settings.value("materialDir", "").toString();
    m_booksDir = settings.value("booksDir", "").toString();
    m_musicDir = settings.value("musicDir", "").toString();
    m_noteBgColor = settings.value("noteBgColor", QColor(255,255,255)).value<QColor>();
    m_readerBgColor = settings.value("readerBgColor", QColor(255,250,240)).value<QColor>();
    m_readerTextColor = settings.value("readerTextColor", QColor(60,50,40)).value<QColor>();
    m_bgImagePath = settings.value("bgImagePath", "").toString();
    m_readerShelfBgPath = settings.value("readerShelfBgPath", "").toString();
    m_readerBgPath = settings.value("readerBgPath", "").toString();
    m_canvasBgPath = settings.value("canvasBgPath", "").toString();
    m_minimapOpacity = settings.value("minimapOpacity", 70).toInt();
    m_zoomBarOpacity = settings.value("zoomBarOpacity", 85).toInt();
    m_minimapVisible = settings.value("minimapVisible", true).toBool();
    m_minimapSize = settings.value("minimapSize", "medium").toString();
    m_minimapThrottle = settings.value("minimapThrottle", true).toBool();
    m_noteBgPath = settings.value("noteBgPath", "").toString();
    m_libraryBgPath = settings.value("libraryBgPath", "").toString();
    m_musicBgPath = settings.value("musicBgPath", "").toString();
    m_noteDefaultFont = settings.value("noteDefaultFont", "Microsoft YaHei").toString();
    m_noteDefaultFontSize = settings.value("noteDefaultFontSize", 14).toInt();

    // Music shortcuts
    m_scPlayPause = settings.value("shortcut/playPause", "Ctrl+P").toString();
    m_scPrev = settings.value("shortcut/prev", "Ctrl+Left").toString();
    m_scNext = settings.value("shortcut/next", "Ctrl+Right").toString();
    m_scVolUp = settings.value("shortcut/volUp", "Ctrl+Up").toString();
    m_scVolDown = settings.value("shortcut/volDown", "Ctrl+Down").toString();

    // Timer shutdown
    m_timerShutdownEnabled = settings.value("timerShutdown/enabled", false).toBool();
    m_timerShutdownHours = settings.value("timerShutdown/hours", 0).toInt();
    m_timerShutdownMinutes = settings.value("timerShutdown/minutes", 0).toInt();
    m_shutdownPCEnabled = settings.value("timerShutdown/shutdownPC", false).toBool();

    // New settings
    m_startupPage = settings.value("startupPage", "last").toString();
    m_defaultSort = settings.value("library/defaultSort", "name").toString();
    m_showFileExt = settings.value("library/showFileExt", true).toBool();
    m_doubleClickBehavior = settings.value("library/doubleClickBehavior", "preview").toString();
    m_readerTheme = settings.value("reader/theme", "light").toString();
    m_readerFont = settings.value("reader/font", "系统默认").toString();
    m_readerLineSpacing = settings.value("reader/lineSpacing", 1.5).toDouble();
    m_readerFontSize = settings.value("reader/fontSize", 16).toInt();
    m_readerMargin = settings.value("reader/margin", "medium").toString();
    m_defaultVolume = settings.value("music/defaultVolume", 70).toInt();
    m_defaultPlayMode = settings.value("music/defaultPlayMode", "sequential").toString();
    m_showLyrics = settings.value("music/showLyrics", false).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings("PureRefCanvas", "里与由");
    settings.setValue("theme", m_currentTheme);
    settings.setValue("fontSize", m_fontSize);
    settings.setValue("closeBehavior", m_closeBehavior);
    settings.setValue("startupRestore", m_startupRestore);
    settings.setValue("autoSave", m_autoSave);
    settings.setValue("autoSaveInterval", m_autoSaveInterval);
    settings.setValue("thumbnailSize", m_thumbnailSize);
    settings.setValue("lastPage", m_stack ? m_stack->currentIndex() : 0);
    settings.setValue("uiScale", m_uiScale);
    settings.setValue("language", m_language);
    settings.setValue("hardwareAcceleration", m_hardwareAcceleration);
    settings.setValue("singleInstance", m_singleInstance);
    settings.setValue("acrylicEnabled", m_acrylicEnabled);
    settings.setValue("acrylicOpacity", m_acrylicOpacity);
    settings.setValue("menuBarTransparent", m_menuBarTransparent);
    settings.setValue("menuBarAutoHide", m_menuBarAutoHide);
    settings.setValue("navToolbarTransparent", m_navToolbarTransparent);
    settings.setValue("libraryStatusBarTransparent", m_libraryStatusBarTransparent);
    settings.setValue("libraryGridTransparent", m_libraryGridTransparent);
    settings.setValue("contentBrowserTransparent", m_contentBrowserTransparent);
    settings.setValue("musicPanelsTransparent", m_musicPanelsTransparent);
    settings.setValue("notePanelsTransparent", m_notePanelsTransparent);
    settings.setValue("readerPanelsTransparent", m_readerPanelsTransparent);
    settings.setValue("materialDir", m_materialDir);
    settings.setValue("booksDir", m_booksDir);
    settings.setValue("musicDir", m_musicDir);
    settings.setValue("noteBgColor", m_noteBgColor);
    settings.setValue("readerBgColor", m_readerBgColor);
    settings.setValue("readerTextColor", m_readerTextColor);
    settings.setValue("bgImagePath", m_bgImagePath);
    settings.setValue("readerShelfBgPath", m_readerShelfBgPath);
    settings.setValue("readerBgPath", m_readerBgPath);
    settings.setValue("canvasBgPath", m_canvasBgPath);
    settings.setValue("minimapOpacity", m_minimapOpacity);
    settings.setValue("zoomBarOpacity", m_zoomBarOpacity);
    settings.setValue("minimapVisible", m_minimapVisible);
    settings.setValue("minimapSize", m_minimapSize);
    settings.setValue("minimapThrottle", m_minimapThrottle);
    settings.setValue("noteBgPath", m_noteBgPath);
    settings.setValue("libraryBgPath", m_libraryBgPath);
    settings.setValue("musicBgPath", m_musicBgPath);
    settings.setValue("noteDefaultFont", m_noteDefaultFont);
    settings.setValue("noteDefaultFontSize", m_noteDefaultFontSize);

    // Music shortcuts
    settings.setValue("shortcut/playPause", m_scPlayPause);
    settings.setValue("shortcut/prev", m_scPrev);
    settings.setValue("shortcut/next", m_scNext);
    settings.setValue("shortcut/volUp", m_scVolUp);
    settings.setValue("shortcut/volDown", m_scVolDown);

    // Timer shutdown
    settings.setValue("timerShutdown/enabled", m_timerShutdownEnabled);
    settings.setValue("timerShutdown/hours", m_timerShutdownHours);
    settings.setValue("timerShutdown/minutes", m_timerShutdownMinutes);
    settings.setValue("timerShutdown/shutdownPC", m_shutdownPCEnabled);

    // New settings
    settings.setValue("startupPage", m_startupPage);
    settings.setValue("library/defaultSort", m_defaultSort);
    settings.setValue("library/showFileExt", m_showFileExt);
    settings.setValue("library/doubleClickBehavior", m_doubleClickBehavior);
    settings.setValue("reader/theme", m_readerTheme);
    settings.setValue("reader/font", m_readerFont);
    settings.setValue("reader/lineSpacing", m_readerLineSpacing);
    settings.setValue("reader/fontSize", m_readerFontSize);
    settings.setValue("reader/margin", m_readerMargin);
    settings.setValue("music/defaultVolume", m_defaultVolume);
    settings.setValue("music/defaultPlayMode", m_defaultPlayMode);
    settings.setValue("music/showLyrics", m_showLyrics);
}

void MainWindow::applyThumbnailSize(const QString &size)
{
    if (!m_materialWidget) return;
    m_materialWidget->setThumbnailSize(size);
}

void MainWindow::applyNoteBgColor(const QColor &c)
{
    if (!m_noteWidget) return;
    m_noteWidget->setBgColor(c);
}

void MainWindow::applyReaderColors(const QColor &bg, const QColor &text)
{
    if (!m_readerWidget) return;
    m_readerWidget->setCustomColors(bg, text);
}

void MainWindow::applyBgImage(const QString &path)
{
    m_bgImagePath = path;

    bool hasImage = !path.isEmpty() && QFile::exists(path);

    if (hasImage) {
        // Make stack and all pages transparent so wallpaper shows through
        m_stack->setStyleSheet("background: transparent; border: none;");
        for (int i = 0; i < m_stack->count(); ++i) {
            QWidget *page = m_stack->widget(i);
            if (page) {
                page->setStyleSheet("background: transparent;");
                page->setAttribute(Qt::WA_TranslucentBackground, false);
            }
        }
        // Canvas: light overlay so grid + wallpaper both visible, not too dark
        if (m_canvasView) {
            m_canvasView->setBackgroundBrush(QColor(30, 30, 32, 60));
            m_canvasView->setStyleSheet("background: transparent; border: none;");
        }
    } else {
        // Restore default solid backgrounds
        m_stack->setStyleSheet("background: #1b2838; border: none;");
        for (int i = 0; i < m_stack->count(); ++i) {
            QWidget *page = m_stack->widget(i);
            if (page) page->setStyleSheet("");
        }
        if (m_canvasView) {
            m_canvasView->setBackgroundBrush(QColor(30, 30, 32));
            m_canvasView->setStyleSheet("");
        }
    }

    applyTheme(m_currentTheme);
}

void MainWindow::applyAcrylic()
{
#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    if (!hwnd) return;
    enableRoundedCorners(hwnd);
    // Convert opacity percentage (30-100) to alpha byte (0-255)
    BYTE alpha = (BYTE)(m_acrylicOpacity * 255 / 100);
    // Color format: 0xAABBGGRR, default dark blue #1B2838
    DWORD gradientColor = ((DWORD)alpha << 24) | 0x0038281B;
    enableAcrylicBlur(hwnd, m_acrylicEnabled, gradientColor);
#endif
}

void MainWindow::applyTransparencySettings()
{
    // Menu bar (respect current theme colors)
    const bool light = (m_currentTheme == "light");
    const QString menuBg = m_menuBarTransparent ? "transparent" : (light ? "#ffffff" : "#16202d");
    const QString menuColor = light ? "#333333" : "#c7d5e0";
    const QString menuBorder = light ? "border-bottom: 1px solid #e0e0e0;" : "border: none;";
    menuBar()->setStyleSheet(QString(
        "QMenuBar { background-color: %1; color: %2; %3 padding: 2px; font-size: 13px; }")
        .arg(menuBg).arg(menuColor).arg(menuBorder));

    // Navigation toolbar
    if (m_navToolBar) {
        if (m_navToolbarTransparent) {
            m_navToolBar->setStyleSheet(
                "QToolBar { background: transparent; border: none; padding: 0px; spacing: 0px; }"
                " QToolBar::separator { width: 0; }");
        } else {
            m_navToolBar->setStyleSheet(
                "QToolBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                " stop:0 rgba(255,255,255,0.08), stop:0.5 rgba(255,255,255,0.02), stop:1 #141e2b);"
                " border-bottom: 1px solid #2a475e; padding: 0px; spacing: 0px; }"
                " QToolBar::separator { width: 0; }");
        }
    }

    // Library status bar (N 项)
    if (m_materialWidget)
        m_materialWidget->setStatusBarTransparent(m_libraryStatusBarTransparent);

    // Library asset grid
    if (m_materialWidget)
        m_materialWidget->setGridTransparent(m_libraryGridTransparent);

    // Global content browser panel
    if (m_contentBrowser)
        m_contentBrowser->setTransparent(m_contentBrowserTransparent);

    // Music player panels
    if (m_musicWidget)
        m_musicWidget->setPanelsTransparent(m_musicPanelsTransparent);

    // Note page panels
    if (m_noteWidget)
        m_noteWidget->setPanelsTransparent(m_notePanelsTransparent);

    // Reader page panels
    if (m_readerWidget)
        m_readerWidget->setPanelsTransparent(m_readerPanelsTransparent);
}

void MainWindow::applyMenuBarMode()
{
    // 半隐藏：隐藏原生菜单栏，用导航栏上的 ☰ 按钮承载菜单；否则显示原生菜单栏。
    if (m_menuBarAutoHide) {
        if (menuBar()) menuBar()->hide();
        if (m_menuHamburger) m_menuHamburger->show();
    } else {
        if (menuBar()) menuBar()->show();
        if (m_menuHamburger) m_menuHamburger->hide();
    }
}

void MainWindow::applyEditorSettings(const QString &font, int fontSize)
{
    m_noteDefaultFont = font;
    m_noteDefaultFontSize = fontSize;
    if (!m_noteWidget) return;
    QTextEdit *editor = m_noteWidget->editor();
    if (!editor) return;
    QFont f = editor->font();
    f.setFamily(font);
    f.setPointSize(fontSize);
    editor->setFont(f);
    // Also set default font for new text
    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFont(f);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void MainWindow::applyMusicShortcuts()
{
    // Clear existing shortcuts
    for (QShortcut *sc : m_musicShortcuts) delete sc;
    m_musicShortcuts.clear();

    if (!m_musicWidget) return;

    auto addSc = [this](const QString &key, auto slot) {
        if (key.isEmpty()) return;
        QShortcut *sc = new QShortcut(QKeySequence(key), this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, m_musicWidget, slot);
        m_musicShortcuts.append(sc);
    };

    addSc(m_scPlayPause, &MusicPlayerWidget::togglePlay);
    addSc(m_scPrev, &MusicPlayerWidget::playPrev);
    addSc(m_scNext, &MusicPlayerWidget::playNext);
    addSc(m_scVolUp, &MusicPlayerWidget::volumeUp);
    addSc(m_scVolDown, &MusicPlayerWidget::volumeDown);
}

void MainWindow::openGlobalSearch()
{
    GlobalSearchDialog *dlg = new GlobalSearchDialog(this, this);
    // Center on screen
    QPoint center = mapToGlobal(rect().center());
    dlg->move(center.x() - dlg->width() / 2, center.y() - dlg->height() / 2);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::openCommandPalette()
{
    CommandPaletteDialog *dlg = new CommandPaletteDialog(this, this);
    QPoint center = mapToGlobal(rect().center());
    dlg->move(center.x() - dlg->width() / 2, center.y() - dlg->height() / 2 - 50);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::openShortcutPanel()
{
    ShortcutPanelDialog *dlg = new ShortcutPanelDialog(this);
    QPoint center = mapToGlobal(rect().center());
    dlg->move(center.x() - dlg->width() / 2, center.y() - dlg->height() / 2);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

#include "mainwindow.moc"

void MainWindow::applyTimerShutdown()
{
    if (m_shutdownTimer) {
        m_shutdownTimer->stop();
        delete m_shutdownTimer;
        m_shutdownTimer = nullptr;
    }

    if (!m_timerShutdownEnabled) return;

    qint64 totalMs = (m_timerShutdownHours * 3600LL + m_timerShutdownMinutes * 60LL) * 1000LL;
    if (totalMs <= 0) return;

    m_shutdownRemainingMs = totalMs;
    m_shutdownTimer = new QTimer(this);
    m_shutdownTimer->setInterval(1000);
    connect(m_shutdownTimer, &QTimer::timeout, this, &MainWindow::onShutdownTick);
    m_shutdownTimer->start();
}

void MainWindow::onShutdownTick()
{
    m_shutdownRemainingMs -= 1000;
    if (m_shutdownRemainingMs <= 0) {
        m_shutdownTimer->stop();
        saveSettings();
        if (m_shutdownPCEnabled) {
            QProcess::startDetached("shutdown", {"/s", "/t", "0"});
        }
        qApp->quit();
    }
}

void MainWindow::refreshCanvasItemList()
{
    if (!m_canvasItemList) return;
    m_canvasItemList->clear();
    CanvasScene *s = canvasScene();
    if (!s) return;
    int idx = 1;
    for (QGraphicsItem *item : s->items()) {
        ImageItem *img = dynamic_cast<ImageItem*>(item);
        if (img) {
            QString name = QString("图片 %1").arg(idx++);
            if (img->isLocked()) name += " 🔒";
            QListWidgetItem *li = new QListWidgetItem(name);
            li->setData(Qt::UserRole, QVariant::fromValue((void*)item));
            m_canvasItemList->addItem(li);
        }
    }
}

void MainWindow::setCurrentUser(const UserInfo &user)
{
    m_currentUser = user;
    QString displayName = user.nickname.isEmpty() ? user.username : user.nickname;
    if (m_userNickname) {
        m_userNickname->setText(displayName);
    }
    if (m_userAvatar && !user.username.isEmpty()) {
        if (!user.avatarPath.isEmpty() && QFile::exists(user.avatarPath)) {
            // Custom avatar - crop to circle
            QPixmap src(user.avatarPath);
            if (!src.isNull()) {
                QPixmap pix(28, 28);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                p.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addEllipse(0, 0, 28, 28);
                p.setClipPath(path);
                p.drawPixmap(0, 0, 28, 28, src.scaled(28, 28, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                m_userAvatar->setPixmap(pix);
            }
        } else {
            // Default gradient avatar
            QPixmap pix(28, 28);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            QLinearGradient grad(0, 0, 28, 28);
            int hue = displayName.at(0).unicode() % 360;
            grad.setColorAt(0, QColor::fromHsl(hue, 120, 160));
            grad.setColorAt(1, QColor::fromHsl((hue + 40) % 360, 120, 140));
            p.setBrush(grad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(0, 0, 28, 28);
            p.setPen(QColor(255, 255, 255, 230));
            QFont font;
            font.setPointSize(11);
            font.setBold(true);
            p.setFont(font);
            p.drawText(pix.rect(), Qt::AlignCenter, QString(displayName.at(0).toUpper()));
            m_userAvatar->setPixmap(pix);
        }
    }
    updateWindowTitle();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_userWidget && event->type() == QEvent::MouseButtonPress) {
        if (m_userMenu) {
            QPoint pos = m_userWidget->mapToGlobal(QPoint(0, m_userWidget->height() + 2));
            m_userMenu->exec(pos);
        }
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setUserAuth(UserAuth *auth)
{
    m_userAuth = auth;
}

void MainWindow::showAccountDialog()
{
    if (!m_userAuth) return;
    AccountDialog dlg(m_userAuth, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentUser = m_userAuth->currentUser();
        setCurrentUser(m_currentUser);
    } else if (!m_userAuth->isLoggedIn()) {
        // User logged out — emit signal so main.cpp can show login dialog again
        emit loggedOut();
    }
}

void MainWindow::logoutUser()
{
    if (m_userAuth) m_userAuth->logout();
    close();
}

void MainWindow::setupAutoSaveTimer()
{
    if (!m_autoSaveTimer) {
        m_autoSaveTimer = new QTimer(this);
        connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::doAutoSave);
    }
    if (m_autoSave && m_autoSaveInterval > 0) {
        m_autoSaveTimer->start(m_autoSaveInterval * 60 * 1000);
    } else {
        m_autoSaveTimer->stop();
    }
}

void MainWindow::doAutoSave()
{
    // Auto save canvas
    if (m_canvasView && m_canvasView->scene()) {
        CanvasScene *scene = qobject_cast<CanvasScene*>(m_canvasView->scene());
        // Skip untouched canvases entirely: serializing means re-encoding every
        // image, which measured 1.25 s (5 x 12 MP photos) to 12 s (50 photos).
        if (scene && m_canvasDirty) {
            QString path = UserAuth::dataRoot() + "/autosave.prc";
            QDir().mkpath(QFileInfo(path).absolutePath());
            if (writeAtomically(path, scene->saveState()))
                m_canvasDirty = false;
        }
    }
    // Auto save note
    if (m_noteWidget && m_noteWidget->isModified()) {
        QString path = UserAuth::dataRoot() + "/autosave_note.html";
        QDir().mkpath(QFileInfo(path).absolutePath());
        writeAtomically(path, m_noteWidget->toHtml().toUtf8());
    }
    statusBar()->showMessage("已自动保存", 2000);
}
