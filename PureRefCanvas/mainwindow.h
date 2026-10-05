#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QStackedWidget;
class QPushButton;
class QToolBar;
class QToolButton;
class QSystemTrayIcon;
class QShortcut;
class CanvasScene;
class CanvasView;
class NoteWidget;
class MaterialWidget;
class ReaderWidget;
class MusicPlayerWidget;
class QLabel;
class QListWidget;
class MaterialLibrary;
class ContentBrowserPanel;
class UserAuth;
#include "userauth.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class GlobalSearchDialog;
    friend class CommandPaletteDialog;
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void setCurrentUser(const UserInfo &user);
    void setUserAuth(UserAuth *auth);

    enum PageType { CanvasPage, NotePage, MaterialPage, BookPage, MusicPage };

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void switchToPage(int index);
    void toggleSidebar();  // 折叠/展开当前页的左侧栏（Ctrl+8，ZCode 风格）

    // File operations
    void openImages();
    void saveCurrent();
    void saveAsCurrent();
    void loadFile();
    void exportImage();
    void about();
    void openSettings();
    void openCommandPalette();
    void openShortcutPanel();

    // Edit operations (delegated to current page)
    void deleteSelected();
    void selectAll();
    void copySelected();
    void pasteContent();
    void duplicateSelected();
    void bringToFront();
    void sendToBack();
    void toggleLock();

    // View
    void zoomIn();
    void zoomOut();
    void zoomActual();
    void zoomToFit();

    // Status updates
    void updateZoomLabel(qreal zoom);
    void updateItemCount(int count);
    void updateSelectionCount(int count);
    void handleFileDrop(const QStringList &paths);
    void onNoteModified(bool modified);

signals:
    void loggedOut();

