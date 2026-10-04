#include "fullscreenplayer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QImage>
#include <QLinearGradient>
#include <QFont>
#include <QPen>
#include <QColor>
#include <QRect>
#include <QPoint>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector>
#include <QPair>

// ==================== FullScreenPlayer ====================

FullScreenPlayer::FullScreenPlayer(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::ArrowCursor);

    m_rotateTimer = new QTimer(this);
    connect(m_rotateTimer, &QTimer::timeout, this, &FullScreenPlayer::onRotate);
    // Not started here: it used to run from construction onwards for the whole
    // process lifetime, repainting (and re-decoding the cover) even while the
    // player was hidden or paused. updateRotateTimer() owns it now.
    m_rotateTimer->setInterval(30);

    // Bottom control bar
    QWidget *controlBar = new QWidget(this);
    controlBar->setObjectName("controlBar");
    controlBar->setStyleSheet("background: transparent;");
    QVBoxLayout *barLayout = new QVBoxLayout(controlBar);
    barLayout->setContentsMargins(50, 8, 50, 20);
    barLayout->setSpacing(6);

    // Progress row
    QHBoxLayout *progressRow = new QHBoxLayout;
    m_currentTimeLabel = new QLabel("00:00");
    m_currentTimeLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px;");
    m_currentTimeLabel->setFixedWidth(50);
    m_currentTimeLabel->setAlignment(Qt::AlignCenter);
    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,0.2); border-radius: 2px; }
        QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; background: #fff; border-radius: 6px; }
        QSlider::sub-page:horizontal { background: #c20c0c; border-radius: 2px; }
    )");
    connect(m_progressSlider, &QSlider::sliderPressed, this, [this](){ m_seeking = true; });
    connect(m_progressSlider, &QSlider::sliderReleased, this, [this](){
        m_seeking = false;
        const qint64 dur = m_duration;
        if (dur > 0) emit seekRequested(dur * m_progressSlider->value()
                                        / m_progressSlider->maximum());
    });
    m_totalTimeLabel = new QLabel("00:00");
    m_totalTimeLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px;");
    m_totalTimeLabel->setFixedWidth(50);
    m_totalTimeLabel->setAlignment(Qt::AlignCenter);
    progressRow->addWidget(m_currentTimeLabel);
    progressRow->addWidget(m_progressSlider, 1);
    progressRow->addWidget(m_totalTimeLabel);
    barLayout->addLayout(progressRow);

    // Controls row
    QHBoxLayout *ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(20);

    m_modeBtn = new NCButton(NCIcons::Repeat);
    m_modeBtn->setFixedSize(40, 40);
    m_modeBtn->setPadding(8);
    m_modeBtn->setBaseColor(QColor(235, 235, 235));
    m_modeBtn->setToolTip("播放模式");
    connect(m_modeBtn, &QPushButton::clicked, this, &FullScreenPlayer::modeChanged);

    // Toggle between vinyl disc and flat cover display
    m_coverModeBtn = new NCButton(NCIcons::Disc);
    m_coverModeBtn->setFixedSize(40, 40);
    m_coverModeBtn->setPadding(8);
    m_coverModeBtn->setBaseColor(QColor(235, 235, 235));
    m_coverModeBtn->setToolTip("切换黑胶 / 封面模式");
    connect(m_coverModeBtn, &QPushButton::clicked, this, [this]() {
        m_coverMode = !m_coverMode;
        m_coverModeBtn->setIconType(m_coverMode ? NCIcons::Photo : NCIcons::Disc);
        emit coverModeToggled();
        updateRotateTimer(); // flat cover mode has nothing to spin
        update();
    });

    m_prevBtn = new NCButton(NCIcons::Prev);
    m_prevBtn->setFixedSize(44, 44);
    m_prevBtn->setPadding(9);
    m_prevBtn->setBaseColor(QColor(240, 240, 240));
    connect(m_prevBtn, &QPushButton::clicked, this, &FullScreenPlayer::prevClicked);

    m_playBtn = new PlayPauseButton;
    m_playBtn->setFixedSize(56, 56);
    m_playBtn->setColors(QColor(255, 255, 255, 40), QColor(255, 255, 255, 230));
    connect(m_playBtn, &QPushButton::clicked, this, &FullScreenPlayer::playPauseClicked);

    m_nextBtn = new NCButton(NCIcons::Next);
    m_nextBtn->setFixedSize(44, 44);
    m_nextBtn->setPadding(9);
    m_nextBtn->setBaseColor(QColor(240, 240, 240));
    connect(m_nextBtn, &QPushButton::clicked, this, &FullScreenPlayer::nextClicked);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setFixedWidth(100);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height: 3px; background: rgba(255,255,255,0.2); border-radius: 1px; }
        QSlider::handle:horizontal { width: 10px; height: 10px; margin: -3px 0; background: #fff; border-radius: 5px; }
        QSlider::sub-page:horizontal { background: #c20c0c; border-radius: 1px; }
    )");
    connect(m_volumeSlider, &QSlider::valueChanged, this, &FullScreenPlayer::volumeChanged);

    m_closeBtn = new NCButton(NCIcons::Close);
    m_closeBtn->setFixedSize(36, 36);
    m_closeBtn->setPadding(7);
    m_closeBtn->setBaseColor(QColor(200, 200, 200));
    connect(m_closeBtn, &QPushButton::clicked, this, &FullScreenPlayer::closed);

    ctrlRow->addStretch();
    ctrlRow->addWidget(m_modeBtn);
    ctrlRow->addWidget(m_coverModeBtn);
    ctrlRow->addWidget(m_prevBtn);
    ctrlRow->addWidget(m_playBtn);
    ctrlRow->addWidget(m_nextBtn);
    ctrlRow->addWidget(m_volumeSlider);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_closeBtn);
    barLayout->addLayout(ctrlRow);

    controlBar->setGeometry(0, 0, width(), 130);
    controlBar->raise();
}

