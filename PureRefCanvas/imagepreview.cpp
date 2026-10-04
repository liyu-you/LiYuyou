#include "imagepreview.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QFileInfo>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTransform>
#include <QtMath>

ImagePreview::ImagePreview(const QString &filePath, QWidget *parent)
    : QDialog(parent), m_filePath(filePath), m_zoomLevel(1.0), m_userZoomed(false)
{
    setWindowTitle("图片预览");
    resize(1000, 750);
    setMinimumSize(400, 300);

    setupUI();
    loadImage();
}

void ImagePreview::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Graphics view
    m_view = new QGraphicsView(this);
    m_scene = new QGraphicsScene(this);
    m_view->setScene(m_scene);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setFocusPolicy(Qt::StrongFocus);

    // Checkerboard background for transparent images
    QPixmap checker(20, 20);
    QPainter cp(&checker);
    cp.fillRect(0, 0, 10, 10, QColor("#2a2a2a"));
    cp.fillRect(10, 10, 10, 10, QColor("#2a2a2a"));
    cp.fillRect(0, 10, 10, 10, QColor("#1e1e1e"));
    cp.fillRect(10, 0, 10, 10, QColor("#1e1e1e"));
    cp.end();
    m_view->setBackgroundBrush(QBrush(checker));
    m_view->setStyleSheet("QGraphicsView { border: none; background: #1e1e1e; }");

    m_pixmapItem = new QGraphicsPixmapItem();
    m_pixmapItem->setTransformationMode(Qt::SmoothTransformation);
    m_scene->addItem(m_pixmapItem);

    mainLayout->addWidget(m_view, 1);

    // Bottom toolbar
    QWidget *toolbar = new QWidget(this);
    toolbar->setFixedHeight(44);
    toolbar->setStyleSheet("background: #16202d; border-top: 1px solid #2a475e;");
    QHBoxLayout *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 6, 12, 6);
    tbLayout->setSpacing(10);

    m_fitBtn = new QPushButton("适应窗口");
    m_fitBtn->setFixedHeight(28);
    m_fitBtn->setStyleSheet("QPushButton { background: #2a475e; color: #c7d5e0; border: none; border-radius: 3px; padding: 4px 12px; font-size: 12px; } QPushButton:hover { background: #3d6b8a; color: #fff; }");
    connect(m_fitBtn, &QPushButton::clicked, this, &ImagePreview::fitToWindow);

    m_actualBtn = new QPushButton("原始大小");
    m_actualBtn->setFixedHeight(28);
    m_actualBtn->setStyleSheet("QPushButton { background: #2a475e; color: #c7d5e0; border: none; border-radius: 3px; padding: 4px 12px; font-size: 12px; } QPushButton:hover { background: #3d6b8a; color: #fff; }");
    connect(m_actualBtn, &QPushButton::clicked, this, &ImagePreview::actualSize);

    QPushButton *zoomOutBtn = new QPushButton("－");
    zoomOutBtn->setFixedSize(28, 28);
    zoomOutBtn->setStyleSheet("QPushButton { background: #2a475e; color: #c7d5e0; border: none; border-radius: 3px; font-size: 16px; font-weight: bold; } QPushButton:hover { background: #3d6b8a; color: #fff; }");
    connect(zoomOutBtn, &QPushButton::clicked, this, &ImagePreview::zoomOut);

    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(10, 800);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(140);
    m_zoomSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height: 3px; background: #2a475e; border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; height: 12px; margin: -5px 0; background: #66c0f4; border-radius: 6px; }
        QSlider::handle:horizontal:hover { background: #1a9fff; }
        QSlider::sub-page:horizontal { background: #66c0f4; border-radius: 2px; }
    )");
    connect(m_zoomSlider, &QSlider::valueChanged, this, &ImagePreview::onZoomSliderChanged);

    QPushButton *zoomInBtn = new QPushButton("＋");
    zoomInBtn->setFixedSize(28, 28);
    zoomInBtn->setStyleSheet("QPushButton { background: #2a475e; color: #c7d5e0; border: none; border-radius: 3px; font-size: 16px; font-weight: bold; } QPushButton:hover { background: #3d6b8a; color: #fff; }");
    connect(zoomInBtn, &QPushButton::clicked, this, &ImagePreview::zoomIn);

    m_zoomLabel = new QLabel("100%");
    m_zoomLabel->setStyleSheet("color: #8f98a0; font-size: 12px; min-width: 48px;");
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    m_infoLabel = new QLabel();
    m_infoLabel->setStyleSheet("color: #5a6570; font-size: 12px;");
    m_infoLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    tbLayout->addWidget(m_fitBtn);
    tbLayout->addWidget(m_actualBtn);
    tbLayout->addSpacing(6);
    tbLayout->addWidget(zoomOutBtn);
    tbLayout->addWidget(m_zoomSlider);
    tbLayout->addWidget(zoomInBtn);
    tbLayout->addWidget(m_zoomLabel);
    tbLayout->addStretch();
    tbLayout->addWidget(m_infoLabel);

    mainLayout->addWidget(toolbar);
}

