#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

// C++23 refactor: system-tray setup and close-to-tray logic was inline in
// MainWindow::createTrayIcon() / closeEvent(). Pulled out here.
//
// The controller owns the QSystemTrayIcon, its context menu, and the
// "close button hides to tray" policy. Window show/hide and the settings /
// account dialogs go through Hooks callbacks.

#include <QObject>
#include <QIcon>
#include <functional>

class QSystemTrayIcon;
class QString;

class TrayController : public QObject
{
    Q_OBJECT
public:
    struct Hooks {
        std::function<QIcon()> windowIcon;
        std::function<void()>  showWindow;   // showNormal + raise + activate
        std::function<void()>  hideWindow;
        std::function<void()>  toggleWindow; // tray-click: hide if visible, show if not
        std::function<void()>  openSettings;
        std::function<void()>  showAccount;
        std::function<void()>  quit;
    };

    explicit TrayController(Hooks hooks, QObject *parent = nullptr);

    // Build the tray icon + context menu. No-op if tray unavailable.
    void setup();
    bool initialized() const { return m_initialized; }

    // Called from MainWindow::closeEvent. Returns true if the close was
    // intercepted (window hidden to tray); caller should then event->ignore().
    // Returns false if the window should actually close (closeBehavior != tray
    // or tray unavailable).
    bool handleCloseRequest(const QString &closeBehavior);

private:
    Hooks            m_hooks;
    QSystemTrayIcon *m_tray = nullptr;
    bool             m_initialized = false;
};

#endif // TRAYCONTROLLER_H
