#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "appsettings.h"
#include "userauth.h"
#include "spacemanager.h"

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
class DesktopStickyManager;
class ReaderWidget;
class MusicPlayerWidget;
class QLabel;
class QListWidget;
class MaterialLibrary;
class ContentBrowserPanel;
class UserAuth;
class AutoSaveController;
class ThemeApplier;
class ShutdownController;
class TrayController;
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
    DesktopStickyManager *m_stickyMgr = nullptr;   // 桌面便签（笔记置顶为桌面参考）

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

    // Timer shutdown is delegated to a dedicated controller.
    ShutdownController *m_shutdown = nullptr;

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

    // System tray is delegated to a dedicated controller.
    TrayController *m_trayCtl = nullptr;

    // Spaces (top-level theme containers). Owned by MainWindow.
    SpaceManager *m_spaceMgr = nullptr;
    QToolButton *m_spaceBtn = nullptr;  // capsule showing current space name
    void setupSpaceSwitcher();
    void showSpaceMenu();
    void updateSpaceButton();
    void openSpaceManager();
    void toggleImmersiveMode();
    void enterImmersiveMode();
    void exitImmersiveMode();
    bool m_immersive = false;
    bool m_wasMaximized = false;

    // Space state memory + environment
    void onSpaceChanged(const QString &newId);
    void saveCurrentSpaceState();
    void restoreSpaceState(const Space &s);
    void applySpaceEnvironment(const Space &s);
    QString m_lastSpaceId;

    // Focus timer
    QTimer *m_focusTimer = nullptr;
    void onFocusTick();

    // Space ritual overlay
    void showSpaceWelcome(const Space &s);

    // Per-space canvas
    QString canvasAutosavePath(const QString &spaceId) const;
    void saveCanvasToSpace(const QString &spaceId);
    void loadCanvasForSpace(const QString &spaceId);

    // Actions stored for menu/toolbar
    QList<QAction*> m_actions;
};

#endif // MAINWINDOW_H
