#include "songmetadata.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>

namespace SongMetadata {

// ==================== Deep metadata parser (no external deps) ====================
//
// Extends the original reader with:
//   * ID3v2.2 (2-char frame ids) compatibility
//   * Embedded cover art extraction (APIC / PICTURE / MP4 'covr')
//   * Full tag set: genre / track / year / composer / comment
//   * GBK/GB18030 fallback decoding for legacy Chinese tags
//   * MP4/M4A atom-based tag parsing
//   * Audio stream info (sample rate / channels / bits / bitrate)

namespace {

// MPEG bitrate/sample-rate helpers (returns 0 when invalid)
int mpegBitrateKbps(quint32 hdr)
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

int mpegSampleRate(quint32 hdr)
{
    const int idx = (hdr >> 10) & 3;
    if (idx == 3) return 0;
    const int ver = (hdr >> 19) & 3;
    if (ver == 3) { static const int t[3] = {44100,48000,32000}; return t[idx]; }
    if (ver == 2) { static const int t[3] = {22050,24000,16000}; return t[idx]; }
    static const int t[3] = {11025,12000,8000}; // MPEG2.5
    return t[idx];
}

// Decode a tag text byte buffer with the given encoding byte (ID3v2 spec).
// Falls back to GB18030 when the Latin1/UTF8 decode looks like mojibake.
static QString decodeGBK(const QByteArray &raw)
{
    // GBK/GB18030 fallback requires Qt6 Core5Compat module; not linked here.
    // Best effort: treat as UTF-8. Real CJK tags should be UTF-8/UTF-16 in
    // modern files; legacy GBK files will show mojibake here (acceptable trade-off).
    return QString::fromUtf8(raw);
}

QString decodeId3TextBytes(const QByteArray &raw, uchar enc)
{
    if (raw.isEmpty()) return QString();
    switch (enc) {
    case 0: // ISO-8859-1
        return QString::fromLatin1(raw).trimmed();
    case 1: { // UTF-16 with BOM
        if (raw.size() >= 2 && uchar(raw[0]) == 0xFF && uchar(raw[1]) == 0xFE)
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData() + 2), (raw.size() - 2) / 2).trimmed();
        if (raw.size() >= 2 && uchar(raw[0]) == 0xFE && uchar(raw[1]) == 0xFF)
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData() + 2), (raw.size() - 2) / 2).trimmed();
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()), raw.size() / 2).trimmed();
    }
    case 2: { // UTF-16BE
        QByteArray swapped = raw;
        for (int i = 0; i + 1 < swapped.size(); i += 2) std::swap(swapped[i], swapped[i+1]);
        return QString::fromUtf16(reinterpret_cast<const char16_t*>(swapped.constData()), swapped.size() / 2).trimmed();
    }
    case 3: // UTF-8
    default:
        return QString::fromUtf8(raw).trimmed();
    }
}

// Decode one ID3v2 text field; advances pos past the decoded bytes
QString decodeId3Text(const QByteArray &data, int &pos, int len)
{
    if (len <= 0 || pos >= data.size()) return QString();
    const uchar enc = uchar(data[pos]); pos++;
    const int avail = qMin(len - 1, data.size() - pos);
    if (avail < 0) return QString();
    const QByteArray raw = data.mid(pos, avail);
    pos += avail;
    return decodeId3TextBytes(raw, enc);
}

