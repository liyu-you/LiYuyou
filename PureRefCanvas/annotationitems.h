#ifndef ANNOTATIONITEMS_H
#define ANNOTATIONITEMS_H

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QColor>

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

#endif
