#include "canvasview.h"
#include "canvasscene.h"
#include "imageitem.h"
#include "annotationitems.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QApplication>
#include <QToolButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QGraphicsRectItem>
#include <QTimer>
#include <QShowEvent>
#include <QtMath>

// ========== FlightWidget (drag-to-canvas flight animation) ==========
class FlightWidget : public QWidget {
public:
    FlightWidget(const QPixmap &pix, const QPoint &from, const QPoint &to)
        : m_from(from), m_to(to), m_pixmap(pix), m_progress(0.0)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setFixedSize(72, 72);
        move(from.x() - 36, from.y() - 36);
        setWindowOpacity(1.0);

        // Control point for quadratic bezier (arc upward)
        m_control = QPoint((from.x() + to.x()) / 2, qMin(from.y(), to.y()) - 100);

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            m_progress += 1.0 / 28.0;
            if (m_progress >= 1.0) {
                m_timer->stop();
                close();
                return;
            }
            qreal t = m_progress;
            qreal x = (1-t)*(1-t)*m_from.x() + 2*(1-t)*t*m_control.x() + t*t*m_to.x();
            qreal y = (1-t)*(1-t)*m_from.y() + 2*(1-t)*t*m_control.y() + t*t*m_to.y();
            move(x - width()/2, y - height()/2);
            if (t > 0.65) setWindowOpacity(1.0 - (t - 0.65) / 0.35);
            qreal scale = 0.75 + 0.25 * qSin(t * M_PI);
            setFixedSize(72 * scale, 72 * scale);
            update();
        });
        m_timer->start();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        // Rounded background
        p.setBrush(QColor(20, 30, 45, 200));
        p.setPen(QPen(QColor(102, 192, 244, 150), 1));
        p.drawRoundedRect(rect(), 8, 8);
        // Image
        QRect imgRect = rect().adjusted(4, 4, -4, -4);
        if (!m_pixmap.isNull())
            p.drawPixmap(imgRect, m_pixmap);
        else
            p.fillRect(imgRect, QColor(102, 192, 244, 80));
    }

private:
    QPoint m_from, m_to, m_control;
    QPixmap m_pixmap;
    qreal m_progress;
    QTimer *m_timer;
};

// ========== MinimapWidget Implementation ==========
MinimapWidget::MinimapWidget(CanvasView *view, QWidget *parent)
    : QWidget(parent), m_canvasView(view)
{
    // Translucent background so the canvas stays visible through the minimap
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(180, 120);
    setPanelOpacity(m_opacity);
    setCursor(Qt::PointingHandCursor);
    setToolTip("迷你地图 - 点击跳转位置");
}

void MinimapWidget::setPanelOpacity(qreal opacity)
{
    // Accept up to 2.0 (200%). The thumbnail cannot exceed full opacity, but
    // the dark panel keeps darkening up to fully solid at 200%.
    m_opacity = qBound(0.0, opacity, 2.0);
    const qreal panelFactor = qMin(1.0, 0.92 * m_opacity);
    const int panelAlpha = qRound(255 * panelFactor);
    setStyleSheet(QString("background: rgba(12,20,28,%1); border: 1px solid rgba(102,192,244,0.55); border-radius: 6px;")
                  .arg(panelAlpha));
    update();
}

void MinimapWidget::setSizeScale(qreal scale)
{
    m_sizeScale = qBound(0.5, scale, 2.0);
    setFixedSize(qRound(180 * m_sizeScale), qRound(120 * m_sizeScale));
    updateMinimap();
    update();
}

void MinimapWidget::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible) updateMinimap();
}

