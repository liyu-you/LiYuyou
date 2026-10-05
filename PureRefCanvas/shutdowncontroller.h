#ifndef SHUTDOWNCONTROLLER_H
#define SHUTDOWNCONTROLLER_H

// C++23 refactor: timer-shutdown countdown was two inline methods on
// MainWindow (applyTimerShutdown / onShutdownTick) plus a QTimer* member.
// Pulled out here.
//
// The controller owns the per-second tick, the remaining-ms counter and
// the final shutdown sequence. It does NOT know how to save settings or
// quit the app: those go through the Hooks callbacks supplied by MainWindow.

#include <QObject>
#include <functional>

class QTimer;

class ShutdownController : public QObject
{
    Q_OBJECT
public:
    struct Hooks {
        std::function<void()> saveSettings;  // persist before quitting
        std::function<void()> quit;           // qApp->quit()
    };

    explicit ShutdownController(Hooks hooks, QObject *parent = nullptr);

    // (Re)arm the countdown. Call from settings dialog or startup.
    void apply(bool enabled, int hours, int minutes, bool shutdownPC);

private:
    void onTick();

    Hooks  m_hooks;
    QTimer *m_timer = nullptr;
    qint64 m_remainingMs = 0;
    bool   m_shutdownPC = false;
};

#endif // SHUTDOWNCONTROLLER_H
