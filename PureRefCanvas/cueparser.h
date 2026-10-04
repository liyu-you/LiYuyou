#ifndef CUEPARSER_H
#define CUEPARSER_H

// Minimal CUE sheet parser. A .cue file describes one audio album split into
// numbered tracks; each track has a title/performer and an INDEX 01 start time
// in MM:SS:FF (frames, 75 fps). We produce virtual SongInfo entries that point
// at the real audio file with start/end offsets.

#include "songinfo.h"
#include <QList>
#include <QString>

namespace CueParser {

struct CueTrack {
    QString audioFile;   // resolved absolute path to the referenced audio file
    QString title;
    QString performer;
    qint64 startMs = 0;
    qint64 endMs = 0;   // exclusive; 0 = end of file
    int trackNumber = 0;
};

// Parse a .cue file. Returns virtual tracks; empty on failure.
// `cuePath` is the absolute path to the .cue file; audio FILE "..." entries are
// resolved relative to the cue file's directory.
QList<SongInfo> parse(const QString &cuePath);

// True when `path` is a synthetic CUE virtual path ("...cue#N").
bool isCueVirtualPath(const QString &path);

// Split a synthetic path into (cueFilePath, trackIndex0Based).
// Returns false when not a CUE path.
bool splitVirtualPath(const QString &path, QString &cuePath, int &trackIndex);

// Given a virtual CUE path, resolve to (realAudioPath, startMs, endMs).
// Returns false when not resolvable.
bool resolveVirtual(const QString &virtualPath, QString &audioPath, qint64 &startMs, qint64 &endMs);

} // namespace CueParser

#endif // CUEPARSER_H
