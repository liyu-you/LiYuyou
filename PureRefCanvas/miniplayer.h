#ifndef MINIPLAYER_H
#define MINIPLAYER_H

#include <QWidget>
#include <QPoint>

#include "playpausebutton.h"
#include "playerwidgets.h"

class QLabel;
class QPushButton;

// ========== Mini floating always-on-top player (迷你模式) ==========
class MiniPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MiniPlayer(QWidget *parent = nullptr);
    // coverPath is optional: without art the label falls back to a title-tinted tile
    void setSong(const QString &title, const QString &artist, const QString &coverPath = QString());
    void setPlaying(bool playing);
    void setProgress(qint64 position, qint64 duration);
    // PlayMode enum order matches MusicPlayerWidget: 0 sequential,1 loop one,
    // 2 loop all, 3 shuffle.
    void setPlayMode(int mode);

signals:
    void playPauseClicked();
    void prevClicked();
    void nextClicked();
    void modeClicked();
    void playlistClicked();
    void seekRequested(qint64 ms);
    void closed();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    PlayPauseButton *m_playBtn;
    NCButton *m_prevBtn;
    NCButton *m_nextBtn;
    NCButton *m_modeBtn;
    NCButton *m_playlistBtn;
    ThinSlider *m_progressSlider;
    NCButton *m_closeBtn;
    QPoint m_dragOffset;
    bool m_dragging = false;
    qint64 m_duration = 0;
    QString m_coverArtPath;   // art the label pixmap was built from
    QPixmap m_coverArt;       // decoded once per cover
};

#endif
