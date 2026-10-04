#ifndef IMAGEFORMATS_H
#define IMAGEFORMATS_H

// Runtime-probed image format support.
//
// Qt's image formats come from plugins (qjpeg, qwebp, qtiff, qsvg, ...), and a
// given Qt build may not ship all of them - the MinGW Qt 6.11.2 used here has no
// WebP and no TIFF plugin at all. A hard-coded extension list would therefore
// promise support the app does not actually have and the failure would be silent
// (the file is accepted, then never decodes).
//
// Everything that has to answer "is this a readable image?" asks QImageReader
// instead, so the app adapts automatically to whatever plugins are installed -
// including picking up WebP the moment the Qt Image Formats component is added.

#include <QImageReader>
#include <QSet>
#include <QString>
#include <QStringList>

namespace ImageFormats {

// Lower-case extensions decodable by this Qt build. Cached: the plugin set is
// fixed for the lifetime of the process.
inline const QSet<QString> &readableExtensions()
{
    static const QSet<QString> exts = []() {
        QSet<QString> set;
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &f : formats)
            set.insert(QString::fromLatin1(f).toLower());
        return set;
    }();
    return exts;
}

inline bool isReadableExtension(const QString &suffix)
{
    return readableExtensions().contains(suffix.toLower());
}

inline bool isReadableFile(const QString &filePath)
{
    const int dot = filePath.lastIndexOf('.');
    if (dot < 0) return false;
    return isReadableExtension(filePath.mid(dot + 1));
}

// "*.png *.jpg ..." for QFileDialog filters, sorted for stable output.
inline QString dialogPatterns()
{
    QStringList exts = readableExtensions().values();
    exts.sort();
    QStringList patterns;
    patterns.reserve(exts.size());
    for (const QString &e : exts)
        patterns << (QStringLiteral("*.") + e);
    return patterns.join(QLatin1Char(' '));
}

} // namespace ImageFormats

#endif // IMAGEFORMATS_H