void FullScreenPlayer::updateProgress(qint64 position, qint64 duration)
{
    m_position = position;
    m_duration = duration;
    if (!m_seeking) {
        m_progressSlider->setMaximum(duration > 0 ? duration : 1);
        m_progressSlider->setValue(position);
    }
    auto fmt = [](qint64 ms) {
        int s = ms / 1000;
        return QString("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
    };
    m_currentTimeLabel->setText(fmt(position));
    m_totalTimeLabel->setText(fmt(duration));
}

void FullScreenPlayer::setPlaying(bool playing)
{
    m_playing = playing;
    if (m_playBtn) m_playBtn->setPlaying(playing);
    updateRotateTimer();
}

void FullScreenPlayer::setPlayMode(int mode)
{
    NCIcons::IconType t = NCIcons::Repeat;
    switch (mode) {
    case 0: t = NCIcons::Sequential; break;
    case 1: t = NCIcons::RepeatOne;  break;
    case 2: t = NCIcons::Repeat;     break;
    case 3: t = NCIcons::Shuffle;    break;
    }
    if (m_modeBtn) m_modeBtn->setIconType(t);
}

// The record only needs to be repainted while it is on screen, in vinyl mode and
// actually spinning (or still easing to a stop, which onRotate handles by leaving
// m_rotSpeed above zero for a moment).
void FullScreenPlayer::updateRotateTimer()
{
    const bool spin = isVisible() && !m_coverMode && (m_playing || m_rotSpeed > 0.004);
    if (spin) {
        if (!m_rotateTimer->isActive()) m_rotateTimer->start();
    } else if (m_rotateTimer->isActive()) {
        m_rotateTimer->stop();
    }
}

// Decoding the cover and smooth-scaling it twice per frame cost ~5.5 ms per frame
// at 33 fps (measured), i.e. most of a core for the whole time the full-screen
// player was open. Both are now done once per cover / per size change.
void FullScreenPlayer::ensureCoverCache(int coverSize)
{
    if (m_coverSource.isNull() && !m_coverPath.isEmpty())
        m_coverSource = QPixmap(m_coverPath); // decoded once
    if (m_coverSource.isNull()) return;

    if (m_bgCoverScaled.isNull() || m_bgScaledFor != size()) {
        m_bgCoverScaled = m_coverSource.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
        m_bgScaledFor = size();
    }
    if (m_discCoverScaled.isNull() || m_discScaledFor != coverSize) {
        m_discCoverScaled = m_coverSource.scaled(QSize(coverSize, coverSize),
                                                Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
        m_discScaledFor = coverSize;
    }
}

void FullScreenPlayer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateRotateTimer();
}

void FullScreenPlayer::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    updateRotateTimer(); // nothing to paint while hidden
}

void FullScreenPlayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Window size changed: the background wash and the disc both need rescaling,
    // which ensureCoverCache() picks up on the next paint from these two values.
    m_bgScaledFor = QSize();
    m_discScaledFor = -1;
}

