#include "annotationitems.h"
#include "imageitem.h"   // dynamic_cast 到 ImageItem（端点吸附）

#include <QPainter>
#include <QPainterPathStroker>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <QtMath>

ArrowItem::ArrowItem(const QPointF &start, const QPointF &end, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_start(start), m_end(end)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
}

QRectF ArrowItem::boundingRect() const
{
    qreal extra = m_width + 10;
    return QRectF(m_start, m_end).normalized().adjusted(-extra, -extra, extra, extra);
}

void ArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    // Draw line
    painter->drawLine(m_start, m_end);

    // Draw arrow head
    qreal angle = qAtan2(m_end.y() - m_start.y(), m_end.x() - m_start.x());
    qreal arrowSize = m_width * 4 + 8;
    QPointF arrowP1 = m_end - QPointF(sin(angle - M_PI / 3) * arrowSize, cos(angle - M_PI / 3) * arrowSize);
    QPointF arrowP2 = m_end - QPointF(sin(angle + M_PI / 3) * arrowSize, cos(angle + M_PI / 3) * arrowSize);

    QPolygonF arrowHead;
    arrowHead << m_end << arrowP1 << arrowP2;
    painter->setBrush(m_color);
    painter->drawPolygon(arrowHead);

    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect());
    }
}

PenItem::PenItem(QGraphicsItem *parent)
    : QGraphicsPathItem(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setPen(QPen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

void PenItem::setColor(const QColor &c)
{
    m_color = c;
    QPen p = pen();
    p.setColor(c);
    setPen(p);
}

void PenItem::setWidth(qreal w)
{
    m_width = w;
    QPen p = pen();
    p.setWidthF(w);
    setPen(p);
}

void PenItem::addPoint(const QPointF &p)
{
    if (m_path.elementCount() == 0)
        m_path.moveTo(p);
    else
        m_path.lineTo(p);
    setPath(m_path);
}

// ========== LinkItem：参考链接线 ==========

LinkItem::LinkItem(const QPointF &a, const QPointF &b, ImageItem *imgA, ImageItem *imgB,
                   QGraphicsItem *parent)
    : QGraphicsItem(parent), m_freeA(a), m_freeB(b)
{
    m_a = imgA;
    m_b = imgB;
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setAcceptHoverEvents(true);
    refreshGeometry();
}

QPointF LinkItem::pointA() const
{
    return m_a ? m_a->sceneContentCenter() : m_freeA;
}

QPointF LinkItem::pointB() const
{
    return m_b ? m_b->sceneContentCenter() : m_freeB;
}

void LinkItem::setFreePoint(EndPoint e, const QPointF &p)
{
    if (e == EndA) { m_a = nullptr; m_freeA = p; }
    else           { m_b = nullptr; m_freeB = p; }
    refreshGeometry();
}

void LinkItem::setAnchor(EndPoint e, ImageItem *img)
{
    if (e == EndA) m_a = img; else m_b = img;
    refreshGeometry();
}

void LinkItem::refreshGeometry()
{
    // 同步自由点回退值：锚定期间始终记录最新端点，锚图任何方式被删除后
    // 端点回退到最后已知位置，而不是创建时的旧坐标。
    if (m_a) m_freeA = m_a->sceneContentCenter();
    if (m_b) m_freeB = m_b->sceneContentCenter();
    prepareGeometryChange();
    update();
}

QRectF LinkItem::boundingRect() const
{
    const qreal extra = m_width + ENDPOINT_R + 4;
    return QRectF(pointA(), pointB()).normalized().adjusted(-extra, -extra, extra, extra);
}

QPainterPath LinkItem::shape() const
{
    // 细线拾取形状：点击只有 2px 线附近才命中，不遮挡其下的图片
    QPainterPath path(pointA());
    path.lineTo(pointB());
    QPainterPathStroker stroker;
    stroker.setWidth(m_width + 6);
    return stroker.createStroke(path);
}

void LinkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    const QPointF a = pointA();
    const QPointF b = pointB();
    painter->drawLine(a, b);

    if (isSelected()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_color);
        painter->drawEllipse(a, ENDPOINT_R, ENDPOINT_R);
        painter->drawEllipse(b, ENDPOINT_R, ENDPOINT_R);
    }
}

void LinkItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF sp = event->scenePos();
        const qreal dA = (sp - pointA()).manhattanLength();
        const qreal dB = (sp - pointB()).manhattanLength();
        if (dA <= ENDPOINT_R + 4 || dB <= ENDPOINT_R + 4) {
            // 点中端点：进入端点拖动（吸附端先解除为自由点，便于改绑）
            m_draggingEndpoint = true;
            m_dragEnd = (dA <= dB) ? EndA : EndB;
            if (isAnchored(m_dragEnd))
                setFreePoint(m_dragEnd, point(m_dragEnd));
            setSelected(true);
            event->accept();
            return;
        }
    }
    m_draggingEndpoint = false;
    m_moved = false;
    m_dragStart = event->scenePos();
    m_dragFreeA = m_freeA;
    m_dragFreeB = m_freeB;
    QGraphicsItem::mousePressEvent(event);   // ItemIsMovable：整线拖动
}

void LinkItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_draggingEndpoint) {
        setFreePoint(m_dragEnd, event->scenePos());
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void LinkItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_draggingEndpoint) {
        m_draggingEndpoint = false;
        // 松手处若命中图片 → 吸附到该图中心
        if (scene()) {
            const QList<QGraphicsItem *> hits =
                scene()->items(event->scenePos(), Qt::IntersectsItemShape);
            for (QGraphicsItem *h : hits) {
                if (h == this)
                    continue;
                if (auto *img = dynamic_cast<ImageItem *>(h)) {
                    setAnchor(m_dragEnd, img);
                    break;
                }
            }
        }
        event->accept();
        return;
    }
    QGraphicsItem::mouseReleaseEvent(event);
}

void LinkItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const QPointF sp = event->scenePos();
    if ((sp - pointA()).manhattanLength() <= ENDPOINT_R + 4 ||
        (sp - pointB()).manhattanLength() <= ENDPOINT_R + 4) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
    QGraphicsItem::hoverMoveEvent(event);
}

QVariant LinkItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // 整线拖动：item 保持原点，把位移施加到自由端点（吸附端由图片中心决定）
        const QPointF delta = value.toPointF() - pos();
        if (!delta.isNull()) {
            m_freeA += delta;
            m_freeB += delta;
            prepareGeometryChange();
            return pos();   // 拒绝实际位移，位移由端点体现
        }
    }
    if (change == ItemSelectedHasChanged)
        update();
    return QGraphicsItem::itemChange(change, value);
}
