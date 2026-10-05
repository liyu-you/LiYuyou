#ifndef LOGGER_H
#define LOGGER_H

// C++23: file-backed logger installed via qInstallMessageHandler.
//
// All qDebug/qWarning/qCritical output is routed here. When enabled, logs
// are appended to dataRoot()/logs/yyyy-MM-dd.log. Old logs (older than
// RETENTION_DAYS) are pruned at startup.
//
// Install once at main() after QCoreApplication is created and after
// UserAuth::dataRoot() is resolvable. Toggle at runtime via setEnabled().

#include <QtGlobal>
#include <QString>

class Logger
{
public:
    // Install the message handler. Must be called once.
    // If enableFileLog is false, messages go to stderr only (default Qt behavior).
    static void install(bool enableFileLog);

    // Toggle file logging at runtime (from settings dialog).
    static void setEnabled(bool on);

    // Open the logs directory in the file explorer.
    static void openLogsDirectory();

    // Directory where logs are written.
    static QString logsDir();

private:
    static void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
    static void pruneOldLogs();
};

#endif // LOGGER_H
