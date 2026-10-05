#include "themeapplier.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>
#include <QWidget>
#include <QMenuBar>
#include <QApplication>
#include <QFont>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTextEdit>
#include <QTextCursor>
#include <QTextCharFormat>

#include "canvasview.h"
#include "materialwidget.h"
#include "notewidget.h"
#include "readerwidget.h"
#include "musicplayer.h"
#include "contentbrowserpanel.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

namespace {
// --- Acrylic blur + rounded corners (Win10/11 DWM interop) -----------------
// Copied verbatim from mainwindow.cpp. These are undocumented user32 APIs
// resolved at runtime so the app still launches on Windows 7 / Server SKUs
// that don't export them.

struct AccentPolicy {
    DWORD AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};
struct WindowCompositionAttributeData {
    DWORD Attribute;
    LPVOID Data;
    DWORD SizeOfData;
};
const DWORD ACCENT_DISABLED = 0;
const DWORD ACCENT_ENABLE_ACRYLICBLURBEHIND = 4;

typedef BOOL (WINAPI *PFN_SetWindowCompositionAttribute)(HWND, WindowCompositionAttributeData*);

void enableAcrylicBlur(HWND hwnd, bool enable, DWORD gradientColor = 0xAA1B2838)
{
    HMODULE hUser = GetModuleHandle(L"user32.dll");
    if (!hUser) return;
    auto SetWindowCompositionAttribute =
        reinterpret_cast<PFN_SetWindowCompositionAttribute>(
            GetProcAddress(hUser, "SetWindowCompositionAttribute"));
    if (!SetWindowCompositionAttribute) return;

    AccentPolicy accent = {enable ? ACCENT_ENABLE_ACRYLICBLURBEHIND : ACCENT_DISABLED, 0, gradientColor, 0};
    WindowCompositionAttributeData data = {19, &accent, sizeof(accent)};
    SetWindowCompositionAttribute(hwnd, &data);
}

// Windows 11: rounded window corners via DWM (silent no-op on Windows 10)
void enableRoundedCorners(HWND hwnd)
{
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    auto DwmSetWindowAttribute =
        reinterpret_cast<PFN_DwmSetWindowAttribute>(
            GetProcAddress(hDwm, "DwmSetWindowAttribute"));
    if (!DwmSetWindowAttribute) return;
    const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    const DWORD DWMWCP_ROUND = 2;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));
}
}
#endif

ThemeApplier::ThemeApplier(AppSettings &cfg, Widgets w)
    : m_cfg(cfg), m_w(w)
{}

void ThemeApplier::applyAll()
{
    // Same order MainWindow used at startup.
    applyTheme(m_cfg.currentTheme);
    applyFontSize(m_cfg.fontSize);
    applyThumbnailSize(m_cfg.thumbnailSize);
    applyNoteBgColor(m_cfg.noteBgColor);
    applyReaderColors(m_cfg.readerBgColor, m_cfg.readerTextColor);
    applyBgImage(m_cfg.bgImagePath);
    applyEditorSettings(m_cfg.noteDefaultFont, m_cfg.noteDefaultFontSize);
    applyTransparencySettings();
    applyMenuBarMode();
    applyAcrylic();
}

