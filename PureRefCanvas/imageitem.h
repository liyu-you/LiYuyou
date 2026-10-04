#ifndef IMAGEITEM_H
#define IMAGEITEM_H

#include <QGraphicsObject>
#include <QGraphicsLineItem>
#include <QPixmap>
#include <QTimer>

class ImageItem : public QGraphicsObject
{
    Q_OBJECT
public:
    enum Handle { NoHandle, TopLeft, TopRight, BottomLeft, BottomRight, RotateHandle };

    explicit ImageItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const { return m_pixmap; }
    QSizeF imageSize() const { return m_size; }

    void bringToFront();
    void sendToBack();

    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Handle handleAt(const QPointF &pos) const;
    QRectF handleRect(Handle handle) const;
    void updateCursorForHandle(Handle handle);
    qreal angleToPoint(const QPointF &scenePos) const;
    void disableCacheDuringDrag();

    QPixmap m_pixmap;
    QSizeF m_size;
    Handle m_activeHandle = NoHandle;
    QPointF m_pressScenePos;
    QPointF m_pressItemPos;
    QSizeF m_pressSize;
    qreal m_pressRotation = 0.0;
    bool m_moved = false;
    bool m_locked = false;
    QTimer *m_cacheTimer = nullptr;

    static constexpr qreal HANDLE = 12.0;
    static constexpr qreal ROTATE_DIST = 28.0;
    static constexpr qreal SNAP_THRESHOLD = 8.0;

    // Smart snap alignment guides (shared across all items)
    static QGraphicsLineItem *s_hGuide;
    static QGraphicsLineItem *s_vGuide;
    static void ensureGuides(QGraphicsScene *scene);
    static void showGuideH(QGraphicsScene *scene, qreal y, qreal x1, qreal x2);
    static void showGuideV(QGraphicsScene *scene, qreal x, qreal y1, qreal y2);
    static void hideGuides();
};

#endif // IMAGEITEM_H
