#include "cueparser.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <QRegularExpression>

namespace CueParser {

// MM:SS:FF → milliseconds. FF is 1/75 second.
static qint64 timeToMs(const QString &s)
{
    const QStringList parts = s.split(':');
    if (parts.size() != 3) return 0;
    bool ok = false;
    const int mm = parts[0].toInt(&ok); if (!ok) return 0;
    const int ss = parts[1].toInt(&ok); if (!ok) return 0;
    const int ff = parts[2].toInt(&ok); if (!ok) return 0;
    return qint64(mm) * 60000 + qint64(ss) * 1000 + qint64(ff) * 1000 / 75;
}

// Strip quotes from a CUE string field.
static QString unquote(const QString &s)
{
    QString v = s.trimmed();
    if (v.startsWith('"') && v.endsWith('"') && v.size() >= 2)
        v = v.mid(1, v.size() - 2);
    return v;
}

QList<SongInfo> parse(const QString &cuePath)
{
    QList<SongInfo> out;
    QFile f(cuePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;

    const QDir cueDir = QFileInfo(cuePath).absoluteDir();
    QString currentFile;        // FILE "..." WAVE
    QString currentPerformer;   // global PERFORMER
    QString currentTitle;       // global TITLE
    struct RawTrack { int no; QString title; QString performer; qint64 startMs; };
    QList<RawTrack> tracks;

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith("REM")) continue;

        if (line.startsWith("FILE ", Qt::CaseInsensitive)) {
            // FILE "album.flac" WAVE
            QString rest = line.mid(4).trimmed();
            // strip trailing format token (WAVE/MP3/AIFF)
            const int lastQuote = rest.lastIndexOf('"');
            if (rest.startsWith('"') && lastQuote > 1) {
                currentFile = unquote(rest.left(lastQuote + 1));
            } else {
                // unquoted: first token
                currentFile = rest.section(' ', 0, 0);
            }
        } else if (line.startsWith("PERFORMER ", Qt::CaseInsensitive)) {
            currentPerformer = unquote(line.mid(10).trimmed());
        } else if (line.startsWith("TITLE ", Qt::CaseInsensitive)) {
            currentTitle = unquote(line.mid(6).trimmed());
        } else if (line.startsWith("TRACK ", Qt::CaseInsensitive)) {
            // TRACK 01 AUDIO
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                bool ok = false;
                const int no = parts[1].toInt(&ok);
                if (ok) tracks.append({no, currentTitle, currentPerformer, 0});
            }
        } else if (line.startsWith("TITLE ", Qt::CaseInsensitive) && !tracks.isEmpty()) {
            tracks.last().title = unquote(line.mid(6).trimmed());
        } else if (line.startsWith("PERFORMER ", Qt::CaseInsensitive) && !tracks.isEmpty()) {
            tracks.last().performer = unquote(line.mid(10).trimmed());
        } else if (line.startsWith("INDEX 01", Qt::CaseInsensitive)) {
            // INDEX 01 00:00:00
            const QString time = line.mid(QString("INDEX 01").size()).trimmed();
            if (!tracks.isEmpty()) tracks.last().startMs = timeToMs(time);
        }
    }

    // Build virtual SongInfo entries.
    for (int i = 0; i < tracks.size(); ++i) {
        const auto &t = tracks.at(i);
        SongInfo s;
        const QString absAudio = cueDir.absoluteFilePath(currentFile);
        // Synthetic path: cue#trackIndex
        s.path = cuePath + "#" + QString::number(i);
        s.title = t.title;
        s.artist = t.performer;
        s.album = currentTitle;
        s.trackNumber = QString::number(t.no);
        s.path = cuePath + "#" + QString::number(i);
        // Store the real audio path in a QByteArray member? We use a convention:
        // resolveVirtual() reparses the cue on demand. Duration estimated later.
        Q_UNUSED(absAudio);
        out << s;
    }
    return out;
}

bool isCueVirtualPath(const QString &path)
{
    return path.contains(".cue#", Qt::CaseInsensitive);
}

bool splitVirtualPath(const QString &path, QString &cuePath, int &trackIndex)
{
    const int idx = path.lastIndexOf('#');
    if (idx < 0) return false;
    cuePath = path.left(idx);
    bool ok = false;
    trackIndex = path.mid(idx + 1).toInt(&ok);
    return ok && cuePath.endsWith(".cue", Qt::CaseInsensitive);
}

bool resolveVirtual(const QString &virtualPath, QString &audioPath, qint64 &startMs, qint64 &endMs)
{
    QString cuePath; int idx;
    if (!splitVirtualPath(virtualPath, cuePath, idx)) return false;
    const QList<SongInfo> tracks = parse(cuePath);
    if (idx < 0 || idx >= tracks.size()) return false;

    // Re-parse to get FILE and times. Our parse() doesn't expose them yet;
    // do a direct second pass for the audio path and timings.
    QFile f(cuePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QDir cueDir = QFileInfo(cuePath).absoluteDir();
    QString fileRef;
    struct R { qint64 start; };
    QList<qint64> starts;
    QString curFile;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        if (line.startsWith("FILE ", Qt::CaseInsensitive)) {
            QString rest = line.mid(4).trimmed();
            const int lastQ = rest.lastIndexOf('"');
            curFile = (rest.startsWith('"') && lastQ > 1) ? unquote(rest.left(lastQ + 1)) : rest.section(' ', 0, 0);
        } else if (line.startsWith("INDEX 01", Qt::CaseInsensitive)) {
            starts << timeToMs(line.mid(8).trimmed());
        }
    }
    fileRef = curFile;
    if (fileRef.isEmpty() || idx >= starts.size()) return false;
    audioPath = cueDir.absoluteFilePath(fileRef);
    startMs = starts.at(idx);
    endMs = (idx + 1 < starts.size()) ? starts.at(idx + 1) : 0;
    return true;
}

} // namespace CueParser
