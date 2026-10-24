#include "annotationitems.h"
#include "canvasnode.h"   // CanvasNode 锚点接口

#include <QPainter>
#include <QPainterPathStroker>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include "canvasscene.h"
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

// ========== LinkItem：曲线箭头参考链接 ==========

LinkItem::LinkItem(const QPointF &a, const QPointF &b,
                   QGraphicsObject *nodeA, QGraphicsObject *nodeB,
                   QGraphicsItem *parent)
    : QGraphicsItem(parent), m_freeA(a), m_freeB(b)
{
    m_a = nodeA;
    m_b = nodeB;
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setAcceptHoverEvents(true);
    refreshGeometry();
}

QPointF LinkItem::centerA() const
{
    if (m_a) {
        if (const CanvasNode *n = canvasNodeOf(m_a))
            return n->sceneNodeCenter();
        return m_a->sceneBoundingRect().center();
    }
    return m_freeA;
}

QPointF LinkItem::centerB() const
{
    if (m_b) {
        if (const CanvasNode *n = canvasNodeOf(m_b))
            return n->sceneNodeCenter();
        return m_b->sceneBoundingRect().center();
    }
    return m_freeB;
}

void LinkItem::setFreePoint(EndPoint e, const QPointF &p)
{
    if (e == EndA) { m_a = nullptr; m_freeA = p; }
    else           { m_b = nullptr; m_freeB = p; }
    refreshGeometry();
}

void LinkItem::setAnchor(EndPoint e, QGraphicsObject *node)
{
    if (e == EndA) m_a = node; else m_b = node;
    refreshGeometry();
}

LinkItem::Edge LinkItem::dominantEdge(const QPointF &from, const QPointF &to)
{
    const qreal dx = to.x() - from.x(), dy = to.y() - from.y();
    if (qAbs(dx) >= qAbs(dy))
        return dx >= 0 ? RightEdge : LeftEdge;
    return dy >= 0 ? BottomEdge : TopEdge;
}

QPointF LinkItem::edgeNormal(Edge e)
{
    switch (e) {
    case RightEdge:  return QPointF(1, 0);
    case LeftEdge:   return QPointF(-1, 0);
    case BottomEdge: return QPointF(0, 1);
    case TopEdge:    return QPointF(0, -1);
    }
    return QPointF();
}

LinkItem::Edge LinkItem::rayEdge(const QRectF &r, const QPointF &from, const QPointF &to,
                                 QPointF &hit)
{
    // 矩形射线：求中心→外部点首先命中的边（Liang-Barsky 简化）
    const qreal dx = to.x() - from.x(), dy = to.y() - from.y();
    qreal t = 1e30;
    Edge edge = LeftEdge;
    if (dx > 1e-9) {
        const qreal tt = (r.right() - from.x()) / dx;
        if (tt < t) { t = tt; edge = RightEdge; }
    }
    if (dx < -1e-9) {
        const qreal tt = (r.left() - from.x()) / dx;
        if (tt < t) { t = tt; edge = LeftEdge; }
    }
    if (dy > 1e-9) {
        const qreal tt = (r.bottom() - from.y()) / dy;
        if (tt < t) { t = tt; edge = BottomEdge; }
    }
    if (dy < -1e-9) {
        const qreal tt = (r.top() - from.y()) / dy;
        if (tt < t) { t = tt; edge = TopEdge; }
    }
    t = qBound(0.0, t, 1.0);
    hit = from + (to - from) * t;
    return edge;
}

QPainterPath LinkItem::makeConnector(const QPointF &a, Edge eA,
                                     const QPointF &b, Edge eB)
{
    // 三次贝塞尔：控制点沿各自边法线方向，长度夹取，得到流程图式平滑连线
    const QPointF nA = edgeNormal(eA);
    const QPointF nB = edgeNormal(eB);
    const qreal dist = QLineF(a, b).length();
    const qreal k = qBound(20.0, dist * 0.5, 90.0);
    QPainterPath p(a);
    p.cubicTo(a + nA * k, b + nB * k, b);
    return p;
}