void MinimapWidget::updateMinimap()
{
    if (!m_canvasView || !isVisible()) return;
    QGraphicsScene *scene = m_canvasView->scene();
    if (!scene) return;

    m_sceneRect = scene->itemsBoundingRect();
    if (m_sceneRect.isEmpty()) {
        m_sceneRect = QRectF(-500, -500, 1000, 1000);
    }
    // Add some padding
    m_sceneRect.adjust(-100, -100, 100, 100);

    qreal w = width() - 8, h = height() - 8;
    m_scale = qMin(w / m_sceneRect.width(), h / m_sceneRect.height());

    // Center the scene content within the thumbnail
    qreal contentW = m_sceneRect.width() * m_scale;
    qreal contentH = m_sceneRect.height() * m_scale;
    m_offsetX = (w - contentW) / 2.0 + 4;
    m_offsetY = (h - contentH) / 2.0 + 4;

    // Render scene to thumbnail
    m_thumbnail = QPixmap(w, h);
    m_thumbnail.fill(Qt::transparent);
    QPainter p(&m_thumbnail);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw the canvas background image first so the minimap stays in sync
    // with the canvas wallpaper (same pixmap, same baked-in dark overlay).
    const QPixmap &bg = m_canvasView->backgroundPixmap();
    if (!bg.isNull()) {
        p.drawPixmap(QRect(0, 0, w, h), bg);
    }

    p.translate(-m_sceneRect.left() * m_scale + m_offsetX, -m_sceneRect.top() * m_scale + m_offsetY);
    p.scale(m_scale, m_scale);
    // Suppress the scene's own background/grid while rendering the minimap so
    // the canvas background image painted above is not covered up.
    if (auto *cs = dynamic_cast<CanvasScene*>(scene)) cs->setBackgroundSuppressed(true);
    scene->render(&p, m_sceneRect, m_sceneRect);
    if (auto *cs = dynamic_cast<CanvasScene*>(scene)) cs->setBackgroundSuppressed(false);
    p.end();

    update();
}

void MinimapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw thumbnail (semi-transparent so the canvas behind stays visible,
    // avoiding occlusion; opacity follows the user setting, capped at full)
    if (!m_thumbnail.isNull()) {
        p.setOpacity(qMin(1.0, m_opacity));
        p.drawPixmap(4, 4, m_thumbnail);
        p.setOpacity(1.0);
    }

    // Draw viewport rectangle
    if (m_canvasView && m_scale > 0) {
        QRectF viewRect = m_canvasView->mapToScene(m_canvasView->viewport()->rect()).boundingRect();
        qreal x = (viewRect.left() - m_sceneRect.left()) * m_scale + m_offsetX;
        qreal y = (viewRect.top() - m_sceneRect.top()) * m_scale + m_offsetY;
        qreal w = viewRect.width() * m_scale;
        qreal h = viewRect.height() * m_scale;

        p.setBrush(QColor(102, 192, 244, 40));
        p.setPen(QPen(QColor(102, 192, 244), 1.5));
        p.drawRoundedRect(QRectF(x, y, w, h), 2, 2);
    }
}

void MinimapWidget::mousePressEvent(QMouseEvent *event)
{
    m_dragging = true;
    mouseMoveEvent(event);
}

void MinimapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || !m_canvasView || m_scale <= 0) return;

    qreal sceneX = (event->position().x() - m_offsetX) / m_scale + m_sceneRect.left();
    qreal sceneY = (event->position().y() - m_offsetY) / m_scale + m_sceneRect.top();

    m_canvasView->centerOn(sceneX, sceneY);
    update();
}

void MinimapWidget::resizeEvent(QResizeEvent *)
{
    updateMinimap();
}

// ========== CanvasView Implementation ==========

