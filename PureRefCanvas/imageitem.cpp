#include "imageitem.h"
#include "canvasscene.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QCursor>
#include <QMenu>
#include <QMessageBox>
#include <QKeyEvent>
#include <QtMath>

// Static guide line members
QGraphicsLineItem *ImageItem::s_hGuide = nullptr;
QGraphicsLineItem *ImageItem::s_vGuide = nullptr;

void ImageItem::ensureGuides(QGraphicsScene *scene)
{
    if (!scene) return;
    if (!s_hGuide) {
        s_hGuide = new QGraphicsLineItem();
        s_hGuide->setPen(QPen(QColor(102, 192, 244, 200), 1, Qt::DashLine));
        s_hGuide->setZValue(9999);
        scene->addItem(s_hGuide);
    }
    if (!s_vGuide) {
        s_vGuide = new QGraphicsLineItem();
        s_vGuide->setPen(QPen(QColor(102, 192, 244, 200), 1, Qt::DashLine));
        s_vGuide->setZValue(9999);
        scene->addItem(s_vGuide);
    }
}

void ImageItem::showGuideH(QGraphicsScene *scene, qreal y, qreal x1, qreal x2)
{
    ensureGuides(scene);
    if (s_hGuide) { s_hGuide->setLine(x1, y, x2, y); s_hGuide->setVisible(true); }
}

void ImageItem::showGuideV(QGraphicsScene *scene, qreal x, qreal y1, qreal y2)
{
    ensureGuides(scene);
    if (s_vGuide) { s_vGuide->setLine(x, y1, x, y2); s_vGuide->setVisible(true); }
}

void ImageItem::hideGuides()
{
    if (s_hGuide) s_hGuide->setVisible(false);
    if (s_vGuide) s_vGuide->setVisible(false);
}

ImageItem::ImageItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_pixmap(pixmap)
{
    m_size = pixmap.size();
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setAcceptHoverEvents(true);

    // Smart cache: disable during drag, restore 150ms after release
    m_cacheTimer = new QTimer(this);
    m_cacheTimer->setSingleShot(true);
    m_cacheTimer->setInterval(150);
    connect(m_cacheTimer, &QTimer::timeout, this, [this]() {
        if (cacheMode() != QGraphicsItem::DeviceCoordinateCache)
            setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    });
}

void ImageItem::disableCacheDuringDrag()
{
    if (cacheMode() != QGraphicsItem::NoCache)
        setCacheMode(QGraphicsItem::NoCache);
    m_cacheTimer->start(); // restart: will restore 150ms after last call
}

void ImageItem::setPixmap(const QPixmap &pixmap)
{
    prepareGeometryChange();
    m_pixmap = pixmap;
    m_size = pixmap.size();
    update();
}

QPointF ImageItem::sceneContentCenter() const
{
    return mapToScene(QPointF(m_size.width() / 2.0, m_size.height() / 2.0));
}

QRectF ImageItem::boundingRect() const
{
    qreal pad = HANDLE + 4;
    return QRectF(-pad, -pad - ROTATE_DIST,
                  m_size.width() + pad * 2,
                  m_size.height() + pad * 2 + ROTATE_DIST);
}

QRectF ImageItem::handleRect(Handle h) const
{
    qreal s = HANDLE;
    switch (h) {
    case TopLeft:     return QRectF(-s/2, -s/2, s, s);
    case TopRight:    return QRectF(m_size.width() - s/2, -s/2, s, s);
    case BottomLeft:  return QRectF(-s/2, m_size.height() - s/2, s, s);
    case BottomRight: return QRectF(m_size.width() - s/2, m_size.height() - s/2, s, s);
    case RotateHandle: {
        qreal cx = m_size.width() / 2.0;
        return QRectF(cx - s/2, -ROTATE_DIST - s/2, s, s);
    }
    default: return QRectF();
    }
}

ImageItem::Handle ImageItem::handleAt(const QPointF &pos) const
{
    if (!isSelected()) return NoHandle;
    const Handle handles[] = { TopLeft, TopRight, BottomLeft, BottomRight, RotateHandle };
    for (Handle h : handles) {
        if (handleRect(h).contains(pos)) return h;
    }
    return NoHandle;
}

void ImageItem::updateCursorForHandle(Handle h)
{
    switch (h) {
    case TopLeft: case BottomRight: setCursor(Qt::SizeFDiagCursor); break;
    case TopRight: case BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
    case RotateHandle: setCursor(Qt::PointingHandCursor); break;
    default: setCursor(Qt::SizeAllCursor); break;
    }
}

qreal ImageItem::angleToPoint(const QPointF &scenePos) const
{
    QPointF center = mapToScene(m_size.width() / 2.0, m_size.height() / 2.0);
    QPointF diff = scenePos - center;
    return qRadiansToDegrees(qAtan2(diff.y(), diff.x()));
}

void ImageItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_locked) {
        // Locked items can still be selected but not moved/resized
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_activeHandle = handleAt(event->pos());
        m_pressScenePos = event->scenePos();
        m_pressItemPos = event->pos();
        m_pressSize = m_size;
        m_pressRotation = rotation();
        m_moved = false;

        if (m_activeHandle != NoHandle) {
            event->accept();
            return;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void ImageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Disable cache during any drag operation (move/resize/rotate)
    if (event->buttons() & Qt::LeftButton)
        disableCacheDuringDrag();

    if (m_activeHandle != NoHandle && (event->buttons() & Qt::LeftButton)) {
        m_moved = true;

        if (m_activeHandle == RotateHandle) {
            qreal angle = angleToPoint(event->scenePos());
            // The rotate handle is at top (angle -90 from center in item space)
            // We want the handle to follow the mouse
            qreal delta = angle - angleToPoint(m_pressScenePos);
            setRotation(m_pressRotation + delta);
            event->accept();
            return;
        }

        // Resize
        // Map mouse delta to item coordinates (accounting for rotation).
        QPointF itemDelta = mapFromScene(event->scenePos()) - mapFromScene(m_pressScenePos);

        qreal newW = m_pressSize.width();
        qreal newH = m_pressSize.height();

        bool keepRatio = !(event->modifiers() & Qt::ShiftModifier);
        qreal ratio = m_pixmap.width() > 0 ? (qreal)m_pixmap.height() / m_pixmap.width() : 1.0;

        switch (m_activeHandle) {
        case TopLeft: {
            newW = m_pressSize.width() - itemDelta.x();
            newH = m_pressSize.height() - itemDelta.y();
            if (keepRatio) {
                if (newW / newH > m_pixmap.width() / (qreal)m_pixmap.height())
                    newW = newH / ratio;
                else
                    newH = newW * ratio;
            }
            // Adjust position so bottom-right stays fixed
            QPointF brScene = mapToScene(m_pressSize.width(), m_pressSize.height());
            prepareGeometryChange();
            m_size = QSizeF(qMax(10.0, newW), qMax(10.0, newH));
            QPointF newBr = mapToScene(m_size.width(), m_size.height());
            moveBy(brScene.x() - newBr.x(), brScene.y() - newBr.y());
            break;
        }
        case TopRight: {
            newW = m_pressSize.width() + itemDelta.x();
            newH = m_pressSize.height() - itemDelta.y();
            if (keepRatio) {
                if (newW / newH > m_pixmap.width() / (qreal)m_pixmap.height())
                    newW = newH / ratio;
                else
                    newH = newW * ratio;
            }
            QPointF blScene = mapToScene(0, m_pressSize.height());
            prepareGeometryChange();
            m_size = QSizeF(qMax(10.0, newW), qMax(10.0, newH));
            QPointF newBl = mapToScene(0, m_size.height());
            moveBy(blScene.x() - newBl.x(), blScene.y() - newBl.y());
            break;
        }
        case BottomLeft: {
            newW = m_pressSize.width() - itemDelta.x();
            newH = m_pressSize.height() + itemDelta.y();
            if (keepRatio) {
                if (newW / newH > m_pixmap.width() / (qreal)m_pixmap.height())
                    newW = newH / ratio;
                else
                    newH = newW * ratio;
            }
            QPointF trScene = mapToScene(m_pressSize.width(), 0);
            prepareGeometryChange();
            m_size = QSizeF(qMax(10.0, newW), qMax(10.0, newH));
            QPointF newTr = mapToScene(m_size.width(), 0);
            moveBy(trScene.x() - newTr.x(), trScene.y() - newTr.y());
            break;
        }
        case BottomRight: {
            newW = m_pressSize.width() + itemDelta.x();
            newH = m_pressSize.height() + itemDelta.y();
            if (keepRatio) {
                if (newW / newH > m_pixmap.width() / (qreal)m_pixmap.height())
                    newW = newH / ratio;
                else
                    newH = newW * ratio;
            }
            prepareGeometryChange();
            m_size = QSizeF(qMax(10.0, newW), qMax(10.0, newH));
            break;
        }
        default: break;
        }

        update();
        event->accept();
        return;
    }

    m_moved = true;
    QGraphicsObject::mouseMoveEvent(event);
}

void ImageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    hideGuides();
    if (m_activeHandle != NoHandle) {
        m_activeHandle = NoHandle;
        updateCursorForHandle(NoHandle);
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void ImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    Handle h = handleAt(event->pos());
    if (h != NoHandle) {
        updateCursorForHandle(h);
    } else {
        setCursor(isSelected() ? Qt::SizeAllCursor : Qt::ArrowCursor);
    }
    QGraphicsObject::hoverMoveEvent(event);
}

void ImageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void ImageItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    setSelected(true);
    QMenu menu;
    menu.addAction("置于顶层", this, &ImageItem::bringToFront);
    menu.addAction("置于底层", this, &ImageItem::sendToBack);
    menu.addSeparator();
    QAction *lockAct = menu.addAction(m_locked ? "解锁 (Ctrl+L)" : "锁定 (Ctrl+L)");
    menu.addSeparator();
    QAction *resetAct = menu.addAction("重置变换");
    QAction *flipH = menu.addAction("水平翻转");
    QAction *flipV = menu.addAction("垂直翻转");
    menu.addSeparator();
    QAction *delAct = menu.addAction("删除");
    QAction *clearAllAct = menu.addAction("清除全部图片");

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == lockAct) {
        setLocked(!m_locked);
    } else if (chosen == resetAct) {
        setRotation(0);
        prepareGeometryChange();
        m_size = m_pixmap.size();
        update();
    } else if (chosen == flipH) {
        setTransform(QTransform::fromScale(-1, 1), true);
    } else if (chosen == flipV) {
        setTransform(QTransform::fromScale(1, -1), true);
    } else if (chosen == delAct) {
        if (auto *sc = dynamic_cast<CanvasScene*>(scene()))
            sc->removeSelectedItems();
    } else if (chosen == clearAllAct) {
        if (auto *sc = dynamic_cast<CanvasScene*>(scene())) {
            int count = sc->imageItems().size();
            if (count > 0 && QMessageBox::question(nullptr, "清除全部图片",
                QString("确定要清除画布上全部 %1 张图片吗？此操作不可撤销。").arg(count)) == QMessageBox::Yes) {
                sc->clearAllImages();
            }
        }
    }
}

