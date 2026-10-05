#include "logger.h"
#include "userauth.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QMutex>
#include <QtGlobal>

namespace {
QMutex g_logMutex;
QFile  g_logFile;
QString g_logPath;
bool    g_fileEnabled = false;

constexpr int RETENTION_DAYS = 7;
}

void Logger::install(bool enableFileLog)
{
    g_fileEnabled = enableFileLog;
    if (g_fileEnabled) {
        QDir().mkpath(logsDir());
        pruneOldLogs();
        g_logPath = logsDir() + "/" +
                    QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log";
        g_logFile.setFileName(g_logPath);
        g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    qInstallMessageHandler(&Logger::messageHandler);
}

void Logger::setEnabled(bool on)
{
    QMutexLocker lock(&g_logMutex);
    if (on == g_fileEnabled) return;
    g_fileEnabled = on;
    if (on) {
        QDir().mkpath(logsDir());
        pruneOldLogs();
        g_logPath = logsDir() + "/" +
                    QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log";
        g_logFile.setFileName(g_logPath);
        g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    } else {
        g_logFile.close();
    }
}

QString Logger::logsDir()
{
    return UserAuth::dataRoot() + "/logs";
}

void Logger::openLogsDirectory()
{
    QDir().mkpath(logsDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(logsDir()));
}

void Logger::pruneOldLogs()
{
    QDir dir(logsDir());
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-RETENTION_DAYS);
    for (const QFileInfo &fi : dir.entryInfoList({"*.log"}, QDir::Files)) {
        if (fi.birthTime() < cutoff || fi.lastModified() < cutoff) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QMutexLocker lock(&g_logMutex);

    const QDateTime now = QDateTime::currentDateTime();
    const QString ts = now.toString("yyyy-MM-dd HH:mm:ss.zzz");
    const char level =
        type == QtDebugMsg    ? 'D' :
        type == QtWarningMsg  ? 'W' :
        type == QtCriticalMsg ? 'C' :
        type == QtFatalMsg    ? 'F' : 'I';

    const QString line = QString("[%1] [%2] %3\n").arg(ts).arg(level).arg(msg);

    // Always echo to stderr (debugger / VSCode output)
    QTextStream err(stderr);
    err << line;

    if (g_fileEnabled && g_logFile.isOpen()) {
        QTextStream out(&g_logFile);
        out << line;
        out.flush();
    }
}
