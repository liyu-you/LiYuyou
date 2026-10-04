#include "songmetadata.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>

namespace SongMetadata {

// ==================== Lightweight metadata readers (no external deps) ====================

// MPEG bitrate/sample-rate helpers (returns 0 when invalid)
static int mpegBitrateKbps(quint32 hdr)
{
    const int idx = (hdr >> 12) & 0xF;
    if (idx == 0 || idx == 15) return 0;
    const int ver  = (hdr >> 19) & 3;   // 3 = MPEG1, 2 = MPEG2, 0 = MPEG2.5
    const int layer = (hdr >> 17) & 3;  // 1 = Layer III
    if (layer != 1) return 0;
    if (ver == 3) { static const int t[15] = {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}; return t[idx]; }
    static const int t[15] = {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160}; // MPEG2 / 2.5
    return t[idx];
}

static int mpegSampleRate(quint32 hdr)
{
    const int idx = (hdr >> 10) & 3;
    if (idx == 3) return 0;
    const int ver = (hdr >> 19) & 3;
    if (ver == 3) { static const int t[3] = {44100,48000,32000}; return t[idx]; }
    if (ver == 2) { static const int t[3] = {22050,24000,16000}; return t[idx]; }
    static const int t[3] = {11025,12000,8000}; // MPEG2.5
    return t[idx];
}

// Decode one ID3v2 text field; advances pos past the decoded bytes
static QString decodeId3Text(const QByteArray &data, int &pos, int len)
{
    if (len <= 0 || pos >= data.size()) return QString();
    const uchar enc = uchar(data[pos]); pos++;
    const int avail = qMin(len - 1, data.size() - pos);
    if (avail < 0) return QString();
    const QByteArray raw = data.mid(pos, avail);
    pos += avail;
    switch (enc) {
    case 0:  return QString::fromLatin1(raw).trimmed();
    case 1:  // UTF-16 with BOM
        if (raw.size() >= 2 && uchar(raw[0]) == 0xFF && uchar(raw[1]) == 0xFE)
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData() + 2), (raw.size() - 2) / 2).trimmed();
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()), raw.size() / 2).trimmed();
    case 2:  // UTF-16BE
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()), raw.size() / 2).trimmed();
    case 3:  return QString::fromUtf8(raw).trimmed();
    default: return QString();
    }
}

static void parseId3v2Frames(const QByteArray &body, int verMajor, SongInfo &song)
{
    int pos = 0;
    while (pos + 10 <= body.size()) {
        if (body[pos] == '\0') break;                 // padding reached
        const QByteArray id = body.mid(pos, 4);
        quint32 size;
        if (verMajor >= 4) {                          // ID3v2.4: synchsafe
            size = (quint32(uchar(body[pos+4]) & 0x7F) << 21) | (quint32(uchar(body[pos+5]) & 0x7F) << 14)
                 | (quint32(uchar(body[pos+6]) & 0x7F) << 7) | quint32(uchar(body[pos+7]) & 0x7F);
        } else {                                      // ID3v2.2/2.3: big-endian
            size = (quint32(uchar(body[pos+4])) << 24) | (quint32(uchar(body[pos+5])) << 16)
                 | (quint32(uchar(body[pos+6])) << 8) | quint32(uchar(body[pos+7]));
        }
        const int content = pos + 10;
        if (content + int(size) > body.size()) break;
        int p = content;
        if (id == "TIT2" && song.title.isEmpty()) { const QString v = decodeId3Text(body, p, int(size)); if (!v.isEmpty()) song.title = v; }
        else if (id == "TPE1") { const QString v = decodeId3Text(body, p, int(size)); if (!v.isEmpty()) song.artist = v; }
        else if (id == "TALB") { const QString v = decodeId3Text(body, p, int(size)); if (!v.isEmpty()) song.album = v; }
        pos = content + int(size);
    }
}

// Estimate MP3 duration: Xing/Info frame count first, then CBR bitrate estimate
static qint64 estimateMp3Duration(const QByteArray &buf, qint64 fileSize, qint64 audioStart)
{
    int xing = -1;
    for (int i = 0; i + 4 <= buf.size(); ++i) {
        if (buf[i] == 'X' && buf[i+1] == 'i' && buf[i+2] == 'n' && buf[i+3] == 'g') { xing = i; break; }
        if (buf[i] == 'I' && buf[i+1] == 'n' && buf[i+2] == 'f' && buf[i+3] == 'o') { xing = i; break; }
    }
    if (xing >= 4) {
        quint32 hdr = 0;
        for (int k = 0; k < 4; ++k) hdr = (hdr << 8) | uchar(buf[xing - 4 + k]);
        const int sr = mpegSampleRate(hdr);
        const int spf = (((hdr >> 19) & 3) == 3) ? 1152 : 576;
        int off = xing + 4;
        quint32 flags = 0;
        if (off + 4 <= buf.size()) flags = (quint32(uchar(buf[off])) << 24) | (quint32(uchar(buf[off+1])) << 16)
                                         | (quint32(uchar(buf[off+2])) << 8) | quint32(uchar(buf[off+3]));
        off += 4;
        if ((flags & 1) && off + 4 <= buf.size()) {
            const quint32 frames = (quint32(uchar(buf[off])) << 24) | (quint32(uchar(buf[off+1])) << 16)
                                 | (quint32(uchar(buf[off+2])) << 8) | quint32(uchar(buf[off+3]));
            if (frames > 0 && sr > 0) return qint64(frames) * spf * 1000 / sr;
        }
    }
    if (buf.size() >= 4) {
        quint32 hdr = 0;
        for (int k = 0; k < 4; ++k) hdr = (hdr << 8) | uchar(buf[k]);
        const int br = mpegBitrateKbps(hdr);
        if (br > 0) {
            qint64 audioBytes = fileSize - audioStart;
            if (audioBytes > 128) audioBytes -= 128; // trailing ID3v1 tag
            return audioBytes * 8 / br;
        }
    }
    return 0;
}

