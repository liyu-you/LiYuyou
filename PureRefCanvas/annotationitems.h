#ifndef ANNOTATIONITEMS_H
#define ANNOTATIONITEMS_H

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QColor>
#include <QPointer>

class ImageItem;

class ArrowItem : public QGraphicsItem
{
public:
    ArrowItem(const QPointF &start = QPointF(), const QPointF &end = QPointF(), QGraphicsItem *parent = nullptr);

    void setStart(const QPointF &p) { prepareGeometryChange(); m_start = p; }
    void setEnd(const QPointF &p) { prepareGeometryChange(); m_end = p; }
    void setColor(const QColor &c) { m_color = c; update(); }
    void setWidth(qreal w) { prepareGeometryChange(); m_width = w; }
    QPointF start() const { return m_start; }
    QPointF end() const { return m_end; }
    QColor color() const { return m_color; }
    qreal width() const { return m_width; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QPointF m_start, m_end;
    QColor m_color = QColor(255, 0, 0);
    qreal m_width = 2.0;
};

class PenItem : public QGraphicsPathItem
{
public:
    PenItem(QGraphicsItem *parent = nullptr);
    void setColor(const QColor &c);
    void setWidth(qreal w);
    void addPoint(const QPointF &p);
    QColor color() const { return m_color; }
    qreal width() const { return m_width; }

private:
    QColor m_color = QColor(255, 0, 0);
    qreal m_width = 2.0;
    QPainterPath m_path;
};

// 链接线：连接两个端点的直线（PureRef 风格参考关联）。
// 端点可吸附到图片（QPointer，图片删除自动置空）或为自由场景点；
// 选中后可整线拖动、Del 删除；端点可拖动重绑（拖到图片上即吸附）。
// 几何模型：item 恒位于场景原点，端点存场景坐标，绘制直接使用场景坐标。
class LinkItem : public QGraphicsItem
{
public:
    enum EndPoint { EndA, EndB };

    LinkItem(const QPointF &a, const QPointF &b, ImageItem *imgA = nullptr,
             ImageItem *imgB = nullptr, QGraphicsItem *parent = nullptr);

    QPointF pointA() const;   // 吸附时 = 图片中心；否则 = 自由点
    QPointF pointB() const;
    QPointF point(EndPoint e) const { return e == EndA ? pointA() : pointB(); }

    void setFreePoint(EndPoint e, const QPointF &p);   // 解除吸附并设为自由点
    void setAnchor(EndPoint e, ImageItem *img);        // 吸附到图片中心

    ImageItem *anchorItem(EndPoint e) const { return (e == EndA ? m_a : m_b).data(); }
    bool isAnchored(EndPoint e) const { return (e == EndA ? m_a : m_b) != nullptr; }

    void setColor(const QColor &c) { m_color = c; update(); }
    void setWidth(qreal w) { prepareGeometryChange(); m_width = w; update(); }
    QColor color() const { return m_color; }
    qreal width() const { return m_width; }

    void refreshGeometry();   // 锚点/自由点变化后重算（场景在图片移动时调用）

    QRectF boundingRect() const override;
    QPainterPath shape() const override;   // 细线形状，点击不挡图片
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    QPointer<ImageItem> m_a, m_b;      // 吸附目标（图片删除后自动置空）
    QPointF m_freeA, m_freeB;          // 自由端点场景坐标
    QPointF m_dragStart;               // 整线拖动起点
    QPointF m_dragFreeA, m_dragFreeB;  // 整线拖动开始时自由端点快照
    bool m_draggingEndpoint = false;
    EndPoint m_dragEnd = EndA;
    bool m_moved = false;
    QColor m_color = QColor(255, 255, 255);
    qreal m_width = 2.0;
    static constexpr qreal ENDPOINT_R = 6.0;
};

#endif // ANNOTATIONITEMS_H