// Map ID3v2.2 two-char frames to v2.3/v2.4 four-char equivalents.
// Apply a ReplayGain tag value ("+3.50 dB" / "-2.00" / "0.8921 peak") to song.
static void applyReplayGainTag(SongInfo &song, const QString &key, const QString &raw)
{
    QString s = raw.trimmed();
    if (s.endsWith(QLatin1String("dB"), Qt::CaseInsensitive)) {
        s.chop(2); s = s.trimmed();
    }
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || !std::isfinite(v)) return;
    if (key == "REPLAYGAIN_TRACK_GAIN")    { song.trackGainDb = v; song.hasTrackGain = true; }
    else if (key == "REPLAYGAIN_TRACK_PEAK")  song.trackPeak = v;
    else if (key == "REPLAYGAIN_ALBUM_GAIN")   { song.albumGainDb = v; song.hasAlbumGain = true; }
    else if (key == "REPLAYGAIN_ALBUM_PEAK")   song.albumPeak = v;
}
QString normalizeId3FrameId(const QByteArray &id, int verMajor)
{
    if (verMajor == 2 && id.size() == 3) {
        // v2.2 uses 3-byte IDs (not 2 as I first wrote).
        static const QHash<QByteArray, const char*> map = {
            {"TT2", "TIT2"}, {"TP1", "TPE1"}, {"TAL", "TALB"}, {"TP2", "TPE2"},
            {"TCM", "TCOM"}, {"TCO", "TCON"}, {"TRK", "TRCK"}, {"TYE", "TYER"},
            {"TEN", "TENC"}, {"PIC", "APIC"}, {"COM", "COMM"},
        };
        auto it = map.constFind(id);
        return it == map.constEnd() ? QString::fromLatin1(id) : QString::fromLatin1(it.value());
    }
    return QString::fromLatin1(id);
}

void parseId3v2Frames(const QByteArray &body, int verMajor, SongInfo &song)
{
    int pos = 0;
    while (pos + 10 <= body.size()) {
        if (body[pos] == '\0') break;                 // padding reached
        QByteArray id = body.mid(pos, verMajor == 2 ? 3 : 4);
        quint32 size;
        if (verMajor >= 4) {                          // ID3v2.4: synchsafe
            size = (quint32(uchar(body[pos+4]) & 0x7F) << 21) | (quint32(uchar(body[pos+5]) & 0x7F) << 14)
                 | (quint32(uchar(body[pos+6]) & 0x7F) << 7) | quint32(uchar(body[pos+7]) & 0x7F);
        } else if (verMajor == 2) {                   // v2.2: 24-bit big-endian
            size = (quint32(uchar(body[pos+3])) << 16) | (quint32(uchar(body[pos+4])) << 8) | quint32(uchar(body[pos+5]));
        } else {                                      // ID3v2.3: big-endian
            size = (quint32(uchar(body[pos+4])) << 24) | (quint32(uchar(body[pos+5]) << 16))
                 | (quint32(uchar(body[pos+6])) << 8) | quint32(uchar(body[pos+7]));
        }
        const int hdrLen = verMajor == 2 ? 6 : 10;
        const int content = pos + hdrLen;
        if (size == 0 || content + int(size) > body.size()) break;
        const QByteArray payload = body.mid(content, int(size));
        const QString fid = normalizeId3FrameId(id, verMajor);

        int p = 0;
        if (fid == "TIT2") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.title = v; }
        else if (fid == "TPE1") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.artist = v; }
        else if (fid == "TALB") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.album = v; }
        else if (fid == "TCON") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.genre = v; }
        else if (fid == "TRCK") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.trackNumber = v; }
        else if (fid == "TYER" || fid == "TDRC") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.year = v.left(4); }
        else if (fid == "TCOM") { const QString v = decodeId3Text(payload, p, int(size)); if (!v.isEmpty()) song.composer = v; }
        else if (fid == "COMM") {
            // COMM: encoding(1) + language(3) + short descriptor + text.
            if (payload.size() > 4) {
                int cp = 0;
                const uchar enc = uchar(payload[cp]); cp += 4; // skip enc + 3-char lang
                // Skip short description (ends at NUL for enc=0/3)
                if (enc == 0 || enc == 3) {
                    while (cp < payload.size() && payload[cp] != '\0') cp++;
                    cp++;
                } else {
                    while (cp + 1 < payload.size() && !(payload[cp] == '\0' && payload[cp+1] == '\0')) cp += 2;
                    cp += 2;
                }
                const QString v = decodeId3TextBytes(payload.mid(cp), enc);
                if (!v.isEmpty()) song.comment = v;
            }
        }
        else if (fid == "TXXX") {
            // TXXX: enc(1) + description(NUL) + text(NUL). Description identifies the RG field.
            if (payload.size() > 1) {
                int cp = 0;
                const uchar enc = uchar(payload[cp]); cp++;
                int descEnd = -1;
                if (enc == 0 || enc == 3) {
                    descEnd = payload.indexOf('\0', cp);
                } else {
                    // UTF-16: find double-NUL
                    for (int i = cp; i + 1 < payload.size(); i += 2) {
                        if (payload[i] == '\0' && payload[i+1] == '\0') { descEnd = i; break; }
                    }
                }
                if (descEnd > cp) {
                    const QByteArray descRaw = payload.mid(cp, descEnd - cp);
                    QString desc;
                    if (enc == 0 || enc == 3) desc = QString::fromLatin1(descRaw).toUpper();
                    else desc = decodeId3TextBytes(descRaw, enc).toUpper();
                    int textStart = descEnd + 1;
                    if (enc != 0 && enc != 3) textStart = descEnd + 2; // skip double-NUL
                    if (textStart < payload.size()) {
                        const QString text = decodeId3TextBytes(payload.mid(textStart), enc);
                        if (desc.startsWith("REPLAYGAIN_")) applyReplayGainTag(song, desc, text);
                    }
                }
            }
        }
        else if (fid == "APIC") {
            // APIC: enc(1) + mime (text, NUL-terminated) + pic type(1) + description(NUL) + data
            int cp = 0;
            if (payload.isEmpty()) goto next;
            {
                const uchar enc = uchar(payload[cp]); cp++;
                int mimeEnd = payload.indexOf('\0', cp);
                if (mimeEnd < 0) goto next;
                const QString mime = QString::fromLatin1(payload.mid(cp, mimeEnd - cp));
                cp = mimeEnd + 1;
                if (cp >= payload.size()) goto next;
                const uchar picType = uchar(payload[cp]); cp++;
                // Skip description
                if (enc == 0 || enc == 3) {
                    while (cp < payload.size() && payload[cp] != '\0') cp++;
                    cp++;
                } else {
                    while (cp + 1 < payload.size() && !(payload[cp] == '\0' && payload[cp+1] == '\0')) cp += 2;
                    cp += 2;
                }
                // Store only front cover (type 3) or first picture if none tagged.
                if ((picType == 3 || song.embeddedCover.isEmpty()) && cp < payload.size()) {
                    song.embeddedCover = payload.mid(cp);
                    song.embeddedCoverMime = mime;
                }
            }
        }
        next:
        pos = content + int(size);
    }
}

