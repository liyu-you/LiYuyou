#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QVersionNumber>

class QNetworkAccessManager;
class QNetworkReply;
class QProgressDialog;
class QFile;

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    struct UpdateInfo {
        QVersionNumber version;
        QString downloadUrl;
        QString releaseNotes;
        bool forceUpdate = false;
        bool isValid = false;
    };

    explicit UpdateChecker(QObject *parent = nullptr);

    // Current app version (defined in CMakeLists, injected via compile definitions)
    static QVersionNumber currentVersion();
    static QString currentVersionString();

    // Check for updates at the given URL. Emits updateAvailable or noUpdateAvailable.
    void checkForUpdates(const QString &updateJsonUrl);

    // Download the update to a temp file. Emits downloadProgress and updateDownloaded.
    void downloadUpdate(const QString &downloadUrl, const QString &savePath);

    // Install the downloaded update: replaces current exe and restarts.
    // Call this after updateDownloaded signal.
    static void installAndRestart(const QString &newExePath);

signals:
    void updateAvailable(const UpdateInfo &info);
    void noUpdateAvailable();
    void checkFailed(const QString &error);
    void downloadProgress(qint64 received, qint64 total);
    void updateDownloaded(const QString &filePath);
    void downloadFailed(const QString &error);

private slots:
    void onCheckFinished();
    void onDownloadReadyRead();
    void onDownloadFinished();
    void onDownloadProgress(qint64 received, qint64 total);

private:
    QNetworkAccessManager *m_manager;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_downloadPath;
};

#endif // UPDATECHECKER_H
