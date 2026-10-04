#ifndef IMAGEPREVIEW_H
#define IMAGEPREVIEW_H

#include <QDialog>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QLabel;
class QPushButton;
class QSlider;

class ImagePreview : public QDialog
{
    Q_OBJECT
public:
    explicit ImagePreview(const QString &filePath, QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void actualSize();
    void fitToWindow();
    void onZoomSliderChanged(int value);

private:
    void setupUI();
    void loadImage();
    void applyZoom(double factor, const QPointF &centerScene = QPointF());
    void updateZoomUI();
    QString formatFileSize(qint64 bytes) const;

    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;

    QLabel *m_zoomLabel;
    QLabel *m_infoLabel;
    QSlider *m_zoomSlider;
    QPushButton *m_fitBtn;
    QPushButton *m_actualBtn;

    QString m_filePath;
    QPixmap m_pixmap;
    double m_zoomLevel;
    bool m_userZoomed;
};

#endif