static void readMp3Meta(const QString &path, SongInfo &song)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray head = f.read(10);
    qint64 audioStart = 0;
    if (head.size() >= 10 && head[0] == 'I' && head[1] == 'D' && head[2] == '3') {
        const int verMajor = head[3];
        const bool hasFooter = head[5] & 0x10;
        const int tagSize = (int(uchar(head[6]) & 0x7F) << 21) | (int(uchar(head[7]) & 0x7F) << 14)
                          | (int(uchar(head[8]) & 0x7F) << 7) | int(uchar(head[9]) & 0x7F);
        const QByteArray body = f.read(qMin(tagSize, 4 * 1024 * 1024));
        parseId3v2Frames(body, verMajor, song);
        audioStart = 10 + tagSize + (hasFooter ? 10 : 0);
    }
    f.seek(audioStart);
    const QByteArray buf = f.read(qMin<qint64>(512 * 1024, f.size() - audioStart));
    const qint64 dur = estimateMp3Duration(buf, f.size(), audioStart);
    if (dur > 0) song.duration = dur;
    f.close();
}

// Vorbis comment block (used by FLAC and OGG/Opus)
static void parseVorbisComments(const QByteArray &d, SongInfo &song)
{
    if (d.size() < 4) return;
    const quint32 vendorLen = quint32(uchar(d[0])) | (quint32(uchar(d[1])) << 8) | (quint32(uchar(d[2])) << 16) | (quint32(uchar(d[3])) << 24);
    int pos = 4 + int(vendorLen);
    if (pos + 4 > d.size()) return;
    const quint32 count = quint32(uchar(d[pos])) | (quint32(uchar(d[pos+1])) << 8) | (quint32(uchar(d[pos+2])) << 16) | (quint32(uchar(d[pos+3])) << 24);
    pos += 4;
    for (quint32 i = 0; i < count && pos + 4 <= d.size(); ++i) {
        const quint32 len = quint32(uchar(d[pos])) | (quint32(uchar(d[pos+1])) << 8) | (quint32(uchar(d[pos+2])) << 16) | (quint32(uchar(d[pos+3])) << 24);
        pos += 4;
        if (pos + int(len) > d.size()) break;
        const QByteArray kv = d.mid(pos, len);
        pos += int(len);
        const int eq = kv.indexOf('=');
        if (eq <= 0) continue;
        const QString key = QString::fromUtf8(kv.left(eq)).toUpper();
        const QString val = QString::fromUtf8(kv.mid(eq + 1)).trimmed();
        if (val.isEmpty()) continue;
        if (key == "TITLE" && song.title.isEmpty()) song.title = val;
        else if (key == "ARTIST") song.artist = val;
        else if (key == "ALBUM")  song.album  = val;
    }
}

static void readFlacMeta(const QString &path, SongInfo &song)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    if (f.read(4) != "fLaC") { f.close(); return; }
    bool done = false;
    while (!done) {
        const QByteArray h = f.read(4);
        if (h.size() < 4) break;
        const bool last = h[0] & 0x80;
        const int type = h[0] & 0x7F;
        const int len = (int(uchar(h[1])) << 16) | (int(uchar(h[2])) << 8) | int(uchar(h[3]));
        if (type == 0) { // STREAMINFO -> exact duration
            const QByteArray d = f.read(qMin(len, 34));
            if (d.size() >= 18) {
                const quint64 sampleRate = (quint64(uchar(d[10])) << 12) | (quint64(uchar(d[11])) << 4) | (quint64(uchar(d[12])) >> 4);
                const quint64 totalSamples = (quint64(uchar(d[13]) & 0x0F) << 32) | (quint64(uchar(d[14])) << 24)
                                           | (quint64(uchar(d[15])) << 16) | (quint64(uchar(d[16])) << 8) | quint64(uchar(d[17]));
                if (sampleRate > 0) song.duration = qint64(totalSamples * 1000 / sampleRate);
            }
            const int skip = len - qMin(len, d.size());
            if (skip > 0) f.seek(f.pos() + skip);
        } else if (type == 4) { // Vorbis comment
            parseVorbisComments(f.read(len), song);
        } else {
            f.seek(f.pos() + len);
        }
        done = last;
    }
    f.close();
}

SongInfo read(const QString &path)
{
    SongInfo song;
    song.path = path;
    song.artist = "未知歌手";
    song.album = "未知专辑";

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "mp3")      readMp3Meta(path, song);
    else if (ext == "flac") readFlacMeta(path, song);

    if (song.title.isEmpty()) song.title = QFileInfo(path).baseName();
    return song;
}

} // namespace SongMetadata