void ThemeApplier::applyTheme(const QString &theme)
{
    QMainWindow *w = m_w.window;
    if (!w) return;

    // Theme transition overlay (safe method: fade overlay instead of screenshot)
    QWidget *overlay = nullptr;
    if (w->centralWidget() && w->isVisible() && m_cfg.currentTheme != theme) {
        overlay = new QWidget(w->centralWidget());
        overlay->setGeometry(w->centralWidget()->rect());
        overlay->setStyleSheet(m_cfg.currentTheme == "dark"
            ? "background: rgba(22,32,45,0.9);"
            : "background: rgba(240,242,245,0.9);");
        overlay->setWindowOpacity(0.0);
        overlay->show();
        overlay->raise();
        QPropertyAnimation *fadeIn = new QPropertyAnimation(overlay, "windowOpacity", w);
        fadeIn->setDuration(120);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->start();
    }

    m_cfg.currentTheme = theme;
    if (theme == "light") {
        w->setStyleSheet(R"(
            QMainWindow { background-color: #f0f2f5; }
            QWidget { background-color: #f0f2f5; color: #1a1a1a; }
            QToolTip { background-color: #ffffff; color: #202124; border: 1px solid #dadce0; border-radius: 6px; padding: 8px 12px; font-size: 12px; }
            QMenuBar { background-color: #ffffff; color: #333; border: none; border-bottom: 1px solid #e0e0e0; padding: 2px; font-size: 13px; }
            QMenuBar::item { background: transparent; padding: 6px 14px; border-radius: 3px; }
            QMenuBar::item:selected { background-color: #e8f0fe; color: #1a73e8; }
            QMenu { background-color: #ffffff; color: #202124; border: 1px solid #dadce0; border-radius: 8px; padding: 6px; font-size: 13px; }
            QMenu::item { padding: 8px 28px 8px 16px; border-radius: 4px; }
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
        w->setStyleSheet(R"(
            QMainWindow { background-color: #1b2838; }
            QWidget { background-color: #1b2838; color: #c7d5e0; }
            QToolTip { background-color: #ffffff; color: #202124; border: 1px solid #dadce0; border-radius: 6px; padding: 8px 12px; font-size: 12px; }
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
    if (!m_cfg.bgImagePath.isEmpty() && QFile::exists(m_cfg.bgImagePath)) {
        w->setStyleSheet(w->styleSheet() + QString(R"(
            QMainWindow { background-image: url(%1); background-repeat: no-repeat; background-position: center; background-attachment: fixed; }
        )").arg(QDir::toNativeSeparators(m_cfg.bgImagePath).replace("\\", "/")));
    }

    // Fade out transition overlay after theme applied
    if (overlay) {
        QTimer::singleShot(50, w, [overlay]() {
            if (!overlay) return;
            QPropertyAnimation *fadeOut = new QPropertyAnimation(overlay, "windowOpacity");
            fadeOut->setDuration(200);
            fadeOut->setStartValue(1.0);
            fadeOut->setEndValue(0.0);
            fadeOut->setEasingCurve(QEasingCurve::OutCubic);
            QObject::connect(fadeOut, &QPropertyAnimation::finished, overlay, &QWidget::deleteLater);
            QObject::connect(fadeOut, &QPropertyAnimation::finished, fadeOut, &QPropertyAnimation::deleteLater);
            fadeOut->start();
        });
    }
}

void ThemeApplier::applyFontSize(int size)
{
    m_cfg.fontSize = size;
    QFont f = QApplication::font();
    f.setPointSize(size);
    QApplication::setFont(f);
}

void ThemeApplier::applyThumbnailSize(const QString &size)
{
    if (!m_w.materialWidget) return;
    m_w.materialWidget->setThumbnailSize(size);
}

void ThemeApplier::applyNoteBgColor(const QColor &c)
{
    if (!m_w.noteWidget) return;
    m_w.noteWidget->setBgColor(c);
}

void ThemeApplier::applyReaderColors(const QColor &bg, const QColor &text)
{
    if (!m_w.readerWidget) return;
    m_w.readerWidget->setCustomColors(bg, text);
}

void ThemeApplier::applyBgImage(const QString &path)
{
    m_cfg.bgImagePath = path;
    QStackedWidget *stack = m_w.stack;
    if (!stack) return;

    bool hasImage = !path.isEmpty() && QFile::exists(path);

    if (hasImage) {
        // Make stack and all pages transparent so wallpaper shows through
        stack->setStyleSheet("background: transparent; border: none;");
        for (int i = 0; i < stack->count(); ++i) {
            QWidget *page = stack->widget(i);
            if (page) {
                page->setStyleSheet("background: transparent;");
                page->setAttribute(Qt::WA_TranslucentBackground, false);
            }
        }
        // Canvas: light overlay so grid + wallpaper both visible, not too dark
        if (m_w.canvasView) {
            m_w.canvasView->setBackgroundBrush(QColor(30, 30, 32, 60));
            m_w.canvasView->setStyleSheet("background: transparent; border: none;");
        }
    } else {
        // Restore default solid backgrounds. Theme-aware: the stack sits
        // BEHIND pages whose own background is transparent or failed to load
        // (e.g. an undecodable reader-shelf image), so a hardcoded dark color
        // would leak through in light mode.
        stack->setStyleSheet(QString("background: %1; border: none;")
                                   .arg(m_cfg.currentTheme == "light" ? "#f0f2f5" : "#1b2838"));
        for (int i = 0; i < stack->count(); ++i) {
            QWidget *page = stack->widget(i);
            if (page) page->setStyleSheet("");
        }
        if (m_w.canvasView) {
            m_w.canvasView->setBackgroundBrush(QColor(30, 30, 32));
            m_w.canvasView->setStyleSheet("");
        }
    }

    applyTheme(m_cfg.currentTheme);
}

void ThemeApplier::applyAcrylic()
{
#ifdef Q_OS_WIN
    if (!m_w.window) return;
    HWND hwnd = reinterpret_cast<HWND>(m_w.window->winId());
    if (!hwnd) return;
    enableRoundedCorners(hwnd);
    BYTE alpha = static_cast<BYTE>(m_cfg.acrylicOpacity * 255 / 100);
    DWORD gradientColor = (static_cast<DWORD>(alpha) << 24) | 0x0038281B;
    enableAcrylicBlur(hwnd, m_cfg.acrylicEnabled, gradientColor);
#endif
}

void ThemeApplier::applyTransparencySettings()
{
    QMainWindow *w = m_w.window;
    if (!w) return;

    const bool light = (m_cfg.currentTheme == "light");
    const QString menuBg = m_cfg.menuBarTransparent ? "transparent" : (light ? "#ffffff" : "#16202d");
    const QString menuColor = light ? "#333333" : "#c7d5e0";
    const QString menuBorder = light ? "border-bottom: 1px solid #e0e0e0;" : "border: none;";
    if (w->menuBar()) {
        w->menuBar()->setStyleSheet(QString(
            "QMenuBar { background-color: %1; color: %2; %3 padding: 2px; font-size: 13px; }")
            .arg(menuBg, menuColor, menuBorder));
    }

    if (m_w.navToolBar) {
        if (m_cfg.navToolbarTransparent) {
            m_w.navToolBar->setStyleSheet(
                "QToolBar { background: transparent; border: none; padding: 0px; spacing: 0px; }"
                " QToolBar::separator { width: 0; }");
        } else {
            m_w.navToolBar->setStyleSheet(
                "QToolBar { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                " stop:0 rgba(255,255,255,0.08), stop:0.5 rgba(255,255,255,0.02), stop:1 #141e2b);"
                " border-bottom: 1px solid #2a475e; padding: 0px; spacing: 0px; }"
                " QToolBar::separator { width: 0; }");
        }
    }

    if (m_w.materialWidget) {
        m_w.materialWidget->setStatusBarTransparent(m_cfg.libraryStatusBarTransparent);
        m_w.materialWidget->setGridTransparent(m_cfg.libraryGridTransparent);
    }
    if (m_w.contentBrowser)   m_w.contentBrowser->setTransparent(m_cfg.contentBrowserTransparent);
    if (m_w.musicWidget)      m_w.musicWidget->setPanelsTransparent(m_cfg.musicPanelsTransparent);
    if (m_w.noteWidget)       m_w.noteWidget->setPanelsTransparent(m_cfg.notePanelsTransparent);
    if (m_w.readerWidget)     m_w.readerWidget->setPanelsTransparent(m_cfg.readerPanelsTransparent);
}

void ThemeApplier::applyMenuBarMode()
{
    QMainWindow *w = m_w.window;
    if (!w) return;
    if (m_cfg.menuBarAutoHide) {
        if (w->menuBar()) w->menuBar()->hide();
        if (m_w.menuHamburger) m_w.menuHamburger->show();
    } else {
        if (w->menuBar()) w->menuBar()->show();
        if (m_w.menuHamburger) m_w.menuHamburger->hide();
    }
}

void ThemeApplier::applyEditorSettings(const QString &font, int fontSize)
{
    m_cfg.noteDefaultFont = font;
    m_cfg.noteDefaultFontSize = fontSize;
    if (!m_w.noteWidget) return;
    QTextEdit *editor = m_w.noteWidget->editor();
    if (!editor) return;
    QFont f = editor->font();
    f.setFamily(font);
    f.setPointSize(fontSize);
    editor->setFont(f);
    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFont(f);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}