void LinkItem::refreshGeometry()
{
    const QPointF cA = centerA();
    const QPointF cB = centerB();
    Edge eA, eB;

    if (m_a) {
        // 边缘点在节点自身坐标系中求（天然处理旋转/缩放）
        const CanvasNode *n = canvasNodeOf(m_a);
        const QRectF local = n ? n->localNodeRect()
                               : QRectF(0, 0, m_a->boundingRect().width(),
                                        m_a->boundingRect().height());
        QPointF hitL;
        eA = rayEdge(local, m_a->mapFromScene(cA), m_a->mapFromScene(cB), hitL);
        m_edgeA = m_a->mapToScene(hitL);
    } else {
        m_edgeA = m_freeA;
        eA = dominantEdge(cA, cB);
    }

    if (m_b) {
        const CanvasNode *n = canvasNodeOf(m_b);
        const QRectF local = n ? n->localNodeRect()
                               : QRectF(0, 0, m_b->boundingRect().width(),
                                        m_b->boundingRect().height());
        QPointF hitL;
        eB = rayEdge(local, m_b->mapFromScene(cB), m_b->mapFromScene(cA), hitL);
        m_edgeB = m_b->mapToScene(hitL);
    } else {
        m_edgeB = m_freeB;
        eB = dominantEdge(cB, cA);
    }

    // 自由点回退值：吸附期间记录边缘点，锚图任何方式删除后线停在最后边缘
    if (m_a) m_freeA = m_edgeA;
    if (m_b) m_freeB = m_edgeB;

    m_path = makeConnector(m_edgeA, eA, m_edgeB, eB);
    prepareGeometryChange();
    update();
}

QRectF LinkItem::boundingRect() const
{
    return m_path.boundingRect().adjusted(-(ARROW_LEN + 2), -(ARROW_LEN + 2),
                                          ARROW_LEN + 2, ARROW_LEN + 2);
}

QPainterPath LinkItem::shape() const
{
    // 细线拾取：点击只有曲线附近才命中，不遮挡其下的图片；箭头三角形也可点
    QPainterPathStroker stroker;
    stroker.setWidth(m_width + 8);
    return stroker.createStroke(m_path).united(arrowPath());
}

QPainterPath LinkItem::arrowPath() const
{
    // 箭头方向 = 曲线末端切线（angleAtPercent 与 QLineF 同约定：0=右，顺时针）
    const qreal angle = qDegreesToRadians(m_path.angleAtPercent(1.0));
    const QPointF dir(qCos(angle), qSin(angle));
    const QPointF perp(-dir.y(), dir.x());
    const QPointF base = m_edgeB - dir * ARROW_LEN;
    QPainterPath arrow(m_edgeB);
    arrow.lineTo(base + perp * ARROW_W);
    arrow.lineTo(base - perp * ARROW_W);
    arrow.closeSubpath();
    return arrow;
}

void LinkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_color, m_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter->strokePath(m_path, pen);

    painter->setPen(Qt::NoPen);
    painter->setBrush(m_color);
    painter->drawPath(arrowPath());

    if (isSelected()) {
        painter->drawEllipse(m_edgeA, ENDPOINT_R, ENDPOINT_R);
        painter->drawEllipse(m_edgeB, ENDPOINT_R, ENDPOINT_R);
    }
}

void LinkItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPointF sp = event->scenePos();
        const qreal dA = (sp - m_edgeA).manhattanLength();
        const qreal dB = (sp - m_edgeB).manhattanLength();
        if (dA <= ENDPOINT_R + 4 || dB <= ENDPOINT_R + 4) {
            // 点中端点：端点拖动（吸附端先解除为自由点，便于改绑）
            m_draggingEndpoint = true;
            m_dragEnd = (dA <= dB) ? EndA : EndB;
            if (isAnchored(m_dragEnd))
                setFreePoint(m_dragEnd, m_dragEnd == EndA ? m_edgeA : m_edgeB);
            setSelected(true);
            event->accept();
            return;
        }
    }
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
        // 松手处若命中画布节点（图片/卡片）→ 吸附
        if (scene()) {
            const QList<QGraphicsItem *> hits =
                scene()->items(event->scenePos(), Qt::IntersectsItemShape);
            for (QGraphicsItem *h : hits) {
                if (h == this)
                    continue;
                if (auto *obj = dynamic_cast<QGraphicsObject *>(h)) {
                    if (canvasNodeOf(obj)) {
                        setAnchor(m_dragEnd, obj);
                        break;
                    }
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
    if ((sp - m_edgeA).manhattanLength() <= ENDPOINT_R + 4 ||
        (sp - m_edgeB).manhattanLength() <= ENDPOINT_R + 4) {
        setCursor(Qt::CrossCursor);
    } else {
        setCursor(Qt::SizeAllCursor);
    }
    QGraphicsItem::hoverMoveEvent(event);
}

QVector<QPair<QString, qreal>> LinkItem::widthPresets()
{
    // 右键菜单与工具栏粗细选择共用同一组预设
    return {
        { QStringLiteral("细"),   1.5 },
        { QStringLiteral("标准"), 2.5 },
        { QStringLiteral("粗"),   4.0 },
        { QStringLiteral("特粗"), 6.0 },
    };
}

void LinkItem::applyWidth(LinkItem *anchor, qreal w)
{
    if (!anchor || w <= 0.0)
        return;
    // 目标：右键时若本线已在多选内，应用到全部选中链接；否则只应用本线
    QList<LinkItem *> targets;
    if (anchor->isSelected() && anchor->scene()) {
        const QList<QGraphicsItem *> selected = anchor->scene()->selectedItems();
        for (QGraphicsItem *it : selected) {
            if (auto *l = dynamic_cast<LinkItem *>(it))
                targets.append(l);
        }
    }
    if (targets.isEmpty())
        targets.append(anchor);
    // 同步场景默认粗细（之后新画的线也采用）
    if (auto *cs = dynamic_cast<CanvasScene *>(anchor->scene()))
        cs->setLinkWidth(w);
    for (LinkItem *l : targets)
        l->setWidth(w);
}

void LinkItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (!isSelected()) {
        if (scene())
            scene()->clearSelection();
        setSelected(true);
    }

    QList<LinkItem *> targets;
    const QList<QGraphicsItem *> selected = scene()->selectedItems();
    for (QGraphicsItem *it : selected) {
        if (auto *l = dynamic_cast<LinkItem *>(it))
            targets.append(l);
    }
    if (targets.isEmpty())
        targets.append(this);

    QMenu menu;
    QMenu *widthMenu = menu.addMenu(QStringLiteral("线条粗细"));
    for (const auto &preset : widthPresets()) {
        bool allMatch = true;
        for (LinkItem *l : targets) {
            if (qAbs(l->width() - preset.second) > 0.01) {
                allMatch = false;
                break;
            }
        }
        const qreal w = preset.second;
        // QGraphicsItem 不是 QObject，用 QMenu::addAction 的仿函数重载（内部自行连接）
        QAction *act = widthMenu->addAction(preset.first, [this, w]() { applyWidth(this, w); });
        act->setCheckable(true);
        act->setChecked(allMatch);
    }
    menu.exec(event->screenPos());
    event->accept();
}

QVariant LinkItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // 整线拖动：item 保持原点，位移施加到自由端点（吸附端由图片边缘决定）
        const QPointF delta = value.toPointF() - pos();
        if (!delta.isNull()) {
            if (!m_a) m_freeA += delta;
            if (!m_b) m_freeB += delta;
            refreshGeometry();
            return pos();
        }
    }
    if (change == ItemSelectedHasChanged)
        update();
    return QGraphicsItem::itemChange(change, value);
}
