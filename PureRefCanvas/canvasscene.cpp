#include "canvasscene.h"
#include "imageitem.h"
#include "annotationitems.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QBuffer>
#include <QDataStream>
#include <QVector>
#include <QPair>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QGraphicsTextItem>
#include <QGraphicsRectItem>
#include <QFont>

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-100000, -100000, 200000, 200000);
    setStickyFocus(true);
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        emit selectionChangedInfo(selectedImageItems().size());
    });
}

ImageItem *CanvasScene::addImage(const QPixmap &pixmap, const QPointF &pos)
{
    if (pixmap.isNull()) return nullptr;
    auto *item = new ImageItem(pixmap);
    item->setPos(pos);
    item->setZValue(m_nextZ++);
    addItem(item);
    emit itemCountChanged(imageItems().size());
    return item;
}

void CanvasScene::addImageAtCenter(const QPixmap &pixmap)
{
    // Place at origin with slight offset based on count
    int count = imageItems().size();
    QPointF pos(count % 10 * 30, (count / 10) * 30);
    addImage(pixmap, pos);
}

void CanvasScene::addImages(const QList<QPixmap> &pixmaps)
{
    for (const auto &pm : pixmaps) {
        addImageAtCenter(pm);
    }
}

void CanvasScene::removeSelectedItems()
{
    auto items = selectedItems();
    for (auto *it : items) {
        removeItem(it);
        delete it;
    }
    emit itemCountChanged(imageItems().size());
}

void CanvasScene::selectAllItems()
{
    for (auto *it : imageItems()) {
        it->setSelected(true);
    }
}

void CanvasScene::clearAll()
{
    QGraphicsScene::clear();
    m_nextZ = 0;
    m_pngCache.clear(); // encoded bytes for images that no longer exist
    emit itemCountChanged(0);
}

void CanvasScene::clearAllImages()
{
    const auto imgs = imageItems();
    for (auto *img : imgs) {
        removeItem(img);
        delete img;
    }
    emit itemCountChanged(imageItems().size());
}

QList<ImageItem*> CanvasScene::imageItems() const
{
    QList<ImageItem*> result;
    for (auto *it : items()) {
        if (auto *img = dynamic_cast<ImageItem*>(it))
            result.append(img);
    }
    return result;
}

QList<ImageItem*> CanvasScene::selectedImageItems() const
{
    QList<ImageItem*> result;
    for (auto *it : selectedItems()) {
        if (auto *img = dynamic_cast<ImageItem*>(it))
            result.append(img);
    }
    return result;
}

void CanvasScene::copySelected()
{
    m_clipboard.clear();
    for (auto *item : selectedImageItems()) {
        m_clipboard.append(item->pixmap());
    }
    // Also copy to system clipboard as image
    if (!m_clipboard.isEmpty()) {
        QApplication::clipboard()->setPixmap(m_clipboard.first());
    }
}

void CanvasScene::paste()
{
    // Try system clipboard first
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (mime && mime->hasImage()) {
        QPixmap pm = QApplication::clipboard()->pixmap();
        if (!pm.isNull()) {
            addImageAtCenter(pm);
            return;
        }
    }
    // Fall back to internal clipboard
    for (const auto &pm : m_clipboard) {
        addImageAtCenter(pm);
    }
}

void CanvasScene::duplicateSelected()
{
    auto items = selectedImageItems();
    for (auto *item : items) {
        auto *newItem = addImage(item->pixmap(), item->pos() + QPointF(20, 20));
        newItem->setRotation(item->rotation());
        newItem->setScale(item->scale());
        newItem->setSelected(true);
        item->setSelected(false);
    }
}

void CanvasScene::bringSelectedToFront()
{
    for (auto *item : selectedImageItems())
        item->bringToFront();
}

void CanvasScene::sendSelectedToBack()
{
    for (auto *item : selectedImageItems())
        item->sendToBack();
}

QByteArray CanvasScene::saveState() const
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);
    out.setVersion(QDataStream::Qt_6_0);

    const auto imgs = imageItems();
    out << (quint32)imgs.size();

    // Re-encoding every image made each save a full PNG pass over the canvas
    // (measured: 1.25 s for 5 x 12 MP photos, 4.7 s for 20, 12 s for 50 - and
    // auto-save does that every few minutes). Encoded bytes are now cached per
    // QPixmap::cacheKey(), so moving/rotating images or saving twice reuses what
    // was already produced; only new or replaced images pay the encode again.
    // The cache is rebuilt in item order each save, which drops entries for
    // deleted images and keeps a stable subset if the total outgrows the budget
    // (the oldest items are evicted first, so the retained set is deterministic
    // and keeps getting hit instead of thrashing).
    QVector<QPair<qint64, QByteArray>> entries;
    entries.reserve(imgs.size());
    qint64 total = 0;
    for (auto *item : imgs) {
        const QPixmap pm = item->pixmap();
        const qint64 key = pm.cacheKey();
        QByteArray pngData = m_pngCache.value(key);
        if (pngData.isEmpty()) {
            QBuffer pngBuf(&pngData);
            pngBuf.open(QIODevice::WriteOnly);
            pm.save(&pngBuf, "PNG");
        }
        total += pngData.size();
        entries.append(qMakePair(key, pngData));
        out << pngData;
        out << item->pos().x() << item->pos().y();
        out << item->rotation();
        out << item->scale();
        out << item->zValue();
        out << item->transform();
    }

    QHash<qint64, QByteArray> next;
    for (const auto &e : entries) {
        if (total > PNG_CACHE_BUDGET) {
            total -= e.second.size(); // evict oldest first
            continue;
        }
        next.insert(e.first, e.second);
    }
    m_pngCache = next;
    return data;
}

