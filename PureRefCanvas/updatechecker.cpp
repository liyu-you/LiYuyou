#include "updatechecker.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>

// Version injected by CMake (see CMakeLists.txt target_compile_definitions)
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

QVersionNumber UpdateChecker::currentVersion()
{
    return QVersionNumber::fromString(QStringLiteral(APP_VERSION));
}

QString UpdateChecker::currentVersionString()
{
    return QStringLiteral(APP_VERSION);
}

void UpdateChecker::checkForUpdates(const QString &updateJsonUrl)
{
    if (updateJsonUrl.isEmpty()) {
        emit checkFailed("更新源地址未配置");
        return;
    }
    QNetworkRequest request{QUrl(updateJsonUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("LiYuYou-Updater/%1").arg(currentVersionString()));
    m_checkReply = m_manager->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, &UpdateChecker::onCheckFinished);
}

void UpdateChecker::onCheckFinished()
{
    if (!m_checkReply) return;
    if (m_checkReply->error() != QNetworkReply::NoError) {
        emit checkFailed(m_checkReply->errorString());
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
        return;
    }
    QByteArray data = m_checkReply->readAll();
    m_checkReply->deleteLater();
    m_checkReply = nullptr;

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit checkFailed("版本信息解析失败");
        return;
    }
    QJsonObject obj = doc.object();
    UpdateInfo info;
    info.version = QVersionNumber::fromString(obj.value("version").toString());
    info.downloadUrl = obj.value("download_url").toString();
    info.releaseNotes = obj.value("release_notes").toString();
    info.forceUpdate = obj.value("force_update").toBool(false);
    info.isValid = !info.version.isNull() && !info.downloadUrl.isEmpty();

    if (!info.isValid) {
        emit checkFailed("版本信息不完整");
        return;
    }
    if (info.version > currentVersion()) {
        emit updateAvailable(info);
    } else {
        emit noUpdateAvailable();
    }
}

void UpdateChecker::downloadUpdate(const QString &downloadUrl, const QString &savePath)
{
    if (downloadUrl.isEmpty() || savePath.isEmpty()) {
        emit downloadFailed("下载地址或保存路径为空");
        return;
    }
    m_downloadPath = savePath;
    m_downloadFile = new QFile(savePath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit downloadFailed("无法创建下载文件");
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
        return;
    }
    QNetworkRequest request{QUrl(downloadUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("LiYuYou-Updater/%1").arg(currentVersionString()));
    m_downloadReply = m_manager->get(request);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &UpdateChecker::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &UpdateChecker::onDownloadProgress);
}

void UpdateChecker::onDownloadReadyRead()
{
    if (m_downloadFile && m_downloadReply)
        m_downloadFile->write(m_downloadReply->readAll());
}

void UpdateChecker::onDownloadFinished()
{
    if (m_downloadFile) {
        m_downloadFile->flush();
        m_downloadFile->close();
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    if (!m_downloadReply) return;
    if (m_downloadReply->error() != QNetworkReply::NoError) {
        emit downloadFailed(m_downloadReply->errorString());
        QFile::remove(m_downloadPath);
    } else {
        emit updateDownloaded(m_downloadPath);
    }
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total)
{
    emit downloadProgress(received, total);
}

void UpdateChecker::installAndRestart(const QString &newExePath)
{
    QString currentExe = QCoreApplication::applicationFilePath();
    if (currentExe.isEmpty() || !QFile::exists(newExePath)) return;

    // Create a batch script that replaces the exe and restarts the app.
    // We use a .bat because the running exe cannot overwrite itself on Windows.
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString batPath = tempDir + "/liyuyou_update.bat";
    QFile bat(batPath);
    if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&bat);
        out << "@echo off\r\n";
        out << "timeout /t 1 /nobreak >nul\r\n";
        out << "copy /Y \"" << QDir::toNativeSeparators(newExePath) << "\" \""
            << QDir::toNativeSeparators(currentExe) << "\"\r\n";
        out << "del \"" << QDir::toNativeSeparators(newExePath) << "\"\r\n";
        out << "start \"\" \"" << QDir::toNativeSeparators(currentExe) << "\"\r\n";
        out << "del \"%~f0\"\r\n";
        bat.close();
    }
    QProcess::startDetached("cmd.exe", {"/c", batPath});
    QCoreApplication::quit();
}
