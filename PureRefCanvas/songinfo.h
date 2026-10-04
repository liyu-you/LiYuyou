#ifndef SONGINFO_H
#define SONGINFO_H

// Plain data types describing one library entry. Kept in their own header so the
// metadata reader, the player widget and the song table can all agree on them
// without pulling in the whole player.

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct SongInfo {
    QString path;
    QString title;
    QString artist;
    QString album;
    qint64 duration = 0; // ms
    bool isOnline = false; // NetEase cloud song (no local file)
    qint64 neteaseId = 0;

    // ---- Rich metadata (filled by deep parser) ----
    QString genre;
    QString trackNumber;     // "3", "3/12"
    QString year;
    QString comment;
    QString composer;

    // Embedded cover art (raw bytes, e.g. JPEG/PNG). Empty when no APIC/PICTURE.
    QByteArray embeddedCover;
    QString embeddedCoverMime; // "image/jpeg" / "image/png"

    // Audio stream info (0 when unknown)
    int sampleRate = 0;       // Hz
    int channels = 0;          // 1/2/...
    int bitsPerSample = 0;     // 16/24/...
    int bitrate = 0;           // kbps

    // Sidecar cover path on disk (cached by the caller after first parse).
    QString coverPath;

    // ReplayGain (0.0 = not present). Linear peak in [0,1]; gain in dB.
    double trackGainDb = 0.0;
    double trackPeak = 0.0;
    double albumGainDb = 0.0;
    double albumPeak = 0.0;
    bool hasTrackGain = false;
    bool hasAlbumGain = false;
};

struct PlaylistInfo {
    QString name;
    QStringList songPaths;
};

#endif // SONGINFO_H