bool CanvasScene::loadState(const QByteArray &data)
{
    QBuffer buffer(const_cast<QByteArray*>(&data));
    buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 count;
    in >> count;
    if (in.status() != QDataStream::Ok) return false;

    clearAll();
    qint64 cachedBytes = 0;
    for (quint32 i = 0; i < count; ++i) {
        QByteArray pngData;
        in >> pngData;
        QPixmap pm;
        pm.loadFromData(pngData, "PNG");
        // Seed the encode cache with the bytes we just read: opening a canvas and
        // letting auto-save run must not re-encode every image from pixels.
        if (!pm.isNull() && cachedBytes + pngData.size() <= PNG_CACHE_BUDGET) {
            m_pngCache.insert(pm.cacheKey(), pngData);
            cachedBytes += pngData.size();
        }
        qreal x, y, rot, scale, z;
        QTransform transform;
        in >> x >> y >> rot >> scale >> z >> transform;
        auto *item = addImage(pm, QPointF(x, y));
        if (item) {
            item->setRotation(rot);
            item->setScale(scale);
            item->setZValue(z);
            item->setTransform(transform);
        }
    }
    return true;
}

void CanvasScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    // When rendering for the minimap we skip the scene's own background so
    // the canvas background image (drawn by the minimap first) stays visible.
    if (m_backgroundSuppressed) return;

    // Steam dark blue background (consistent with other workspaces)
    painter->fillRect(rect, QColor(27, 40, 56));

    // Subtle grid
    const qreal gridSize = 50;
    painter->setPen(QPen(QColor(42, 58, 74), 1));
    painter->setRenderHint(QPainter::Antialiasing, false);

    qreal left = qFloor(rect.left() / gridSize) * gridSize;
    qreal top = qFloor(rect.top() / gridSize) * gridSize;

    QVarLengthArray<QLineF, 100> lines;
    for (qreal x = left; x < rect.right(); x += gridSize)
        lines.append(QLineF(x, rect.top(), x, rect.bottom()));
    for (qreal y = top; y < rect.bottom(); y += gridSize)
        lines.append(QLineF(rect.left(), y, rect.right(), y));
    painter->drawLines(lines.data(), lines.size());
}

void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Delete)) {
        removeSelectedItems();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllItems();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copySelected();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        paste();
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_D) {
        duplicateSelected();
        event->accept();
        return;
    }
    QGraphicsScene::keyPressEvent(event);
}

// ========== Annotation Tools ==========

ArrowItem *CanvasScene::createArrow(const QPointF &start, const QPointF &end)
{
    ArrowItem *arrow = new ArrowItem(start, end);
    arrow->setColor(m_annoColor);
    arrow->setWidth(m_annoWidth);
    arrow->setZValue(++m_nextZ);
    addItem(arrow);
    emit itemCountChanged(imageItems().size());
    return arrow;
}

PenItem *CanvasScene::createPen()
{
    PenItem *pen = new PenItem();
    pen->setColor(m_annoColor);
    pen->setWidth(m_annoWidth);
    pen->setZValue(++m_nextZ);
    addItem(pen);
    emit itemCountChanged(imageItems().size());
    return pen;
}

void CanvasScene::addTextAnnotation(const QPointF &pos)
{
    QGraphicsTextItem *text = addText("双击编辑文字", QFont("Microsoft YaHei", 14));
    text->setDefaultTextColor(m_annoColor);
    text->setPos(pos);
    text->setZValue(++m_nextZ);
    text->setFlag(QGraphicsItem::ItemIsSelectable, true);
    text->setFlag(QGraphicsItem::ItemIsMovable, true);
    text->setTextInteractionFlags(Qt::TextEditorInteraction);
    emit itemCountChanged(imageItems().size());
}

void CanvasScene::addRectAnnotation(const QPointF &start, const QPointF &end)
{
    QRectF rect(start, end);
    QGraphicsRectItem *r = addRect(rect.normalized(), QPen(m_annoColor, m_annoWidth, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    r->setZValue(++m_nextZ);
    r->setFlag(QGraphicsItem::ItemIsSelectable, true);
    r->setFlag(QGraphicsItem::ItemIsMovable, true);
    emit itemCountChanged(imageItems().size());
}
