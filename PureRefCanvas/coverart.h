#ifndef COVERART_H
#define COVERART_H

// Cover art lookup shared by the player bar, the mini player and the full-screen
// player, so all three agree on what a song's cover is:
//   1. an image next to the audio file with the same base name
//   2. the usual folder-art names (cover/folder/front/album/AlbumArt)
// Returns an empty string when the song has no art, which callers render as a
// title-tinted tile rather than a blank square.

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace CoverArt {

inline QString find(const QString &audioPath)
{
    if (audioPath.isEmpty()) return QString();
    const QFileInfo fi(audioPath);
    const QString dir = fi.absolutePath();
    const QString base = fi.completeBaseName();

    static const QStringList exts = {"jpg", "jpeg", "png", "webp"};
    for (const QString &e : exts) {
        const QString p = dir + QLatin1Char('/') + base + QLatin1Char('.') + e;
        if (QFile::exists(p)) return p;
    }
    static const QStringList folderNames = {"cover", "folder", "front", "album", "AlbumArt"};
    for (const QString &n : folderNames) {
        for (const QString &e : exts) {
            const QString p = dir + QLatin1Char('/') + n + QLatin1Char('.') + e;
            if (QFile::exists(p)) return p;
        }
    }
    return QString();
}

} // namespace CoverArt

#endif // COVERART_H