CanvasView::CanvasView(CanvasScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent), m_canvasScene(scene)
{
    // Software-rendered viewport (OpenGL causes horizontal stripe artifacts with drawBackground)
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemBoundingRect);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    // SmartViewportUpdate only repaints changed regions — much smoother for
    // zoom/pan than FullViewportUpdate. ImageItem disables its own cache
    // during drag (then restores DeviceCoordinateCache) to avoid ghosting.
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    // CacheBackground disabled - can cause stale cache ghosting with moving items
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setBackgroundBrush(QColor(27, 40, 56));

    // Create minimap — parent is CanvasView (not viewport) so it stays fixed
    // during panning/zooming and never gets clipped or moved by viewport scrolling.
    m_minimap = new MinimapWidget(this, this);
    m_minimap->move(width() - m_minimap->width() - 10, height() - m_minimap->height() - 10);
    m_minimap->raise();

    // Empty-state hint: shown when the scene has no items. Centered on the
    // viewport, sits above the background image but below real canvas items.
    m_emptyHint = new QLabel(this);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setText(
        "<div style='color:rgba(255,255,255,0.75); font-family:\"Microsoft YaHei\";'>"
        "<p style='font-size:20px; font-weight:300; margin-bottom:8px;'>画布是空的</p>"
        "<p style='font-size:13px; color:rgba(255,255,255,0.55);'>从左侧素材库拖图片到这里</p>"
        "<p style='font-size:13px; color:rgba(255,255,255,0.55);'>或按 Ctrl+V 粘贴剪贴板图片</p>"
        "</div>");
    m_emptyHint->setStyleSheet("background: transparent;");
    m_emptyHint->raise();

    // Update minimap when scene changes, optionally throttled so rapid changes
    // (e.g. dragging items) re-render the thumbnail at most every 150ms instead
    // of every frame. The 80ms timer below keeps repainting the cached
    // thumbnail + viewport rect in between, which is cheap.
    m_minimapThrottle = new QTimer(this);
    m_minimapThrottle->setSingleShot(true);
    m_minimapThrottle->setInterval(150);
    connect(m_minimapThrottle, &QTimer::timeout, this, [this]() {
        if (m_minimap && m_minimap->isVisible()) m_minimap->updateMinimap();
    });
    connect(scene, &QGraphicsScene::changed, this, [this]() {
        if (!m_minimap || !m_minimap->isVisible()) return;
        if (m_minimapThrottleEnabled) {
            if (!m_minimapThrottle->isActive()) m_minimapThrottle->start();
        } else {
            m_minimap->updateMinimap();
        }
        updateEmptyHint();
    });

    // Periodic viewport rectangle update (for zoom/pan)
    QTimer *minimapTimer = new QTimer(this);
    connect(minimapTimer, &QTimer::timeout, this, [this]() {
        if (m_minimap && m_minimap->isVisible()) m_minimap->update();
    });
    minimapTimer->start(80);

    createZoomBar();
    positionOverlays();
}

void CanvasView::createZoomBar()
{
    m_zoomBar = new QWidget(this);
    m_zoomBar->setStyleSheet(
        "background: rgba(18,28,40,0.85);"
        "border: 1px solid rgba(102,192,244,0.4);"
        "border-radius: 8px;");
    QHBoxLayout *lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(2);

    auto makeBtn = [this](const QString &txt, const QString &tip) {
        QToolButton *b = new QToolButton(m_zoomBar);
        b->setText(txt);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(26, 26);
        b->setStyleSheet(
            "QToolButton { background: transparent; border: none; border-radius: 5px;"
            " color: #c7d5e0; font-size: 16px; font-weight: 600; }"
            "QToolButton:hover { background: rgba(102,192,244,0.18); color: #66c0f4; }"
            "QToolButton:pressed { background: rgba(102,192,244,0.32); color: #ffffff; }");
        return b;
    };

    QToolButton *minus = makeBtn("−", "缩小  (Ctrl+-)");
    m_zoomPercent = makeBtn("100%", "点击恢复 100%");
    m_zoomPercent->setFixedWidth(54);
    m_zoomPercent->setStyleSheet(
        "QToolButton { background: transparent; border: none; border-radius: 5px;"
        " color: #66c0f4; font-size: 12px; font-weight: 700; }"
        "QToolButton:hover { background: rgba(102,192,244,0.18); }");
    QToolButton *plus = makeBtn("＋", "放大  (Ctrl++)");
    QToolButton *fit = makeBtn("⛶", "适应窗口");

    lay->addWidget(minus);
    lay->addWidget(m_zoomPercent);
    lay->addWidget(plus);
    lay->addWidget(fit);
    m_zoomBar->adjustSize();

    connect(minus, &QToolButton::clicked, this, &CanvasView::zoomOut);
    connect(plus, &QToolButton::clicked, this, &CanvasView::zoomIn);
    connect(fit, &QToolButton::clicked, this, &CanvasView::zoomToFit);
    connect(m_zoomPercent, &QToolButton::clicked, this, &CanvasView::zoomActual);
    connect(this, &CanvasView::zoomChanged, this, [this](qreal z) {
        if (m_zoomPercent)
            m_zoomPercent->setText(QString("%1%").arg(qRound(z * 100)));
    });
}

void CanvasView::setZoomBarOpacity(qreal opacity)
{
    m_zoomBarOpacity = qBound(0.2, opacity, 1.0);
    if (m_zoomBar) {
        int alpha = qRound(255 * m_zoomBarOpacity);
        m_zoomBar->setStyleSheet(QString(
            "background: rgba(18,28,40,%1);"
            "border: 1px solid rgba(102,192,244,0.4);"
            "border-radius: 8px;").arg(alpha));
    }
}

