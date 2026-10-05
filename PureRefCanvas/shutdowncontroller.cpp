#include "shutdowncontroller.h"

#include <QTimer>
#include <QProcess>

ShutdownController::ShutdownController(Hooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks))
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &ShutdownController::onTick);
}

void ShutdownController::apply(bool enabled, int hours, int minutes, bool shutdownPC)
{
    m_timer->stop();

    if (!enabled) return;

    const qint64 totalMs = (hours * 3600LL + minutes * 60LL) * 1000LL;
    if (totalMs <= 0) return;

    m_remainingMs = totalMs;
    m_shutdownPC  = shutdownPC;
    m_timer->start();
}

void ShutdownController::onTick()
{
    m_remainingMs -= 1000;
    if (m_remainingMs > 0) return;

    m_timer->stop();
    if (m_hooks.saveSettings) m_hooks.saveSettings();
    if (m_shutdownPC)
        QProcess::startDetached("shutdown", {"/s", "/t", "0"});
    if (m_hooks.quit) m_hooks.quit();
}
