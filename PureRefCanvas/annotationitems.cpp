#include "annotationitems.h"
#include <QPainter>
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