QPoint CanvasView::clampOverlayPos(const QSize &size, const QPoint &pos) const
{
    // On windows too small to hold the overlay plus its margin, prefer the
    // margin (the overlay stays anchored top-left) rather than pushing it off
    // the opposite edge, where it would be completely invisible.
    const int maxX = qMax(OVERLAY_MARGIN, width() - size.width() - OVERLAY_MARGIN);
    const int maxY = qMax(OVERLAY_MARGIN, height() - size.height() - OVERLAY_MARGIN);
    return QPoint(qBound(OVERLAY_MARGIN, pos.x(), maxX),
                  qBound(OVERLAY_MARGIN, pos.y(), maxY));
}

void CanvasView::positionOverlays()
{
    // Minimap is anchored to the bottom-right corner
    if (m_minimap) {
        m_minimap->move(width() - m_minimap->width() - OVERLAY_MARGIN,
                        height() - m_minimap->height() - OVERLAY_MARGIN);
    }

    if (!m_zoomBar) return;

    m_zoomBar->adjustSize();
    const QSize barSize = m_zoomBar->size();
    QPoint pos;

    // m_minimapVisible is authoritative: isVisible() is false until the view is
    // first shown, which would mis-place the bar during construction.
    if (m_minimap && m_minimapVisible) {
        const QRect mm = m_minimap->geometry();
        // Preferred: horizontally centered on the minimap, just above it
        pos = QPoint(mm.center().x() - barSize.width() / 2,
                     mm.top() - OVERLAY_GAP - barSize.height());

        if (pos.y() < OVERLAY_MARGIN) {
            // Not enough room above (narrow/short window or large minimap):
            // move beside the minimap, bottom-aligned with it.
            pos = QPoint(mm.left() - OVERLAY_GAP - barSize.width(),
                         mm.bottom() + 1 - barSize.height());

            if (pos.x() < OVERLAY_MARGIN) {
                // Still no room: keep it centered over the minimap instead of
                // letting it drift off-screen (drawn above via raise()).
                pos = QPoint(mm.center().x() - barSize.width() / 2,
                             mm.bottom() + 1 - barSize.height());
            }
        }
    } else {
        // No minimap: tuck the bar into the bottom-right corner
        pos = QPoint(width() - barSize.width() - OVERLAY_MARGIN,
                     height() - barSize.height() - OVERLAY_MARGIN);
    }

    m_zoomBar->move(clampOverlayPos(barSize, pos));
    m_zoomBar->raise();
}

bool CanvasView::isPanning() const
{
    return m_spacePressed || m_panning;
}

void CanvasView::applyZoom(qreal factor, const QPoint &anchorPos)
{
    qreal newZoom = m_zoom * factor;
    newZoom = qBound(MIN_ZOOM, newZoom, MAX_ZOOM);
    if (qFuzzyCompare(newZoom, m_zoom)) return;

    // Anchor zoom under mouse
    QPointF sceneAnchor = mapToScene(anchorPos);
    qreal actualFactor = newZoom / m_zoom;
    scale(actualFactor, actualFactor);
    m_zoom = newZoom;

    // Adjust scroll so the point under mouse stays
    QPointF newViewAnchor = mapFromScene(sceneAnchor);
    QPointF delta = newViewAnchor - anchorPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());

    emit zoomChanged(m_zoom);
}

void CanvasView::setZoom(qreal level)
{
    level = qBound(MIN_ZOOM, level, MAX_ZOOM);
    qreal factor = level / m_zoom;
    scale(factor, factor);
    m_zoom = level;
    emit zoomChanged(m_zoom);
}

void CanvasView::zoomIn()
{
    applyZoom(1.2, rect().center());
}

void CanvasView::zoomOut()
{
    applyZoom(1.0 / 1.2, rect().center());
}

void CanvasView::zoomActual()
{
    setZoom(1.0);
    centerOn(0, 0);
}

void CanvasView::zoomToFit()
{
    auto items = m_canvasScene->imageItems();
    if (items.isEmpty()) {
        zoomActual();
        return;
    }
    QRectF bounds;
    for (auto *item : items) {
        bounds = bounds.united(item->sceneBoundingRect());
    }
    if (bounds.isEmpty()) return;

    // Add margin
    qreal margin = 80;
    bounds.adjust(-margin, -margin, margin, margin);

    fitInView(bounds, Qt::KeepAspectRatio);
    // Recalculate actual zoom
    qreal sx = transform().m11();
    qreal sy = transform().m22();
    m_zoom = qMin(sx, sy);
    emit zoomChanged(m_zoom);
}