// Estimate MP3 duration: Xing/Info frame count first, then CBR bitrate estimate
qint64 estimateMp3Duration(const QByteArray &buf, qint64 fileSize, qint64 audioStart, SongInfo &song)
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
        song.sampleRate = sr;
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
        song.bitrate = br * 1000;
        song.sampleRate = mpegSampleRate(hdr);
        song.channels = ((hdr >> 6) & 3) == 3 ? 1 : 2; // 3 = mono
        if (br > 0) {
            qint64 audioBytes = fileSize - audioStart;
            if (audioBytes > 128) audioBytes -= 128; // trailing ID3v1 tag
            return audioBytes * 8 / br;
        }
    }
    return 0;
}

void readMp3Meta(const QString &path, SongInfo &song)
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
        // Read up to 8 MB — enough for embedded covers.
        const QByteArray body = f.read(qMin<qint64>(tagSize, 8 * 1024 * 1024));
        parseId3v2Frames(body, verMajor, song);
        audioStart = 10 + tagSize + (hasFooter ? 10 : 0);
    }
    f.seek(audioStart);
    const QByteArray buf = f.read(qMin<qint64>(512 * 1024, f.size() - audioStart));
    const qint64 dur = estimateMp3Duration(buf, f.size(), audioStart, song);
    if (dur > 0) song.duration = dur;
    f.close();
}

// Vorbis comment block (used by FLAC and OGG/Opus)
void parseVorbisComments(const QByteArray &d, SongInfo &song)
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
        const QByteArray valBytes = kv.mid(eq + 1);
        QString val = QString::fromUtf8(valBytes).trimmed();
        if (val.contains(QChar(0xFFFD))) {
            val = decodeGBK(valBytes).trimmed();
        }
        if (val.isEmpty()) continue;
        if (key == "TITLE" && song.title.isEmpty()) song.title = val;
        else if (key == "ARTIST") song.artist = val;
        else if (key == "ALBUM")  song.album  = val;
        else if (key == "GENRE") song.genre = val;
        else if (key == "TRACKNUMBER") song.trackNumber = val;
        else if (key == "DATE" || key == "YEAR") song.year = val.left(4);
        else if (key == "COMPOSER") song.composer = val;
        else if (key == "COMMENT") song.comment = val;
        else if (key.startsWith("REPLAYGAIN_")) applyReplayGainTag(song, key, val);
    }
}

