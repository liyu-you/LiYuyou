#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "appsettings.h"
#include "userauth.h"

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
class AutoSaveController;
class ThemeApplier;
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
    void applyAutostart(bool enabled);  // write/remove Windows Run registry key
    void refreshCanvasItemList();
    void showAccountDialog();
    void logoutUser();
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

    // All persistent preferences used to live as 65 loose members here. They
    // are now in AppSettings (appsettings.h); access them via m_cfg.
    // lastPage is UI state (current tab), loaded but re-read from m_stack on save.
    int m_lastPage = 0;
    AppSettings m_cfg;

    QToolBar *m_navToolBar = nullptr;
    QToolButton *m_brandBtn = nullptr;  // 应用图标按钮 = 切换侧边栏
    QToolButton *m_menuHamburger = nullptr;  // 半隐藏模式下的 ☰ 菜单按钮

    QList<QShortcut*> m_musicShortcuts;

    // Timer shutdown runtime state
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

    // Auto-save timer + weekly report are delegated to a dedicated controller.
    AutoSaveController *m_autosave = nullptr;
    ThemeApplier *m_theme = nullptr;

    // System tray
    QSystemTrayIcon *m_tray = nullptr;
    bool m_trayInitialized = false;

    // Actions stored for menu/toolbar
    QList<QAction*> m_actions;
};

#endif // MAINWINDOW_H