void CanvasView::setBackgroundImage(const QString &path)
{
    if (path == m_bgPath) return;
    m_bgPath = path;

    if (!path.isEmpty() && QFile::exists(path)) {
        QPixmap original(path);
        if (!original.isNull()) {
            // Pre-scale to 1600x900 base
            m_bgPixmap = original.scaled(1600, 900, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            // Pre-multiply dark overlay into pixmap (avoids runtime alpha blending)
            QPainter p(&m_bgPixmap);
            p.fillRect(m_bgPixmap.rect(), QColor(20, 20, 24, 140));
            p.end();
            // Pre-scale to viewport size for fast 1:1 blit
            if (!viewport()->rect().isEmpty()) {
                m_scaledBgPixmap = m_bgPixmap.scaled(viewport()->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            } else {
                m_scaledBgPixmap = m_bgPixmap;
            }
        }
        setStyleSheet("background: transparent; border: none;");
    } else {
        m_bgPixmap = QPixmap();
        m_scaledBgPixmap = QPixmap();
        setStyleSheet("background: #1e1e20; border: none;");
    }
    viewport()->update();

    // Keep the minimap background in sync with the canvas background image
    if (m_minimap) m_minimap->updateMinimap();
}

void CanvasView::drawBackground(QPainter *painter, const QRectF &rect)
{
    if (!m_scaledBgPixmap.isNull()) {
        painter->save();
        painter->resetTransform();
        painter->drawPixmap(0, 0, m_scaledBgPixmap);
        painter->restore();
    } else {
        // Use backgroundBrush so semi-transparent overlay works with global wallpaper
        painter->fillRect(rect, backgroundBrush().color());
    }
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl+wheel = horizontal scroll
        horizontalScrollBar()->setValue(
            horizontalScrollBar()->value() - event->angleDelta().y() / 4);
    } else {
        qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
        applyZoom(factor, event->position().toPoint());
    }
    event->accept();
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
    CanvasScene::Tool tool = m_canvasScene->currentTool();

    // Annotation drawing with left button
    if (event->button() == Qt::LeftButton && tool != CanvasScene::SelectTool && !m_spacePressed) {
        QPointF scenePos = mapToScene(event->pos());
        m_drawing = true;
        m_drawStart = scenePos;

        if (tool == CanvasScene::ArrowTool) {
            m_currentArrow = m_canvasScene->createArrow(scenePos, scenePos);
        } else if (tool == CanvasScene::RectTool) {
            m_currentRect = new QGraphicsRectItem(QRectF(scenePos, QSizeF(1, 1)));
            m_currentRect->setPen(QPen(m_canvasScene->annotationColor(), m_canvasScene->annotationWidth()));
            m_currentRect->setFlag(QGraphicsItem::ItemIsSelectable, true);
            m_currentRect->setFlag(QGraphicsItem::ItemIsMovable, true);
            m_canvasScene->addItem(m_currentRect);
        } else if (tool == CanvasScene::PenTool) {
            m_currentPen = m_canvasScene->createPen();
            m_currentPen->addPoint(scenePos);
        } else if (tool == CanvasScene::TextTool) {
            m_canvasScene->addTextAnnotation(scenePos);
            m_drawing = false;
        }
        event->accept();
        return;
    }

    // Pan with middle mouse or space+left, or left on empty area
    bool canPan = m_spacePressed || event->button() == Qt::MiddleButton;

    if (canPan && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // Left click on empty area: deselect and start panning (PureRef style)
    // Hold Ctrl or Shift to rubber-band select instead
    if (event->button() == Qt::LeftButton && !m_spacePressed) {
        QPointF scenePos = mapToScene(event->pos());
        QGraphicsItem *item = scene()->itemAt(scenePos, transform());
        if (!item) {
            bool selectMode = event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
            if (selectMode) {
                // Rubber-band selection
                setDragMode(QGraphicsView::RubberBandDrag);
                if (event->modifiers() & Qt::ControlModifier) {
                    // Ctrl: additive selection - don't clear
                } else {
                    // Shift: also additive, keep existing
                }
                QGraphicsView::mousePressEvent(event);
                return;
            }
            m_panning = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            scene()->clearSelection();
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drawing) {
        QPointF scenePos = mapToScene(event->pos());
        CanvasScene::Tool tool = m_canvasScene->currentTool();
        if (tool == CanvasScene::ArrowTool && m_currentArrow) {
            m_currentArrow->setEnd(scenePos);
        } else if (tool == CanvasScene::RectTool && m_currentRect) {
            m_currentRect->setRect(QRectF(m_drawStart, scenePos).normalized());
        } else if (tool == CanvasScene::PenTool && m_currentPen) {
            m_currentPen->addPoint(scenePos);
        }
        event->accept();
        return;
    }

    if (m_panning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drawing) {
        m_drawing = false;
        m_currentArrow = nullptr;
        m_currentPen = nullptr;
        m_currentRect = nullptr;
        event->accept();
        return;
    }

    if (m_panning) {
        m_panning = false;
        setCursor(m_spacePressed ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);

    // Reset from rubber-band selection back to normal
    if (dragMode() == QGraphicsView::RubberBandDrag) {
        setDragMode(QGraphicsView::NoDrag);
    }
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = true;
        if (!m_panning) setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    // +/- zoom
    if (event->key() == Qt::Key_Equal || event->key() == Qt::Key_Plus) {
        zoomIn();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Minus) {
        zoomOut();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_0) {
        zoomActual();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_F) {
        zoomToFit();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spacePressed = false;
        if (!m_panning) unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void CanvasView::dropEvent(QDropEvent *event)
{
    QStringList paths;
    const QMimeData *mime = event->mimeData();

    if (mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile())
                paths.append(url.toLocalFile());
        }
    }

    if (!paths.isEmpty()) {
        // Flight animation: from left edge to drop position
        QPoint to = mapToGlobal(event->position().toPoint());
        QPoint from = mapToGlobal(QPoint(0, event->position().toPoint().y()));
        if ((from - to).manhattanLength() > 30) {
            FlightWidget *flight = new FlightWidget(QPixmap(), from, to);
            flight->show();
        }
        emit fileDropped(paths);
        event->acceptProposedAction();
        return;
    }

    if (mime->hasImage()) {
        QPixmap pm = qvariant_cast<QPixmap>(mime->imageData());
        if (!pm.isNull()) {
            QPointF scenePos = mapToScene(event->position().toPoint());
            m_canvasScene->addImage(pm, scenePos);
        }
        event->acceptProposedAction();
        return;
    }

    QGraphicsView::dropEvent(event);
}

void CanvasView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // Re-scale background to new viewport size for fast 1:1 blit
    if (!m_bgPixmap.isNull() && !viewport()->rect().isEmpty()) {
        m_scaledBgPixmap = m_bgPixmap.scaled(viewport()->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    positionOverlays();
    if (m_emptyHint) {
        m_emptyHint->setGeometry(0, 0, width(), height());
    }
    updateEmptyHint();
}

void CanvasView::showEvent(QShowEvent *event)
{
    // Defer full viewport repaint to avoid blocking workspace switch.
    // FullViewportUpdate + many image caches can cause noticeable lag on show.
    setUpdatesEnabled(false);
    QGraphicsView::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        setUpdatesEnabled(true);
        viewport()->update();
    });
}

void CanvasView::toggleMinimap()
{
    setMinimapVisible(!m_minimapVisible);
}

void CanvasView::setMinimapVisible(bool visible)
{
    if (!m_minimap) return;
    m_minimapVisible = visible;
    m_minimap->setVisible(visible);
    if (visible) m_minimap->updateMinimap();
    positionOverlays();
}

void CanvasView::setMinimapScale(qreal scale)
{
    if (!m_minimap) return;
    m_minimap->setSizeScale(scale);
    // Keep anchored to the bottom-right corner
    positionOverlays();
}

void CanvasView::setMinimapThrottleEnabled(bool enabled)
{
    m_minimapThrottleEnabled = enabled;
    if (!enabled && m_minimapThrottle) m_minimapThrottle->stop();
    // Refresh immediately so the thumbnail reflects the new mode
    if (m_minimap && m_minimap->isVisible()) m_minimap->updateMinimap();
}

void CanvasView::updateEmptyHint()
{
    if (!m_emptyHint || !m_canvasScene) return;
    // Empty-state hint disabled by user request.
    m_emptyHint->setVisible(false);
}