// Sample a small grid of pixels to get a dominant-ish tint color of the cover
static QColor coverTintColor(const QString &coverPath)
{
    QPixmap pm(coverPath);
    if (pm.isNull()) return QColor();
    QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    const int W = img.width(), H = img.height();
    if (W <= 0 || H <= 0) return QColor();
    qint64 r = 0, g = 0, b = 0, n = 0;
    const int step = qMax(1, qMin(W, H) / 32);
    for (int y = 0; y < H; y += step)
        for (int x = 0; x < W; x += step) {
            QRgb px = img.pixel(x, y);
            const int a = qAlpha(px);
            if (a < 40) continue;
            r += qRed(px); g += qGreen(px); b += qBlue(px); n++;
        }
    if (n == 0) return QColor();
    return QColor(int(r / n), int(g / n), int(b / n));
}

void FullScreenPlayer::updateSong(const QString &title, const QString &artist, const QString &coverPath)
{
    m_title = title;
    m_artist = artist;

    // Cache key covers content changes too, not just a different path: a cover
    // cached to a fixed temporary file would otherwise keep showing the previous
    // image. Checked once per song change, not per frame.
    const QFileInfo info(coverPath);
    const QString key = coverPath.isEmpty()
        ? QString()
        : coverPath + QLatin1Char('|') + QString::number(info.lastModified().toMSecsSinceEpoch())
              + QLatin1Char('|') + QString::number(info.size());
    if (key != m_coverCacheKey) {
        m_coverCacheKey = key;
        m_coverSource = QPixmap();
        m_bgCoverScaled = QPixmap();
        m_discCoverScaled = QPixmap();
        m_bgScaledFor = QSize();
        m_discScaledFor = -1;
    }

    m_coverPath = coverPath;
    m_bgTint = coverTintColor(coverPath);
    update();
}

void FullScreenPlayer::setLyrics(const QVector<QPair<qint64, QString>> &lyrics)
{
    m_lyrics = lyrics;
    m_currentLyricIndex = -1;
    update();
}

void FullScreenPlayer::updateLyricLine(qint64 position)
{
    int idx = -1;
    for (int i = 0; i < m_lyrics.size(); ++i) {
        if (m_lyrics[i].first <= position) idx = i;
        else break;
    }
    if (idx != m_currentLyricIndex) {
        m_currentLyricIndex = idx;
        update();
    }
}