void ImageItem::bringToFront()
{
    auto items = scene()->items();
    qreal maxZ = 0;
    for (auto *it : items) {
        if (it != this) maxZ = qMax(maxZ, it->zValue());
    }
    setZValue(maxZ + 1);
}

void ImageItem::sendToBack()
{
    auto items = scene()->items();
    qreal minZ = 0;
    for (auto *it : items) {
        if (it != this) minZ = qMin(minZ, it->zValue());
    }
    setZValue(minZ - 1);
}

void ImageItem::setLocked(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setAcceptHoverEvents(!locked);
    update();
}

QVariant ImageItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedHasChanged) {
        update();
    } else if (change == ItemPositionChange) {
        disableCacheDuringDrag();

        // Smart snap alignment (only during user drag, not programmatic)
        if (scene() && m_activeHandle == NoHandle && !m_locked) {
            QPointF newPos = value.toPointF();
            QRectF myRect = QRectF(newPos, m_size);
            if (rotation() != 0) {
                QPointF c = myRect.center();
                qreal rad = qDegreesToRadians(rotation());
                qreal cosA = qAbs(qCos(rad)), sinA = qAbs(qSin(rad));
                qreal w = m_size.width() * cosA + m_size.height() * sinA;
                qreal h = m_size.width() * sinA + m_size.height() * cosA;
                myRect = QRectF(c.x() - w/2, c.y() - h/2, w, h);
            }

            qreal snapX = newPos.x(), snapY = newPos.y();
            bool snappedX = false, snappedY = false;
            qreal guideX = 0, guideY = 0;
            qreal minX1 = myRect.left(), maxX2 = myRect.right();
            qreal minY1 = myRect.top(), maxY2 = myRect.bottom();

            QList<QGraphicsItem*> items = scene()->items();
            for (QGraphicsItem *other : items) {
                if (other == this) continue;
                ImageItem *otherImg = dynamic_cast<ImageItem*>(other);
                if (!otherImg || otherImg->m_locked) continue;

                QRectF oRect = otherImg->sceneBoundingRect();

                if (!snappedX) {
                    qreal dx = myRect.left() - oRect.left();
                    if (qAbs(dx) < SNAP_THRESHOLD) { snapX = newPos.x() - dx; snappedX = true; guideX = oRect.left(); }
                    else { dx = myRect.right() - oRect.right(); if (qAbs(dx) < SNAP_THRESHOLD) { snapX = newPos.x() - dx; snappedX = true; guideX = oRect.right(); } }
                    if (!snappedX) { dx = myRect.center().x() - oRect.center().x(); if (qAbs(dx) < SNAP_THRESHOLD) { snapX = newPos.x() - dx; snappedX = true; guideX = oRect.center().x(); } }
                    if (!snappedX) { dx = myRect.left() - oRect.right(); if (qAbs(dx) < SNAP_THRESHOLD) { snapX = newPos.x() - dx; snappedX = true; guideX = oRect.right(); } }
                    if (!snappedX) { dx = myRect.right() - oRect.left(); if (qAbs(dx) < SNAP_THRESHOLD) { snapX = newPos.x() - dx; snappedX = true; guideX = oRect.left(); } }
                }

                if (!snappedY) {
                    qreal dy = myRect.top() - oRect.top();
                    if (qAbs(dy) < SNAP_THRESHOLD) { snapY = newPos.y() - dy; snappedY = true; guideY = oRect.top(); }
                    else { dy = myRect.bottom() - oRect.bottom(); if (qAbs(dy) < SNAP_THRESHOLD) { snapY = newPos.y() - dy; snappedY = true; guideY = oRect.bottom(); } }
                    if (!snappedY) { dy = myRect.center().y() - oRect.center().y(); if (qAbs(dy) < SNAP_THRESHOLD) { snapY = newPos.y() - dy; snappedY = true; guideY = oRect.center().y(); } }
                    if (!snappedY) { dy = myRect.top() - oRect.bottom(); if (qAbs(dy) < SNAP_THRESHOLD) { snapY = newPos.y() - dy; snappedY = true; guideY = oRect.bottom(); } }
                    if (!snappedY) { dy = myRect.bottom() - oRect.top(); if (qAbs(dy) < SNAP_THRESHOLD) { snapY = newPos.y() - dy; snappedY = true; guideY = oRect.top(); } }
                }

                minX1 = qMin(minX1, oRect.left());
                maxX2 = qMax(maxX2, oRect.right());
                minY1 = qMin(minY1, oRect.top());
                maxY2 = qMax(maxY2, oRect.bottom());

                if (snappedX && snappedY) break;
            }

            if (snappedX || snappedY) {
                ensureGuides(scene());
                if (s_vGuide) {
                    if (snappedX) { s_vGuide->setLine(guideX, minY1 - 20, guideX, maxY2 + 20); s_vGuide->setVisible(true); }
                    else s_vGuide->setVisible(false);
                }
                if (s_hGuide) {
                    if (snappedY) { s_hGuide->setLine(minX1 - 20, guideY, maxX2 + 20, guideY); s_hGuide->setVisible(true); }
                    else s_hGuide->setVisible(false);
                }
                return QPointF(snappedX ? snapX : newPos.x(), snappedY ? snapY : newPos.y());
            } else {
                hideGuides();
            }
        }
    }
    if (change == ItemPositionHasChanged || change == ItemScenePositionHasChanged) {
        // 图片位置变化后通知场景刷新吸附到它的链接线（setPos / moveBy / 拖动）
        if (auto *sc = dynamic_cast<CanvasScene *>(scene()))
            sc->refreshLinksFor(this);
    }
    return QGraphicsObject::itemChange(change, value);
}

