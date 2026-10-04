#ifndef SONGMETADATA_H
#define SONGMETADATA_H

// Tag reader for local audio files, split out of musicplayer.cpp so the parsing
// can be tested and measured on its own (and so the player file is not 2500 lines
// of mixed concerns).
//
// Coverage matches what the player had before: ID3v2 for mp3 and Vorbis comments
// for flac. Other formats keep the file name as the title, exactly as before.

#include "songinfo.h"

namespace SongMetadata {

SongInfo read(const QString &path);

} // namespace SongMetadata

#endif // SONGMETADATA_H