void FullScreenPlayer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Cover art: decoded and scaled once per cover/size rather than per frame.
    const int coverSize = qMin(320, height() - 220);
    ensureCoverCache(coverSize);

    // Rounded rect background
    QPainterPath bgPath;
    bgPath.addRoundedRect(rect(), 16, 16);
    p.setClipPath(bgPath);

    // Background: dark gradient tinted with the cover's dominant color
    QColor top = QColor(22, 28, 38), bot = QColor(12, 14, 20);
    if (m_bgTint.isValid()) {
        top = m_bgTint.darker(240);
        bot = m_bgTint.darker(420);
    }
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0, top);
    bg.setColorAt(1, bot);
    p.fillPath(bgPath, bg);

    // Cover used as a faint background wash
    if (!m_bgCoverScaled.isNull()) {
        p.setOpacity(0.12);
        p.drawPixmap(rect(), m_bgCoverScaled);
        p.setOpacity(1.0);
    }
    p.setClipping(false);

    // Border
    p.setPen(QPen(QColor(255, 255, 255, 15), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);

    int w = width();
    int h = height();

    // Left side: cover (vinyl disc or flat cover); coverSize computed above
    int coverX = 90;
    int coverY = (h - coverSize) / 2 - 20;

    // Cover shadow
    p.save();
    p.translate(coverX + coverSize / 2, coverY + coverSize / 2);
    p.setBrush(QColor(0, 0, 0, 80));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPoint(4, 4), coverSize / 2 + 2, coverSize / 2 + 2);
    p.restore();

    // Rotating cover (cached pixmap, no per-frame decode/scale)
    if (!m_coverPath.isEmpty()) {
        if (!m_discCoverScaled.isNull()) {
            if (m_coverMode) {
                // Flat cover mode: no rotation, rounded-rect cover art
                const int pad = 6;
                QPainterPath rp;
                rp.addRoundedRect(coverX + pad, coverY + pad, coverSize - pad * 2, coverSize - pad * 2, 14, 14);
                p.setClipPath(rp);
                p.drawPixmap(coverX + pad, coverY + pad, coverSize - pad * 2, coverSize - pad * 2,
                    m_discCoverScaled);
                p.setClipping(false);
            } else {
                p.save();
                p.translate(coverX + coverSize / 2, coverY + coverSize / 2);
                p.rotate(m_rotation);
                // Circular clip
                QPainterPath clip;
                clip.addEllipse(-coverSize / 2, -coverSize / 2, coverSize, coverSize);
                p.setClipPath(clip);
                p.drawPixmap(-coverSize / 2, -coverSize / 2, coverSize, coverSize,
                    m_discCoverScaled);
                p.restore();

                // Center hole
                p.save();
                p.translate(coverX + coverSize / 2, coverY + coverSize / 2);
                p.setBrush(QColor(20, 25, 35));
                p.setPen(QPen(QColor(102, 192, 244, 100), 2));
                p.drawEllipse(QPoint(0, 0), 18, 18);
                p.setBrush(QColor(102, 192, 244, 150));
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(0, 0), 5, 5);
                p.restore();
            }
        }
    } else {
        // Placeholder disc.
        //
        // A plain filled circle with a centred glyph is rotationally symmetric, so
        // the spin (and with it the pause ease-out) was invisible - which is what
        // every song looked like in a library without cover art. Grooves, an
        // asymmetric sheen, a radial seam and a per-song label give the rotation
        // something to show, and make switching tracks visible as well.
        const int hue = m_title.isEmpty() ? 205 : int(qHash(m_title) % 360);
        const QColor disc = QColor::fromHsl(hue, 130, 120);
        const QString label = m_title.isEmpty() ? QStringLiteral("♪") : m_title.left(2);

        p.save();
        p.translate(coverX + coverSize / 2, coverY + coverSize / 2);
        const qreal radius = coverSize / 2.0;
        if (!m_coverMode) p.rotate(m_rotation);

        if (m_coverMode) {
            // Flat mode: rounded square, no rotation
            p.setBrush(disc.darker(160));
            p.setPen(QPen(QColor(102, 192, 244, 80), 2));
            p.drawRoundedRect(QRectF(-radius + 6, -radius + 6, coverSize - 12, coverSize - 12), 14, 14);
        } else {
            // Vinyl: base disc, grooves, sheen, seam
            p.setBrush(disc.darker(150));
            p.setPen(QPen(QColor(102, 192, 244, 80), 2));
            p.drawEllipse(QPointF(0, 0), radius, radius);

            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(255, 255, 255, 20), 1));
            for (int r = int(coverSize * 0.30); r < int(radius) - 2; r += 6)
                p.drawEllipse(QPointF(0, 0), r, r);

            // Asymmetric highlight: this is the part that actually reads as spin
            QLinearGradient sheen(-radius, -radius, radius, radius);
            sheen.setColorAt(0.00, QColor(255, 255, 255, 0));
            sheen.setColorAt(0.42, QColor(255, 255, 255, 46));
            sheen.setColorAt(0.58, QColor(255, 255, 255, 46));
            sheen.setColorAt(1.00, QColor(255, 255, 255, 0));
            p.setBrush(sheen);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(0, 0), radius - 1, radius - 1);

            // Radial seam: a second, very legible rotation cue
            p.setPen(QPen(QColor(255, 255, 255, 55), 2));
            p.drawLine(QPointF(coverSize * 0.08, 0), QPointF(coverSize * 0.46, 0));
        }

        // Label with the song's initials, so switching tracks is visible too
        p.setPen(Qt::NoPen);
        p.setBrush(disc.darker(230));
        p.drawEllipse(QPointF(0, 0), coverSize * 0.20, coverSize * 0.20);
        p.setPen(QColor(232, 240, 250));
        QFont f = p.font();
        f.setPointSize(qMax(10, coverSize / 10));
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(-coverSize * 0.20, -coverSize * 0.20, coverSize * 0.40, coverSize * 0.40),
                   Qt::AlignCenter, label);
        p.restore();
    }

    // Right side: song info + lyrics
    int rightX = coverX + coverSize + 70;
    int rightW = w - rightX - 70;

    // Title
    p.setPen(QColor(255, 255, 255));
    QFont titleFont = p.font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(rightX, coverY, rightW, 44, Qt::AlignLeft | Qt::AlignVCenter,
        m_title.isEmpty() ? "未播放" : m_title);

    // Artist
    p.setPen(QColor(180, 190, 200));
    QFont artistFont = p.font();
    artistFont.setPointSize(14);
    artistFont.setBold(false);
    p.setFont(artistFont);
    p.drawText(rightX, coverY + 48, rightW, 28, Qt::AlignLeft | Qt::AlignVCenter,
        m_artist.isEmpty() ? "—" : m_artist);

    // Lyrics - show 5 lines (2 before + current + 2 after)
    if (!m_lyrics.isEmpty()) {
        int lyricY = coverY + 95;
        int lineH = 38;
        QFont lyricFont = p.font();
        lyricFont.setPointSize(14);
        p.setFont(lyricFont);

        // Record the interactive lyric area for click-to-seek
        m_lyricRect = QRect(rightX, lyricY, rightW, lineH * 5);

        for (int offset = -2; offset <= 2; ++offset) {
            int idx = m_currentLyricIndex + offset;
            if (idx < 0 || idx >= m_lyrics.size()) continue;
            QString text = m_lyrics[idx].second;
            if (offset == 0) {
                p.setPen(QColor(102, 192, 244));
                QFont boldFont = lyricFont; boldFont.setBold(true); boldFont.setPointSize(16);
                p.setFont(boldFont);
            } else if (qAbs(offset) == 1) {
                p.setPen(QColor(255, 255, 255, 160));
                p.setFont(lyricFont);
            } else {
                p.setPen(QColor(255, 255, 255, 80));
                p.setFont(lyricFont);
            }
            p.drawText(rightX, lyricY + (offset + 2) * lineH, rightW, lineH,
                Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    } else {
        m_lyricRect = QRect();
        p.setPen(QColor(255, 255, 255, 70));
        QFont lf = p.font(); lf.setPointSize(14); p.setFont(lf);
        p.drawText(rightX, coverY + 120, rightW, 36, Qt::AlignLeft, "暂无歌词");
    }

    // Position control bar at bottom
    QWidget *bar = findChild<QWidget*>("controlBar");
    if (bar) bar->setGeometry(0, h - 130, w, 130);
}

void FullScreenPlayer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit closed();
    } else if (event->key() == Qt::Key_Space) {
        emit playPauseClicked();
    } else if (event->key() == Qt::Key_Left) {
        emit prevClicked();
    } else if (event->key() == Qt::Key_Right) {
        emit nextClicked();
    }
    QWidget::keyPressEvent(event);
}