void ImageItem::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        if (auto *sc = dynamic_cast<CanvasScene*>(scene()))
            sc->removeSelectedItems();
        event->accept();
        return;
    }
    // Nudge with arrow keys
    qreal step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    switch (event->key()) {
    case Qt::Key_Left:  moveBy(-step, 0); event->accept(); return;
    case Qt::Key_Right: moveBy(step, 0);  event->accept(); return;
    case Qt::Key_Up:    moveBy(0, -step); event->accept(); return;
    case Qt::Key_Down:  moveBy(0, step);  event->accept(); return;
    default: break;
    }
    QGraphicsObject::keyPressEvent(event);
}

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    // Draw image
    painter->drawPixmap(QRectF(0, 0, m_size.width(), m_size.height()), m_pixmap,
                        QRectF(0, 0, m_pixmap.width(), m_pixmap.height()));

    // Selection outline
    if (isSelected()) {
        QColor borderColor = m_locked ? QColor(255, 165, 0) : QColor(0, 160, 255);
        QPen pen(borderColor, 1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(0, 0, m_size.width(), m_size.height()));

        if (!m_locked) {
            // Corner handles
            painter->setBrush(QColor(255, 255, 255));
            const Handle handles[] = { TopLeft, TopRight, BottomLeft, BottomRight };
            for (Handle h : handles) {
                painter->drawRect(handleRect(h));
            }

            // Rotate handle + line
            qreal cx = m_size.width() / 2.0;
            painter->drawLine(QPointF(cx, 0), QPointF(cx, -ROTATE_DIST));
            painter->drawEllipse(handleRect(RotateHandle));
        }
    }

    // Lock indicator
    if (m_locked) {
        qreal lockSize = 20;
        qreal x = m_size.width() - lockSize - 4;
        qreal y = 4;
        painter->setBrush(QColor(255, 165, 0, 200));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(QRectF(x, y, lockSize, lockSize), 3, 3);
        painter->setPen(QColor(255, 255, 255));
        QFont f = painter->font();
        f.setPixelSize(12);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(QRectF(x, y, lockSize, lockSize), Qt::AlignCenter, "🔒");
    }
}