void ImagePreview::loadImage()
{
    m_pixmap = QPixmap(m_filePath);
    if (m_pixmap.isNull()) {
        m_infoLabel->setText("无法加载图片");
        return;
    }

    m_pixmapItem->setPixmap(m_pixmap);
    m_scene->setSceneRect(m_pixmap.rect());

    QFileInfo fi(m_filePath);
    qint64 size = fi.size();
    m_infoLabel->setText(QString("%1 × %2  ·  %3  ·  %4")
        .arg(m_pixmap.width())
        .arg(m_pixmap.height())
        .arg(formatFileSize(size))
        .arg(fi.fileName()));

    fitToWindow();
}

void ImagePreview::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() == 0) {
        QDialog::wheelEvent(event);
        return;
    }

    m_userZoomed = true;

    // Map mouse position to scene coordinates BEFORE zooming
    QPointF scenePos = m_view->mapToScene(event->position().toPoint());

    double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    double newZoom = m_zoomLevel * factor;

    // Clamp zoom
    if (newZoom < 0.1) newZoom = 0.1;
    if (newZoom > 8.0) newZoom = 8.0;
    factor = newZoom / m_zoomLevel;

    m_zoomLevel = newZoom;

    // Apply scale
    m_view->scale(factor, factor);

    // Center on the mouse position in scene coordinates
    // This ensures the point under the mouse stays under the mouse
    m_view->centerOn(scenePos);

    updateZoomUI();
    event->accept();
}

void ImagePreview::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        break;
    case Qt::Key_Minus:
        zoomOut();
        break;
    case Qt::Key_0:
        actualSize();
        break;
    case Qt::Key_F:
        fitToWindow();
        break;
    case Qt::Key_Escape:
        close();
        break;
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}

void ImagePreview::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    // Auto-fit on first show or if user hasn't manually zoomed
    if (!m_userZoomed && !m_pixmap.isNull()) {
        fitToWindow();
    }
}

void ImagePreview::zoomIn()
{
    m_userZoomed = true;
    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    applyZoom(1.2, center);
}

void ImagePreview::zoomOut()
{
    m_userZoomed = true;
    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    applyZoom(1.0 / 1.2, center);
}

void ImagePreview::actualSize()
{
    m_userZoomed = true;
    m_zoomLevel = 1.0;
    m_view->resetTransform();
    updateZoomUI();
}

void ImagePreview::fitToWindow()
{
    if (m_pixmap.isNull()) return;

    m_view->resetTransform();

    QRectF viewRect = m_view->viewport()->rect();
    QRectF sceneRect = m_pixmap.rect();

    double scaleX = viewRect.width() / sceneRect.width();
    double scaleY = viewRect.height() / sceneRect.height();
    double scale = qMin(scaleX, scaleY);

    // Don't upscale small images beyond 2x in fit mode
    if (scale > 2.0) scale = 2.0;

    m_zoomLevel = scale;
    m_view->scale(scale, scale);
    m_view->centerOn(m_pixmap.rect().center());

    updateZoomUI();
}

void ImagePreview::onZoomSliderChanged(int value)
{
    m_userZoomed = true;
    double targetZoom = value / 100.0;
    double factor = targetZoom / m_zoomLevel;

    QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
    m_zoomLevel = targetZoom;
    m_view->scale(factor, factor);
    m_view->centerOn(center);

    updateZoomUI();
}

void ImagePreview::applyZoom(double factor, const QPointF &centerScene)
{
    double newZoom = m_zoomLevel * factor;
    if (newZoom < 0.1) newZoom = 0.1;
    if (newZoom > 8.0) newZoom = 8.0;
    factor = newZoom / m_zoomLevel;

    m_zoomLevel = newZoom;
    m_view->scale(factor, factor);
    if (!centerScene.isNull())
        m_view->centerOn(centerScene);

    updateZoomUI();
}

void ImagePreview::updateZoomUI()
{
    int percent = qRound(m_zoomLevel * 100);
    m_zoomLabel->setText(QString("%1%").arg(percent));

    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(qBound(10, percent, 800));
    m_zoomSlider->blockSignals(false);
}

QString ImagePreview::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
}