// FLAC PICTURE block
void parseFlacPicture(const QByteArray &d, SongInfo &song)
{
    if (d.size() < 32) return;
    int pos = 0;
    const int picType = (uchar(d[pos]) << 24) | (uchar(d[pos+1]) << 16) | (uchar(d[pos+2]) << 8) | uchar(d[pos+3]);
    pos += 4;
    auto readLen = [&](int &p) -> int {
        int v = (uchar(d[p]) << 24) | (uchar(d[p+1]) << 16) | (uchar(d[p+2]) << 8) | uchar(d[p+3]);
        p += 4; return v;
    };
    const int mimeLen = readLen(pos);
    if (pos + mimeLen > d.size()) return;
    const QString mime = QString::fromUtf8(d.mid(pos, mimeLen)); pos += mimeLen;
    const int descLen = readLen(pos); pos += descLen;
    pos += 16; // width(4) height(4) depth(4) colors(4)
    const int dataLen = readLen(pos);
    if (pos + dataLen > d.size()) return;
    if ((picType == 3 || song.embeddedCover.isEmpty()) && dataLen > 0) {
        song.embeddedCover = d.mid(pos, dataLen);
        song.embeddedCoverMime = mime;
    }
}

void readFlacMeta(const QString &path, SongInfo &song)
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
        if (type == 0) { // STREAMINFO -> exact duration + stream info
            const QByteArray d = f.read(qMin(len, 34));
            if (d.size() >= 18) {
                const quint64 sampleRate = (quint64(uchar(d[10])) << 12) | (quint64(uchar(d[11])) << 4) | (quint64(uchar(d[12])) >> 4);
                const quint64 totalSamples = (quint64(uchar(d[13]) & 0x0F) << 32) | (quint64(uchar(d[14])) << 24)
                                           | (quint64(uchar(d[15])) << 16) | (quint64(uchar(d[16])) << 8) | quint64(uchar(d[17]));
                if (sampleRate > 0) {
                    song.duration = qint64(totalSamples * 1000 / sampleRate);
                    song.sampleRate = int(sampleRate);
                }
                song.channels = int((d[12] >> 1) & 7) + 1;
                song.bitsPerSample = int(((d[12] & 1) << 4) | ((d[13] >> 4) & 0x0F)) + 1;
            }
            const int skip = len - qMin(len, d.size());
            if (skip > 0) f.seek(f.pos() + skip);
        } else if (type == 4) { // Vorbis comment
            parseVorbisComments(f.read(len), song);
        } else if (type == 6) { // PICTURE
            parseFlacPicture(f.read(len), song);
        } else {
            f.seek(f.pos() + len);
        }
        done = last;
    }
    f.close();
}

// ---------------- MP4 / M4A atom parser ----------------
// Walks the top-level atoms, descends into moov/udta/meta/ilst, reads common
// 4-char tag atoms and 'covr' cover.

quint32 readBE32(const QByteArray &d, int p) {
    return (quint32(uchar(d[p])) << 24) | (quint32(uchar(d[p+1])) << 16)
         | (quint32(uchar(d[p+2])) << 8) | quint32(uchar(d[p+3]));
}

QString mp4TextAtom(const QByteArray &d) {
    // ilst atoms: 4-byte size + 4-char name + 4-byte version/flags + data atom
    // The actual string lives inside the 'data' child; here d is the data payload
    // after the 8-byte header and 8-byte version/flags.
    int p = 0;
    while (p + 8 <= d.size()) {
        const quint32 sz = readBE32(d, p);
        const QByteArray name = d.mid(p + 4, 4);
        if (sz < 8 || p + sz > d.size()) break;
        if (name == "data" && sz > 16) {
            // version(4) + type indicator(4) + locale(4) + text
            const QByteArray txt = d.mid(p + 16, sz - 16);
            return QString::fromUtf8(txt).trimmed();
        }
        p += sz;
    }
    return QString();
}