void FullScreenPlayer::mouseDoubleClickEvent(QMouseEvent *)
{
    emit closed();
}

void FullScreenPlayer::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->pos();
    const int h = height();
    const int coverSize = qMin(320, h - 220);
    const QRect coverRect(90, (h - coverSize) / 2 - 20, coverSize, coverSize);

    if (coverRect.contains(pos)) {
        // Click cover: toggle vinyl <-> flat cover mode
        m_coverMode = !m_coverMode;
        if (m_coverModeBtn) m_coverModeBtn->setIconType(m_coverMode ? NCIcons::Photo : NCIcons::Disc);
        emit coverModeToggled();
        updateRotateTimer(); // flat cover mode has nothing to spin
        update();
        event->accept();
        return;
    }

    // Click a lyric line: seek to that timestamp
    if (!m_lyrics.isEmpty() && !m_lyricRect.isEmpty() && m_lyricRect.contains(pos)) {
        const int lineH = m_lyricRect.height() / 5;
        const int rowInBlock = (pos.y() - m_lyricRect.top()) / lineH;
        const int idx = m_currentLyricIndex + (rowInBlock - 2);
        if (idx >= 0 && idx < m_lyrics.size()) {
            emit seekRequested(m_lyrics[idx].first);
            event->accept();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void FullScreenPlayer::wheelEvent(QWheelEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const int h = height();
    const int coverSize = qMin(320, h - 220);
    const QRect coverRect(90, (h - coverSize) / 2 - 20, coverSize, coverSize);
    if (coverRect.contains(pos)) {
        m_coverMode = !m_coverMode;
        if (m_coverModeBtn) m_coverModeBtn->setIconType(m_coverMode ? NCIcons::Photo : NCIcons::Disc);
        emit coverModeToggled();
        updateRotateTimer(); // flat cover mode has nothing to spin
        update();
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void FullScreenPlayer::onRotate()
{
    // Ease rotation speed toward target: 0.6°/30ms while playing (~20°/s = 360°/18s),
    // smoothly decelerate to a stop when paused instead of stopping instantly.
    const qreal target = m_playing ? 0.6 : 0.0;
    m_rotSpeed += (target - m_rotSpeed) * 0.06;
    if (qAbs(m_rotSpeed) > 0.004) {
        m_rotation += m_rotSpeed;
        if (m_rotation >= 360) m_rotation -= 360;
        if (m_rotation < 0) m_rotation += 360;
        update();
    } else {
        m_rotSpeed = 0;
    }
    updateRotateTimer(); // stop ticking once the disc has finished easing to a halt
}
