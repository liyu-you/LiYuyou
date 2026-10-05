#ifndef THUMBCACHE_H
#define THUMBCACHE_H

// Disk-backed thumbnail cache shared by the content browser and library grid.
// Keyed by (file path + mtime) so edited source images regenerate automatically.
//
// Entries are stored as WebP when Qt can write it (Google's libwebp, shipped as
// Qt's qwebp plugin) because a lossy WebP thumbnail is roughly a third the size
// of the equivalent PNG, which makes the first paint after a cold start much
// faster. The format is probed at runtime rather than assumed: a Qt build
// without the WebP plugin keeps using PNG, and both variants are readable, so a
// cache written before the plugin was installed is still reused as-is.

#include <QString>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QImageWriter>
#include <QPixmap>

namespace ThumbCache {

inline QString cacheDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::tempPath() + "/liyiyou-thumbs";
    QString dir = base + "/thumbs";
    QDir().mkpath(dir);
    return dir;
}

// True when this Qt build can encode WebP (Qt Image Formats component present).
inline bool webpWritable()
{
    static const bool available =
        QImageWriter::supportedImageFormats().contains(QByteArrayLiteral("webp"));
    return available;
}

// Cache path without an extension; the extension follows the format actually
// used, so lookups have to probe both.
inline QString cacheBasePath(const QString &filePath)
{
    QFileInfo fi(filePath);
    // v3: bumped from 360px to 480px max edge for sharper HiDPI cards.
    const QString key = "v3#" + filePath + "#" + QString::number(fi.lastModified().toSecsSinceEpoch());
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5).toHex();
    return cacheDir() + "/" + QString::fromLatin1(hash);
}

// Existing cache entry for a source image, preferring the smaller WebP copy.
inline QString cachedFileFor(const QString &filePath)
{
    const QString base = cacheBasePath(filePath);
    const QString webp = base + ".webp";
    if (QFile::exists(webp)) return webp;
    const QString png = base + ".png";
    if (QFile::exists(png)) return png;
    return QString();
}

// Load a cached thumbnail; returns a null pixmap when absent.
inline QPixmap load(const QString &filePath)
{
    const QString file = cachedFileFor(filePath);
    if (file.isEmpty()) return QPixmap();
    QPixmap p(file);
    return p.isNull() ? QPixmap() : p;
}

// Best-effort persist a thumbnail.
inline void save(const QString &filePath, const QPixmap &pix)
{
    if (pix.isNull()) return;
    const QString base = cacheBasePath(filePath);
    const bool webp = webpWritable();
    // Quality 85 is visually lossless for thumbnails at a fraction of PNG's size.
    const bool ok = webp ? pix.save(base + ".webp", "webp", 85)
                         : pix.save(base + ".png", "png");
    if (ok) {
        // Drop the other variant so a stale copy can never win the lookup above.
        QFile::remove(base + (webp ? ".png" : ".webp"));
    }
}

// Remove all cached thumbnails from disk.
inline void clear()
{
    QDir dir(cacheDir());
    const QStringList files = dir.entryList(QStringList() << "*.png" << "*.webp", QDir::Files);
    for (const QString &f : files)
        QFile::remove(dir.filePath(f));
}

} // namespace ThumbCache

#endif
