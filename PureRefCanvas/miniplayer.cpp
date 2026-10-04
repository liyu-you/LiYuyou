#include "miniplayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QColor>
#include <QRectF>
#include <QMouseEvent>

// ========== MiniPlayer (迷你模式) ==========
MiniPlayer::MiniPlayer(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground); // rounded panel corners
    setFixedSize(430, 76);
    setToolTip("迷你播放器 - 拖动移动，双击还原");

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 8, 10, 6);
    outer->setSpacing(2);

    QHBoxLayout *mainLay = new QHBoxLayout;
    mainLay->setSpacing(8);

    m_coverLabel = new QLabel("♫");
    m_coverLabel->setFixedSize(46, 46);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet("background: #34353a; border-radius: 6px; color: #a0a0a0; font-size: 20px;");
    mainLay->addWidget(m_coverLabel);

    QVBoxLayout *infoLay = new QVBoxLayout;
    infoLay->setSpacing(2);
    m_titleLabel = new QLabel("未播放");
    m_titleLabel->setStyleSheet("color: #e8e8e8; font-size: 13px; font-weight: 500; background: transparent;");
    m_artistLabel = new QLabel("—");
    m_artistLabel->setStyleSheet("color: #8a8a8a; font-size: 11px; background: transparent;");
    infoLay->addWidget(m_titleLabel);
    infoLay->addWidget(m_artistLabel);
    mainLay->addLayout(infoLay, 1);

    m_modeBtn = new NCButton(NCIcons::Repeat);
    m_modeBtn->setFixedSize(25, 25);
    m_modeBtn->setPadding(4);
    m_modeBtn->setToolTip("播放模式");
    m_prevBtn = new NCButton(NCIcons::Prev);
    m_prevBtn->setFixedSize(26, 26);
    m_prevBtn->setPadding(3);
    m_playBtn = new PlayPauseButton;
    m_playBtn->setColors(QColor("#c20c0c"), QColor(255, 255, 255));
    m_playBtn->setFixedSize(32, 32);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setFocusPolicy(Qt::NoFocus);
    m_nextBtn = new NCButton(NCIcons::Next);
    m_nextBtn->setFixedSize(26, 26);
    m_nextBtn->setPadding(3);
    m_playlistBtn = new NCButton(NCIcons::Playlist);
    m_playlistBtn->setFixedSize(25, 25);
    m_playlistBtn->setPadding(4);
    m_playlistBtn->setBgColor(QColor("#2b2c2f"));
    m_playlistBtn->setToolTip("播放列表");

    connect(m_modeBtn, &QPushButton::clicked, this, &MiniPlayer::modeClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &MiniPlayer::prevClicked);
    connect(m_playBtn, &QPushButton::clicked, this, &MiniPlayer::playPauseClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &MiniPlayer::nextClicked);
    connect(m_playlistBtn, &QPushButton::clicked, this, &MiniPlayer::playlistClicked);

    QHBoxLayout *ctrlLay = new QHBoxLayout;
    ctrlLay->setSpacing(4);
    ctrlLay->addWidget(m_modeBtn);
    ctrlLay->addWidget(m_prevBtn);
    ctrlLay->addWidget(m_playBtn);
    ctrlLay->addWidget(m_nextBtn);
    ctrlLay->addWidget(m_playlistBtn);
    mainLay->addLayout(ctrlLay);

    m_closeBtn = new NCButton(NCIcons::Expand);
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setPadding(4);
    m_closeBtn->setBaseColor(QColor("#a0a0a0"));
    m_closeBtn->setToolTip("退出迷你模式");
    connect(m_closeBtn, &QPushButton::clicked, this, &MiniPlayer::closed);
    mainLay->addWidget(m_closeBtn);

    outer->addLayout(mainLay);

    m_progressSlider = new ThinSlider;
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setLive(false);
    m_progressSlider->setFixedHeight(12);
    connect(m_progressSlider, &ThinSlider::seekFinished, this, [this](int v) {
        if (m_duration > 0) emit seekRequested(m_duration * v / 1000);
    });
    outer->addWidget(m_progressSlider);
}

void MiniPlayer::setPlayMode(int mode)
{
    NCIcons::IconType t = NCIcons::Repeat;
    switch (mode) {
    case 0: t = NCIcons::Sequential; break;
    case 1: t = NCIcons::RepeatOne;  break;
    case 2: t = NCIcons::Repeat;     break;
    case 3: t = NCIcons::Shuffle;    break;
    }
    m_modeBtn->setIconType(t);
}

void MiniPlayer::setSong(const QString &title, const QString &artist, const QString &coverPath)
{
    m_titleLabel->setText(title.isEmpty() ? "未播放" : title);
    m_artistLabel->setText(artist.isEmpty() ? "—" : artist);

    // Show the song's cover art when it has any; otherwise a tile tinted from the
    // title, so a glance at the floating window still identifies the track. The
    // cover is decoded once per track, not on every status update.
    if (!coverPath.isEmpty() && coverPath != m_coverArtPath) {
        const QPixmap raw(coverPath);
        m_coverArtPath = coverPath;
        m_coverArt = raw.isNull()
            ? QPixmap()
            : raw.scaled(m_coverLabel->size(), Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);
    } else if (coverPath.isEmpty()) {
        m_coverArtPath.clear();
        m_coverArt = QPixmap();
    }

    QPixmap tile(m_coverLabel->size());
    tile.fill(Qt::transparent);
    {
        QPainter p(&tile);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(tile.rect()), 6, 6);
        p.setClipPath(clip);
        if (!m_coverArt.isNull()) {
            p.drawPixmap(0, 0, m_coverArt);
        } else {
            const int hue = title.isEmpty() ? 205 : int(qHash(title) % 360);
            p.fillRect(tile.rect(), QColor::fromHsl(hue, 130, 95));
            p.setPen(QColor(230, 230, 230));
            QFont f = p.font();
            f.setBold(true);
            f.setPointSize(14);
            p.setFont(f);
            p.drawText(tile.rect(), Qt::AlignCenter,
                       title.isEmpty() ? QStringLiteral("♫") : title.left(2));
        }
    }
    m_coverLabel->setText(QString());
    m_coverLabel->setPixmap(tile);
}

void MiniPlayer::setPlaying(bool playing)
{
    m_playBtn->setPlaying(playing);
}

void MiniPlayer::setProgress(qint64 position, qint64 duration)
{
    m_duration = duration;
    if (!m_progressSlider->isSliding()) {
        m_progressSlider->setValue(duration > 0 ? int(position * 1000 / duration) : 0);
    }
}

void MiniPlayer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, width() - 1, height() - 1), 10, 10);
    p.fillPath(path, QColor("#2b2c2f"));
    p.setPen(QPen(QColor("#3a3b40"), 1));
    p.drawPath(path);
}

void MiniPlayer::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
        m_dragging = true;
    }
    QWidget::mousePressEvent(e);
}

void MiniPlayer::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging) move(e->globalPosition().toPoint() - m_dragOffset);
    QWidget::mouseMoveEvent(e);
}

void MiniPlayer::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(e);
}

void MiniPlayer::mouseDoubleClickEvent(QMouseEvent *)
{
    emit closed();
}
