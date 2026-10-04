#ifndef IMAGEIO_H
#define IMAGEIO_H

// Image decoding that is safe away from the GUI thread.
//
// QImageReader is instance-local and QImage may cross threads, unlike QPixmap
// which must be created on the GUI thread. Used by the import path so dropping
// a batch of photos decodes in a worker instead of freezing the window (measured
// 145 ms per 12 MP photo on the UI thread).
//
// Deliberately reads at native resolution: the canvas displays images at any
// zoom level, so the full pixel data is required. No auto-orientation: the
// previous code (QPixmap(path)) did not apply EXIF orientation either, and this
// must not change how already-placed images look.

#include <QImage>
#include <QImageReader>
#include <QString>

namespace ImageIO {

inline QImage decodeFull(const QString &path)
{
    QImageReader reader(path);
    return reader.read();
}

} // namespace ImageIO

#endif // IMAGEIO_H
