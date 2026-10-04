#ifndef PLAYPAUSEBUTTON_H
#define PLAYPAUSEBUTTON_H

#include <QPushButton>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QRectF>
#include <QEnterEvent>

// Play/pause button with a 200ms icon morph animation
class PlayPauseButton : public QPushButton {
    Q_OBJECT
public:
    explicit PlayPauseButton(QWidget *parent = nullptr) : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(200);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_progress = v.toReal();
            update();
        });
    }
    void setPlaying(bool playing) {
        if (m_playing == playing) return;
        m_playing = playing;
        m_anim->stop();
        m_anim->setStartValue(m_progress);
        m_anim->setEndValue(m_playing ? 1.0 : 0.0);
        m_anim->start();
    }
    void setPlayingInstant(bool playing) {
        m_playing = playing;
        m_progress = playing ? 1.0 : 0.0;
        m_anim->stop();
        update();
    }
    void setColors(const QColor &bg, const QColor &fg) { m_bg = bg; m_fg = fg; update(); }
    // NetEase main-bar style: a stroked ring with a morphing glyph (no fill),
    // turning red on hover.
    void setOutlineStyle(bool on) { m_outline = on; update(); }

protected:
    void enterEvent(QEnterEvent *e) override { QPushButton::enterEvent(e); m_hover = true; update(); }
    void leaveEvent(QEvent *e) override { QPushButton::leaveEvent(e); m_hover = false; update(); }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = qMin(width(), height());
        const qreal cx = width() / 2.0, cy = height() / 2.0;

        if (m_outline) {
            const QColor c = m_hover ? QColor("#c20c0c") : QColor("#f0f0f0");
            QPen ring(c);
            ring.setWidthF(s * 0.065);
            p.setPen(ring);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QRectF(cx - s * 0.44, cy - s * 0.44, s * 0.88, s * 0.88));
            paintMorph(&p, c, s, cx, cy);
            return;
        }

        // Background circle
        p.setBrush(m_bg);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(cx - s / 2, cy - s / 2, s, s));
        paintMorph(&p, m_fg, s, cx, cy);
    }

    void paintMorph(QPainter *painter, const QColor &fg, qreal s, qreal cx, qreal cy) {
        QPainter &p = *painter;
        // Icon morph: t = 0 -> play triangle, t = 1 -> pause bars
        const qreal t = m_progress;
        const qreal r = s * 0.34;
        if (t < 1.0) {
            QPainterPath play;
            play.moveTo(cx - r * 0.35, cy - r);
            play.lineTo(cx - r * 0.35, cy + r);
            play.lineTo(cx + r * 1.05, cy);
            play.closeSubpath();
            p.setOpacity(1.0 - t);
            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawPath(play);
            p.setOpacity(1.0);
        }
        if (t > 0.0) {
            const qreal w = s * 0.11 * (0.3 + 0.7 * t);
            p.setOpacity(t);
            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawRoundedRect(QRectF(cx - r * 0.68, cy - r, w, r * 2), w / 2, w / 2);
            p.drawRoundedRect(QRectF(cx + r * 0.68 - w, cy - r, w, r * 2), w / 2, w / 2);
            p.setOpacity(1.0);
        }
    }

private:
    bool m_playing = false;
    qreal m_progress = 0.0;
    QVariantAnimation *m_anim;
    QColor m_bg = QColor(42, 71, 94);
    QColor m_fg = QColor(102, 192, 244);
    bool m_outline = false;
    bool m_hover = false;
};

#endif
