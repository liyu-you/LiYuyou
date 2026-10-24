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

// 链接线：连接两个端点的曲线箭头（流程图/参考图谱风格）。
// 端点可吸附到图片（QPointer，图片删除自动置空）或为自由场景点；吸附时线端
// 落在朝对端的图片边缘，三次贝塞尔曲线连接，B 端带箭头（方向 A→B）。
// 几何模型：item 恒位于场景原点，端点存场景坐标，绘制直接使用场景坐标。
class LinkItem : public QGraphicsItem
{
public:
    enum EndPoint { EndA, EndB };
    enum Edge { LeftEdge, RightEdge, TopEdge, BottomEdge };

    LinkItem(const QPointF &a, const QPointF &b, ImageItem *imgA = nullptr,
             ImageItem *imgB = nullptr, QGraphicsItem *parent = nullptr);

    QPointF centerA() const;   // 吸附时=图片中心；自由时=自由点（用于射线方向）
    QPointF centerB() const;
    QPointF edgePointA() const { return m_edgeA; }   // 实际线端（图片边缘/自由点）
    QPointF edgePointB() const { return m_edgeB; }
    QPointF pointA() const { return edgePointA(); }   // 兼容旧名
    QPointF pointB() const { return edgePointB(); }

    void setFreePoint(EndPoint e, const QPointF &p);   // 解除吸附并设为自由点
    void setAnchor(EndPoint e, ImageItem *img);        // 吸附到图片

    ImageItem *anchorItem(EndPoint e) const { return (e == EndA ? m_a : m_b).data(); }
    bool isAnchored(EndPoint e) const { return (e == EndA ? m_a : m_b) != nullptr; }

    void setColor(const QColor &c) { m_color = c; update(); }
    void setWidth(qreal w) { prepareGeometryChange(); m_width = w; update(); }
    QColor color() const { return m_color; }
    qreal width() const { return m_width; }

    void refreshGeometry();   // 重算边缘点/贝塞尔（场景在图片移动时调用）

    QRectF boundingRect() const override;
    QPainterPath shape() const override;   // 细线形状，点击不挡图片
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // 可复用的连接曲线构造（CanvasView 预览也用）：
    static Edge dominantEdge(const QPointF &from, const QPointF &to);
    static QPointF edgeNormal(Edge e);
    static QPainterPath makeConnector(const QPointF &a, Edge eA,
                                      const QPointF &b, Edge eB);

    QPainterPath arrowPath() const;   // B 端箭头三角形（测试可检查）

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    // 从 from 到 to 的射线与矩形 r 的交点；返回命中边，hit=交点
    static Edge rayEdge(const QRectF &r, const QPointF &from, const QPointF &to, QPointF &hit);

    QPointer<ImageItem> m_a, m_b;      // 吸附目标（图片删除后自动置空）
    QPointF m_freeA, m_freeB;          // 自由端点/回退落点
    QPointF m_edgeA, m_edgeB;          // 实际线端
    QPainterPath m_path;               // 当前贝塞尔
    bool m_draggingEndpoint = false;
    EndPoint m_dragEnd = EndA;
    QColor m_color = QColor(170, 176, 182);
    qreal m_width = 2.5;
    static constexpr qreal ENDPOINT_R = 6.0;
    static constexpr qreal ARROW_LEN = 9.0;
    static constexpr qreal ARROW_W = 4.5;
};

#endif // ANNOTATIONITEMS_H
