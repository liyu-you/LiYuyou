#ifndef CANVASVIEW_H
#define CANVASVIEW_H

#include <QGraphicsView>
#include <QWidget>
#include <QPixmap>

class QTimer;
class QToolButton;
class QLabel;
class CanvasScene;
class ArrowItem;
class PenItem;
class ImageItem;
class QGraphicsRectItem;
class QGraphicsPathItem;
class CanvasView;

// Minimap widget - shows bird's eye view of the entire canvas
class MinimapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MinimapWidget(CanvasView *view, QWidget *parent = nullptr);
    void updateMinimap();
    void setVisible(bool visible) override;
    // Control how opaque the minimap panel/thumbnail is (0.0 = fully see-through,
    // 1.0 = fully opaque). Lower values reduce occlusion of the canvas behind it.
    void setPanelOpacity(qreal opacity);
    qreal panelOpacity() const { return m_opacity; }
    // Scale the minimap size (1.0 = 180x120)
    void setSizeScale(qreal scale);
    qreal sizeScale() const { return m_sizeScale; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    CanvasView *m_canvasView;
    QPixmap m_thumbnail;
    bool m_dragging = false;
    qreal m_scale = 1.0;
    QRectF m_sceneRect;
    qreal m_opacity = 0.7;
    qreal m_sizeScale = 1.0;
    qreal m_offsetX = 4.0;
    qreal m_offsetY = 4.0;
};

class CanvasView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit CanvasView(CanvasScene *scene, QWidget *parent = nullptr);

    qreal zoomLevel() const { return m_zoom; }
    void setZoom(qreal level);
    void zoomIn();
    void zoomOut();
    void zoomActual();
    void zoomToFit();
    void setBackgroundImage(const QString &path);
    void toggleMinimap();
    void setMinimapVisible(bool visible);
    bool minimapVisible() const { return m_minimapVisible; }
    void setMinimapScale(qreal scale);
    void setZoomBarOpacity(qreal opacity);
    qreal zoomBarOpacity() const { return m_zoomBarOpacity; }
    // Throttle minimap thumbnail re-renders during rapid scene changes (dragging)
    void setMinimapThrottleEnabled(bool enabled);
    bool minimapThrottleEnabled() const { return m_minimapThrottleEnabled; }
    MinimapWidget *minimap() const { return m_minimap; }
    // Expose the canvas background image so the minimap can render it in sync
    const QPixmap &backgroundPixmap() const { return m_bgPixmap; }

signals:
    void zoomChanged(qreal level);
    void fileDropped(const QStringList &paths);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void showEvent(QShowEvent *event) override;

private:
    bool isPanning() const;
    void applyZoom(qreal factor, const QPoint &anchorPos);
    void createZoomBar();
    void positionOverlays();
    // Keeps a floating overlay fully inside the view, with a small margin
    QPoint clampOverlayPos(const QSize &size, const QPoint &pos) const;

    CanvasScene *m_canvasScene;
    bool m_panning = false;
    bool m_spacePressed = false;
    QPoint m_lastPanPos;
    qreal m_zoom = 1.0;

    // Annotation drawing state
    bool m_drawing = false;
    QPointF m_drawStart;
    ArrowItem *m_currentArrow = nullptr;
    PenItem *m_currentPen = nullptr;
    QGraphicsRectItem *m_currentRect = nullptr;

    // Link-line tool state（链接线：点两张图/空白处连线）
    QWidget *m_toolBar = nullptr;
    QToolButton *m_linkBtn = nullptr;
    bool m_linkToolActive = false;
    bool m_linkPending = false;          // 已选起点，等待第二击
    QPointF m_linkStartPt;               // 起点（锚图为图片中心）
    ImageItem *m_linkStartImg = nullptr; // 起点锚图（可为空 = 自由点）
    QGraphicsPathItem *m_linkPreview = nullptr; // 虚线预览（置顶、不拦截鼠标）
    void createToolBar();
    void setLinkToolActive(bool on);
    void clearLinkState();

    static constexpr qreal MIN_ZOOM = 0.05;
    static constexpr qreal MAX_ZOOM = 20.0;

    QPixmap m_bgPixmap;
    QPixmap m_scaledBgPixmap;
    QString m_bgPath;
    MinimapWidget *m_minimap = nullptr;
    bool m_minimapVisible = true;
    QTimer *m_minimapThrottle = nullptr;
    bool m_minimapThrottleEnabled = true;
    QWidget *m_zoomBar = nullptr;
    QToolButton *m_zoomPercent = nullptr;
    qreal m_zoomBarOpacity = 0.85;
    QLabel *m_emptyHint = nullptr;
    void updateEmptyHint();

    // Floating-overlay layout metrics (logical px)
    static constexpr int OVERLAY_MARGIN = 10;
    static constexpr int OVERLAY_GAP = 8;
};

#endif // CANVASVIEW_H