private:
    void createActions();
    void createMenus();
    void createStatusBar();
    void createTrayIcon();
    void setupNavigation();
    void setupPages();

    // Helpers for current page
    PageType currentPage() const;
    CanvasView *canvasView() const;
    NoteWidget *noteWidget() const;
    CanvasScene *canvasScene() const;
    void updateWindowTitle();
    void updateNavButtons();
    void updateStatusForPage();

    bool maybeSave();
    void loadImages(const QStringList &paths);
    void applyTheme(const QString &theme);
    void applyFontSize(int size);
    void applyThumbnailSize(const QString &size);
    void applyNoteBgColor(const QColor &c);
    void applyReaderColors(const QColor &bg, const QColor &text);
    void applyBgImage(const QString &path);
    void applyAcrylic();
    void applyTransparencySettings();
    void applyMenuBarMode();   // 普通菜单栏 <-> ☰ 半隐藏按钮
    void applyAutostart(bool enabled);  // write/remove Windows Run registry key
    void applyEditorSettings(const QString &font, int fontSize);
    void refreshCanvasItemList();
    void showAccountDialog();
    void logoutUser();
    void setupAutoSaveTimer();
    void maybeGenerateWeeklyReport();
    void doAutoSave();
    void loadSettings();
    void saveSettings();
    void applyMusicShortcuts();
    void openGlobalSearch();

    QWidget *m_navBar;
    QStackedWidget *m_stack;
    QList<QPushButton*> m_navButtons;

    CanvasView *m_canvasView;
    NoteWidget *m_noteWidget;
    MaterialWidget *m_materialWidget;
    ReaderWidget *m_readerWidget;
    MusicPlayerWidget *m_musicWidget;

    QLabel *m_zoomLabel;
    QLabel *m_itemCountLabel;
    QLabel *m_selectionLabel;
    QLabel *m_pageTypeLabel;

    QString m_canvasFilePath;
    QString m_noteFilePath;
    // Set whenever the canvas changes and cleared after a successful save, so
    // auto-save can skip untouched canvases instead of re-serializing them.
    bool m_canvasDirty = false;
    QString m_currentTheme;
    int m_fontSize;

    // General settings
    QString m_closeBehavior = "tray"; // "tray" or "quit"
    bool m_startupRestore = true;
    bool m_autoSave = true;
    int m_autoSaveInterval = 5; // minutes
    QString m_thumbnailSize = "medium"; // small/medium/large
    int m_lastPage = 0;
    QString m_startupPage = "last"; // last/canvas/note/library/reader/music

    // Library settings
    QString m_defaultSort = "name"; // name/date/size/type
    bool m_showFileExt = true;
    QString m_doubleClickBehavior = "preview"; // preview/external/canvas

    // Reader settings
    QString m_readerTheme = "light"; // light/dark/eye/parchment
    QString m_readerFont = "系统默认";
    qreal m_readerLineSpacing = 1.5;
    int m_readerFontSize = 16;
    QString m_readerMargin = "medium"; // narrow/medium/wide

    // Music settings
    int m_defaultVolume = 70;
    QString m_defaultPlayMode = "sequential"; // sequential/loopOne/loopAll/shuffle
    bool m_showLyrics = false;

    // Advanced settings (require restart)
    int m_uiScale = 100;
    QString m_language = "zh";
    bool m_hardwareAcceleration = true;
    bool m_singleInstance = false;
    bool m_autostart = false;
    bool m_acrylicEnabled = true;
    int m_acrylicOpacity = 73;
    bool m_menuBarTransparent = true;
    bool m_menuBarAutoHide = true;   // 半隐藏：菜单栏折叠成 ☰ 按钮，点击才弹出
    bool m_navToolbarTransparent = true;
    bool m_libraryStatusBarTransparent = true;
    bool m_libraryGridTransparent = true;
    bool m_contentBrowserTransparent = true;
    bool m_musicPanelsTransparent = true;
    bool m_notePanelsTransparent = true;
    bool m_readerPanelsTransparent = true;
    QToolBar *m_navToolBar = nullptr;
    QToolButton *m_brandBtn = nullptr;  // 应用图标按钮 = 切换侧边栏
    QToolButton *m_menuHamburger = nullptr;  // 半隐藏模式下的 ☰ 菜单按钮
    int m_minimapOpacity = 70;
    int m_zoomBarOpacity = 85;
    bool m_minimapVisible = true;
    QString m_minimapSize = "medium";
    bool m_minimapThrottle = true;
    QString m_materialDir;
    QString m_booksDir;
    QString m_musicDir;
    QColor m_noteBgColor = QColor(255, 255, 255);
    QColor m_readerBgColor = QColor(255, 250, 240);
    QColor m_readerTextColor = QColor(60, 50, 40);
    QString m_bgImagePath;
    QString m_readerShelfBgPath;
    QString m_readerBgPath;
    QString m_canvasBgPath;
    QString m_noteBgPath;
    QString m_libraryBgPath;
    QString m_musicBgPath;
    QString m_noteDefaultFont = "Microsoft YaHei";
    int m_noteDefaultFontSize = 14;

    // Music shortcuts
    QString m_scPlayPause = "Ctrl+P";
    QString m_scPrev = "Ctrl+Left";
    QString m_scNext = "Ctrl+Right";
    QString m_scVolUp = "Ctrl+Up";
    QString m_scVolDown = "Ctrl+Down";
    QList<QShortcut*> m_musicShortcuts;

    // Timer shutdown
    bool m_timerShutdownEnabled = false;
    int m_timerShutdownHours = 0;
    int m_timerShutdownMinutes = 0;
    bool m_shutdownPCEnabled = false;
    QTimer *m_shutdownTimer = nullptr;
    qint64 m_shutdownRemainingMs = 0;
    void applyTimerShutdown();
    void onShutdownTick();
    QListWidget *m_canvasItemList = nullptr;
    MaterialLibrary *m_sharedLibrary = nullptr;
    ContentBrowserPanel *m_contentBrowser = nullptr;
    UserAuth *m_userAuth = nullptr;
    UserInfo m_currentUser;
    QLabel *m_userAvatar = nullptr;
    QLabel *m_userNickname = nullptr;
    QWidget *m_userWidget = nullptr;
    QMenu *m_userMenu = nullptr;

    // Auto save timer
    QTimer *m_autoSaveTimer = nullptr;

    // System tray
    QSystemTrayIcon *m_tray = nullptr;
    bool m_trayInitialized = false;

    // Actions stored for menu/toolbar
    QList<QAction*> m_actions;
};

#endif // MAINWINDOW_H
