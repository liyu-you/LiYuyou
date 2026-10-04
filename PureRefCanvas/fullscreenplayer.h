#ifndef FULLSCREENPLAYER_H
#define FULLSCREENPLAYER_H

#include <QWidget>
#include <QString>
#include <QRect>
#include <QColor>
#include <QVector>
#include <QPair>

#include "playpausebutton.h"
#include "playerwidgets.h"

class QTimer;
class QLabel;
class QSlider;
class QPushButton;
class QShowEvent;
class QHideEvent;
class QResizeEvent;

// Full-screen player mode (NetEase style)
class FullScreenPlayer : public QWidget {
    Q_OBJECT
public:
    explicit FullScreenPlayer(QWidget *parent = nullptr);
    void updateSong(const QString &title, const QString &artist, const QString &coverPath);
    void updateProgress(qint64 position, qint64 duration);
    void setPlaying(bool playing);
    void setLyrics(const QVector<QPair<qint64, QString>> &lyrics);
    void updateLyricLine(qint64 position);
    // Sync the play-mode glyph (0 sequential, 1 loop one, 2 loop all, 3 shuffle)
    void setPlayMode(int mode);

signals:
    void playPauseClicked();
    void prevClicked();
    void nextClicked();
    void volumeChanged(int value);
    void modeChanged();
    void coverModeToggled();
    void seekRequested(qint64 ms);
    void closed();

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onRotate();

private:
    // Repaint the spinning vinyl only while it can be seen and is actually moving
    void updateRotateTimer();
    // Decode the cover once and rescale only when the cover or the size changes
    void ensureCoverCache(int coverSize);

    QPixmap m_coverSource;        // decoded cover art, one decode per cover
    QPixmap m_bgCoverScaled;      // cover scaled to the window (background wash)
    QPixmap m_discCoverScaled;    // cover scaled to the disc/cover size
    QSize m_bgScaledFor;          // size m_bgCoverScaled was built for
    int m_discScaledFor = -1;     // coverSize m_discCoverScaled was built for
    QString m_coverCacheKey;      // path + mtime + size the caches were built from

    QString m_title, m_artist, m_coverPath;
    qint64 m_position = 0, m_duration = 0;
    bool m_playing = false;
    qreal m_rotation = 0;
    qreal m_rotSpeed = 0;          // degrees per tick (eased)
    bool m_coverMode = false;      // false = vinyl disc, true = flat cover
    QColor m_bgTint;               // dominant cover color for background
    QTimer *m_rotateTimer;
    QVector<QPair<qint64, QString>> m_lyrics;
    int m_currentLyricIndex = -1;
    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QLabel *m_currentTimeLabel;
    QLabel *m_totalTimeLabel;
    QSlider *m_progressSlider;
    PlayPauseButton *m_playBtn;
    NCButton *m_prevBtn;
    NCButton *m_nextBtn;
    NCButton *m_modeBtn;
    NCButton *m_coverModeBtn;
    NCButton *m_closeBtn;
    QSlider *m_volumeSlider;
    QLabel *m_lyricLabel;
    bool m_seeking = false;
    // Lyric row geometry (set during paint, used for click-to-seek)
    QRect m_lyricRect;
};

#endif