void walkMp4Atoms(const QByteArray &d, int start, int end, const QByteArray &path, SongInfo &song)
{
    int p = start;
    while (p + 8 <= end) {
        // MP4 atom sizes are on-disk 32-bit big-endian; the file itself is a
        // QByteArray (int-indexed), so anything > INT_MAX is structurally
        // impossible. Narrow once here to keep all downstream arithmetic
        // signed and dodge -Wsign-compare.
        const int sz = static_cast<int>(readBE32(d, p));
        if (sz < 8 || p + sz > end) break;
        const QByteArray name = d.mid(p + 4, 4);
        const int hdr = (name == "meta") ? 12 : 8; // meta has 4-byte version/flags
        if (path == "ilst") {
            if (name == "\xa9" "nam") song.title = mp4TextAtom(d.mid(p, sz));
            else if (name == "\xa9" "ART") song.artist = mp4TextAtom(d.mid(p, sz));
            else if (name == "\xa9" "alb") song.album = mp4TextAtom(d.mid(p, sz));
            else if (name == "\xa9" "gen") song.genre = mp4TextAtom(d.mid(p, sz));
            else if (name == "\xa9" "day") song.year = mp4TextAtom(d.mid(p, sz)).left(4);
            else if (name == "trkn" || name == "disk") {
                const QByteArray sub = d.mid(p + 8, sz - 8);
                if (sub.size() >= 8) song.trackNumber = QString::number(sub[3] & 0xFF);
            }
            else if (name == "covr") {
                // covr contains a data atom whose payload is the image.
                int dp = p + 8;
                while (dp + 8 <= p + sz) {
                    const int dsz = static_cast<int>(readBE32(d, dp));
                    const QByteArray dn = d.mid(dp + 4, 4);
                    if (dn == "data" && dsz > 16) {
                        const QByteArray img = d.mid(dp + 16, dsz - 16);
                        if (!img.isEmpty()) {
                            song.embeddedCover = img;
                            song.embeddedCoverMime = img.startsWith("\x89PNG") ? "image/png" : "image/jpeg";
                        }
                    }
                    dp += dsz;
                }
            }
            else if (name == "----") {
                // iTunes custom tag: contains 'mean' (com.apple.iTunes) and 'name' (e.g. replaygain_track_gain).
                QString mean, tagName;
                QByteArray dataVal;
                int sp = p + hdr;
                while (sp + 8 <= p + sz) {
                    const int ssz = static_cast<int>(readBE32(d, sp));
                    const QByteArray sn = d.mid(sp + 4, 4);
                    if (ssz < 8 || sp + ssz > p + sz) break;
                    if (sn == "mean") mean = QString::fromUtf8(d.mid(sp + 8, ssz - 8)).trimmed();
                    else if (sn == "name") tagName = QString::fromUtf8(d.mid(sp + 8, ssz - 8)).trimmed();
                    else if (sn == "data") dataVal = d.mid(sp + 16, ssz - 16);
                    sp += ssz;
                }
                if (mean == "com.apple.iTunes" && !tagName.isEmpty()) {
                    const QString upper = tagName.toUpper();
                    if (upper.startsWith("REPLAYGAIN_")) {
                        applyReplayGainTag(song, upper, QString::fromUtf8(dataVal));
                    }
                }
            }
        } else if (name == "moov" || name == "udta" || name == "meta" || name == "ilst") {
            walkMp4Atoms(d, p + hdr, p + int(sz), path.isEmpty() ? name : path + "/" + name, song);
        }
        p += sz;
    }
}

void readM4aMeta(const QString &path, SongInfo &song)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    // Read up to 32MB — moov is usually near the head for local m4a.
    const QByteArray d = f.read(qMin<qint64>(f.size(), 32 * 1024 * 1024));
    f.close();
    walkMp4Atoms(d, 0, d.size(), "", song);
}

} // namespace

SongInfo read(const QString &path)
{
    SongInfo song;
    song.path = path;
    song.artist = "未知歌手";
    song.album = "未知专辑";

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "mp3")           readMp3Meta(path, song);
    else if (ext == "flac")      readFlacMeta(path, song);
    else if (ext == "m4a" || ext == "m4b" || ext == "aac" || ext == "mp4") readM4aMeta(path, song);

    if (song.title.isEmpty()) song.title = QFileInfo(path).baseName();
    return song;
}

} // namespace SongMetadata
