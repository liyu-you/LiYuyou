#ifndef CANVASSCENE_H
#define CANVASSCENE_H

#include <QGraphicsScene>
#include <QList>
#include <QColor>
#include <QHash>

class QPixmap;
class ImageItem;
class ArrowItem;
class PenItem;
class LinkItem;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT
public:
    enum Tool { SelectTool, ArrowTool, TextTool, RectTool, PenTool, LinkTool };

    explicit CanvasScene(QObject *parent = nullptr);

    ImageItem *addImage(const QPixmap &pixmap, const QPointF &pos = QPointF());
    void addImageAtCenter(const QPixmap &pixmap);
    void addImages(const QList<QPixmap> &pixmaps);

    void removeSelectedItems();
    void selectAllItems();
    void clearAll();
    void clearAllImages();

    QList<ImageItem*> imageItems() const;
    QList<ImageItem*> selectedImageItems() const;

    void copySelected();
    void paste();
    void duplicateSelected();

    // Serialization
    QByteArray saveState() const;
    bool loadState(const QByteArray &data);

    void bringSelectedToFront();
    void sendSelectedToBack();

    // Annotation tools
    void setCurrentTool(Tool tool) { m_currentTool = tool; }
    Tool currentTool() const { return m_currentTool; }
    void setAnnotationColor(const QColor &c) { m_annoColor = c; }
    QColor annotationColor() const { return m_annoColor; }
    void setAnnotationWidth(qreal w) { m_annoWidth = w; }
    qreal annotationWidth() const { return m_annoWidth; }

    // Allow the minimap to render items without the scene's own dark
    // background/grid, so the canvas background image shows through.
    void setBackgroundSuppressed(bool on) { m_backgroundSuppressed = on; }
    bool backgroundSuppressed() const { return m_backgroundSuppressed; }

    ArrowItem *createArrow(const QPointF &start, const QPointF &end);
    PenItem *createPen();
    void addTextAnnotation(const QPointF &pos);
    void addRectAnnotation(const QPointF &start, const QPointF &end);

    // 链接线：连接两张图（或自由点）的参考线。imgA/imgB 为空表示对应端为自由点 pt。
    LinkItem *createLink(ImageItem *imgA, const QPointF &ptA,
                         ImageItem *imgB, const QPointF &ptB);
    QList<LinkItem *> links() const { return m_links; }

    // 链接线默认粗细（工具栏粗细选择器写入；新链接采用）
    void setLinkWidth(qreal w) { m_linkWidth = w; }
    qreal linkWidth() const { return m_linkWidth; }

    // 由 ImageItem::itemChange 在图片位置变化后调用，刷新吸附到该图的链接
    void refreshLinksFor(QGraphicsItem *moved);

signals:
    void itemCountChanged(int count);
    void selectionChangedInfo(int count);
    void toolChanged(Tool tool);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QList<QPixmap> m_clipboard;
    QList<LinkItem *> m_links;   // 当前画布上的全部链接线（随图片移动同步）
    qreal m_nextZ = 0;
    Tool m_currentTool = SelectTool;
    QColor m_annoColor = QColor(255, 50, 50);
    qreal m_annoWidth = 2.0;
    qreal m_linkWidth = 2.5;   // 链接线默认粗细（1.5 太细，2.5 为标准）
    bool m_backgroundSuppressed = false;

    // Encoded PNG bytes per QPixmap::cacheKey(), so saving a canvas does not
    // re-encode images that did not change (see saveState()). Rebuilt on every
    // save, which also drops entries for images that were deleted.
    mutable QHash<qint64, QByteArray> m_pngCache;
    // Bound on that cache. A canvas whose PNG total outgrows it keeps nothing,
    // which costs the old re-encode-everything behaviour but never unbounded RAM.
    static constexpr qint64 PNG_CACHE_BUDGET = 128ll * 1024 * 1024;
};

#endif // CANVASSCENE_H
