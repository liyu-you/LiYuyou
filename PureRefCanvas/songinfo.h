#ifndef SONGINFO_H
#define SONGINFO_H

// Plain data types describing one library entry. Kept in their own header so the
// metadata reader, the player widget and the song table can all agree on them
// without pulling in the whole player.

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
};

struct PlaylistInfo {
    QString name;
    QStringList songPaths;
};

#endif // SONGINFO_H
