#ifndef PLAYERWIDGETS_H
#define PLAYERWIDGETS_H

// NetEase Cloud Music (网易云音乐) style player widgets, shared by the main
// player bar, the mini floating player and the full-screen playing page.
//
// Everything is painted with QPainter from a 24x24 design space, so no image
// assets are needed and icons stay crisp at any DPI. Colours follow the classic
// dark client: bar #2b2c2f, accent red #c20c0c, near-white icons that turn red
// on hover.

#include <QPushButton>
#include <QWidget>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QColor>
#include <QRectF>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QTreeWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QtGlobal>

#include "songinfo.h"

namespace NCIcons {

// Scaler: icon design space is 24x24 units mapped onto any target rect.
struct G {
    QPainter *p = nullptr;
    QRectF r;
    qreal X(qreal v) const { return r.left() + r.width() * v / 24.0; }
    qreal Y(qreal v) const { return r.top() + r.height() * v / 24.0; }
    qreal W(qreal v) const { return r.width() * v / 24.0; }
    qreal H(qreal v) const { return r.height() * v / 24.0; }
    QPointF P(qreal x, qreal y) const { return {X(x), Y(y)}; }
    QRectF R(qreal x, qreal y, qreal w, qreal h) const { return {X(x), Y(y), W(w), H(h)}; }
};

inline void applyPen(G &g, const QColor &c, qreal w, Qt::PenCapStyle cap = Qt::RoundCap,
                     Qt::PenJoinStyle join = Qt::RoundJoin)
{
    QPen pen(c);
    pen.setWidthF(g.W(w));
    pen.setCapStyle(cap);
    pen.setJoinStyle(join);
    g.p->setPen(pen);
    g.p->setBrush(Qt::NoBrush);
}

inline void fillPath(G &g, const QPainterPath &path, const QColor &c)
{
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(c);
    g.p->drawPath(path);
}

// ---- transport ----
inline void prev(G &g, const QColor &c)
{
    applyPen(g, c, 2.0);
    g.p->drawLine(g.P(4, 7), g.P(4, 17));
    QPainterPath t;
    t.moveTo(g.P(8.5, 12));
    t.lineTo(g.P(17.5, 6.3));
    t.lineTo(g.P(17.5, 17.7));
    t.closeSubpath();
    fillPath(g, t, c);
}

inline void next(G &g, const QColor &c)
{
    applyPen(g, c, 2.0);
    g.p->drawLine(g.P(20, 7), g.P(20, 17));
    QPainterPath t;
    t.moveTo(g.P(15.5, 12));
    t.lineTo(g.P(6.5, 6.3));
    t.lineTo(g.P(6.5, 17.7));
    t.closeSubpath();
    fillPath(g, t, c);
}

inline void play(G &g, const QColor &c)
{
    QPainterPath t;
    t.moveTo(g.P(8.2, 5.4));
    t.lineTo(g.P(19.2, 12));
    t.lineTo(g.P(8.2, 18.6));
    t.closeSubpath();
    fillPath(g, t, c);
}

inline void pause(G &g, const QColor &c)
{
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(c);
    g.p->drawRoundedRect(g.R(8.4, 5.6, 3.1, 12.8), g.H(1.6), g.H(1.6));
    g.p->drawRoundedRect(g.R(12.5, 5.6, 3.1, 12.8), g.H(1.6), g.H(1.6));
}

// ---- play modes ----
inline void repeat(G &g, const QColor &c)
{
    applyPen(g, c, 1.7);
    QPainterPath path;
    // bottom/left/right arcs approximated with the classic repeat polyline
    path.moveTo(g.P(3, 12));
    path.lineTo(g.P(3, 10));
    path.cubicTo(g.P(3, 7.6), g.P(4.8, 6), g.P(7, 6));
    path.lineTo(g.P(21, 6));
    g.p->drawPath(path);
    QPainterPath head;
    head.moveTo(g.P(17, 2));
    head.lineTo(g.P(21, 6));
    head.lineTo(g.P(17, 10));
    g.p->drawPath(head);

    QPainterPath path2;
    path2.moveTo(g.P(21, 12));
    path2.lineTo(g.P(21, 14));
    path2.cubicTo(g.P(21, 16.4), g.P(19.2, 18), g.P(17, 18));
    path2.lineTo(g.P(3, 18));
    g.p->drawPath(path2);
    QPainterPath head2;
    head2.moveTo(g.P(7, 22));
    head2.lineTo(g.P(3, 18));
    head2.lineTo(g.P(7, 14));
    g.p->drawPath(head2);
}

inline void repeatOne(G &g, const QColor &c)
{
    repeat(g, c);
    QFont f = g.p->font();
    f.setBold(true);
    f.setPixelSize(int(g.H(8.2)));
    g.p->setFont(f);
    g.p->setPen(c);
    g.p->drawText(g.R(6, 7.4, 12, 10), Qt::AlignCenter, QStringLiteral("1"));
}

inline void shuffle(G &g, const QColor &c)
{
    applyPen(g, c, 1.7);
    QPainterPath p1;
    p1.moveTo(g.P(16, 3));
    p1.lineTo(g.P(21, 3));
    p1.lineTo(g.P(21, 8));
    g.p->drawPath(p1);
    g.p->drawLine(g.P(4, 20), g.P(21, 3));
    QPainterPath p2;
    p2.moveTo(g.P(21, 16));
    p2.lineTo(g.P(21, 21));
    p2.lineTo(g.P(16, 21));
    g.p->drawPath(p2);
    g.p->drawLine(g.P(15, 15), g.P(21, 21));
    g.p->drawLine(g.P(4, 4), g.P(9, 9));
}

inline void sequential(G &g, const QColor &c)
{
    applyPen(g, c, 1.8);
    g.p->drawLine(g.P(4, 7), g.P(14, 7));
    g.p->drawLine(g.P(4, 12), g.P(18, 12));
    g.p->drawLine(g.P(4, 17), g.P(19, 17));
    QPainterPath head;
    head.moveTo(g.P(15, 13));
    head.lineTo(g.P(21, 17));
    head.lineTo(g.P(15, 21));
    g.p->drawPath(head);
}

// ---- playlist / queue document ----
inline void playlist(G &g, const QColor &c, const QColor &bg)
{
    applyPen(g, c, 1.6);
    g.p->drawRoundedRect(g.R(5, 2.6, 13, 16.6), g.W(1.8), g.W(1.8));
    g.p->drawLine(g.P(8.6, 7.6), g.P(14.6, 7.6));
    g.p->drawLine(g.P(8.6, 11), g.P(14.6, 11));
    g.p->drawLine(g.P(8.6, 14.4), g.P(12.4, 14.4));
    // play badge overlapping the bottom-left corner
    const QPointF ctr = g.P(6.4, 20.2);
    const qreal r = g.W(4.4);
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(bg);
    g.p->drawEllipse(ctr, r, r);
    QPen pen(c);
    pen.setWidthF(g.W(1.3));
    g.p->setPen(pen);
    g.p->setBrush(Qt::NoBrush);
    g.p->drawEllipse(ctr, r, r);
    QPainterPath tri;
    tri.moveTo(g.P(5.6, 18.8));
    tri.lineTo(g.P(5.6, 21.6));
    tri.lineTo(g.P(8.2, 20.2));
    tri.closeSubpath();
    fillPath(g, tri, c);
}

// ---- volume ----
inline void speaker(G &g, const QColor &c)
{
    QPainterPath s;
    s.moveTo(g.P(4, 9));
    s.lineTo(g.P(7.4, 9));
    s.lineTo(g.P(12.6, 4.4));
    s.lineTo(g.P(12.6, 19.6));
    s.lineTo(g.P(7.4, 15));
    s.lineTo(g.P(4, 15));
    s.closeSubpath();
    fillPath(g, s, c);
}

inline void volumeHigh(G &g, const QColor &c)
{
    speaker(g, c);
    applyPen(g, c, 1.6);
    QRectF a1 = g.R(12.5, 9.5, 5, 5);
    g.p->drawArc(a1, -55 * 16, 110 * 16);
    QRectF a2 = g.R(11.0, 7.2, 9.6, 9.6);
    g.p->drawArc(a2, -55 * 16, 110 * 16);
}

inline void volumeLow(G &g, const QColor &c)
{
    speaker(g, c);
    applyPen(g, c, 1.6);
    QRectF a1 = g.R(12.5, 9.5, 5, 5);
    g.p->drawArc(a1, -55 * 16, 110 * 16);
}

inline void mute(G &g, const QColor &c)
{
    speaker(g, c);
    applyPen(g, c, 1.8);
    g.p->drawLine(g.P(15, 9.5), g.P(20, 14.5));
    g.p->drawLine(g.P(20, 9.5), g.P(15, 14.5));
}

// ---- heart ----
inline QPainterPath heartPath(G &g)
{
    QPainterPath p;
    p.moveTo(g.P(12, 20.6));
    p.cubicTo(g.P(8.6, 17.9), g.P(3.6, 14.2), g.P(3.6, 9.1));
    p.cubicTo(g.P(3.6, 6.4), g.P(5.5, 4.6), g.P(8, 4.6));
    p.cubicTo(g.P(9.7, 4.6), g.P(11.1, 5.6), g.P(12, 7.1));
    p.cubicTo(g.P(12.9, 5.6), g.P(14.3, 4.6), g.P(16, 4.6));
    p.cubicTo(g.P(18.5, 4.6), g.P(20.4, 6.4), g.P(20.4, 9.1));
    p.cubicTo(g.P(20.4, 14.2), g.P(15.4, 17.9), g.P(12, 20.6));
    p.closeSubpath();
    return p;
}

inline void heart(G &g, const QColor &c, bool filled)
{
    QPainterPath p = heartPath(g);
    if (filled) {
        fillPath(g, p, c);
    } else {
        applyPen(g, c, 1.7);
        g.p->drawPath(p);
    }
}

// ---- mini / expand / trash / close ----
inline void mini(G &g, const QColor &c)
{
    applyPen(g, c, 1.7);
    g.p->drawRoundedRect(g.R(2.6, 4.6, 18.8, 14.8), g.W(2), g.W(2));
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(c);
    g.p->drawRoundedRect(g.R(12, 11, 7.4, 6), g.W(1), g.W(1));
}

inline void expand(G &g, const QColor &c)
{
    applyPen(g, c, 1.7);
    QPainterPath p;
    p.moveTo(g.P(8, 3)); p.lineTo(g.P(5, 3)); p.lineTo(g.P(3, 5)); p.lineTo(g.P(3, 8));
    g.p->drawPath(p);
    QPainterPath p2;
    p2.moveTo(g.P(16, 3)); p2.lineTo(g.P(19, 3)); p2.lineTo(g.P(21, 5)); p2.lineTo(g.P(21, 8));
    g.p->drawPath(p2);
    QPainterPath p3;
    p3.moveTo(g.P(8, 21)); p3.lineTo(g.P(5, 21)); p3.lineTo(g.P(3, 19)); p3.lineTo(g.P(3, 16));
    g.p->drawPath(p3);
    QPainterPath p4;
    p4.moveTo(g.P(16, 21)); p4.lineTo(g.P(19, 21)); p4.lineTo(g.P(21, 19)); p4.lineTo(g.P(21, 16));
    g.p->drawPath(p4);
}

inline void trash(G &g, const QColor &c)
{
    applyPen(g, c, 1.6);
    g.p->drawLine(g.P(3, 6), g.P(21, 6));
    QPainterPath p;
    p.moveTo(g.P(8, 6)); p.lineTo(g.P(8, 4)); p.lineTo(g.P(16, 4)); p.lineTo(g.P(16, 6));
    g.p->drawPath(p);
    QPainterPath b;
    b.moveTo(g.P(19, 6)); b.lineTo(g.P(18, 20)); b.lineTo(g.P(6, 20)); b.lineTo(g.P(5, 6));
    g.p->drawPath(b);
    g.p->drawLine(g.P(10, 10.5), g.P(10, 16));
    g.p->drawLine(g.P(14, 10.5), g.P(14, 16));
}

inline void close(G &g, const QColor &c)
{
    applyPen(g, c, 1.9);
    g.p->drawLine(g.P(5.5, 5.5), g.P(18.5, 18.5));
    g.p->drawLine(g.P(18.5, 5.5), g.P(5.5, 18.5));
}

// vinyl disc (黑胶)
inline void disc(G &g, const QColor &c)
{
    applyPen(g, c, 1.6);
    g.p->drawEllipse(g.R(3, 3, 18, 18));
    g.p->drawEllipse(g.R(8.6, 8.6, 6.8, 6.8));
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(c);
    g.p->drawEllipse(g.R(11.2, 11.2, 1.6, 1.6));
}

// flat cover / picture (封面)
inline void photo(G &g, const QColor &c)
{
    applyPen(g, c, 1.6);
    g.p->drawRoundedRect(g.R(3, 4.5, 18, 15), g.W(1.6), g.W(1.6));
    QPainterPath m;
    m.moveTo(g.P(5.5, 16.5));
    m.lineTo(g.P(9.5, 12.5));
    m.lineTo(g.P(12.5, 15.5));
    m.lineTo(g.P(15, 13));
    m.lineTo(g.P(18.5, 16.5));
    g.p->drawPath(m);
    g.p->setPen(Qt::NoPen);
    g.p->setBrush(c);
    g.p->drawEllipse(g.P(8.5, 8.5), g.W(1.3), g.W(1.3));
}

enum IconType {
    None, Prev, Next, Play, Pause, Repeat, RepeatOne, Shuffle, Sequential,
    Playlist, VolumeHigh, VolumeLow, Mute, Heart, HeartOutline, Mini, Expand,
    Trash, Close, Disc, Photo
};

inline void draw(IconType t, QPainter *painter, const QRectF &rect, const QColor &c,
                 const QColor &bg = QColor("#2b2c2f"))
{
    G g; g.p = painter; g.r = rect;
    painter->setRenderHint(QPainter::Antialiasing);
    switch (t) {
    case Prev: prev(g, c); break;
    case Next: next(g, c); break;
    case Play: play(g, c); break;
    case Pause: pause(g, c); break;
    case Repeat: repeat(g, c); break;
    case RepeatOne: repeatOne(g, c); break;
    case Shuffle: shuffle(g, c); break;
    case Sequential: sequential(g, c); break;
    case Playlist: playlist(g, c, bg); break;
    case VolumeHigh: volumeHigh(g, c); break;
    case VolumeLow: volumeLow(g, c); break;
    case Mute: mute(g, c); break;
    case Heart: heart(g, c, true); break;
    case HeartOutline: heart(g, c, false); break;
    case Mini: mini(g, c); break;
    case Expand: expand(g, c); break;
    case Trash: trash(g, c); break;
    case Close: close(g, c); break;
    case Disc: disc(g, c); break;
    case Photo: photo(g, c); break;
    case None: break;
    }
}

} // namespace NCIcons

// Flat icon button: near-white glyph that turns red on hover.
class NCButton : public QPushButton {
    Q_OBJECT
public:
    explicit NCButton(NCIcons::IconType icon, QWidget *parent = nullptr)
        : QPushButton(parent), m_icon(icon)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFlat(true);
    }
    void setIconType(NCIcons::IconType icon) { m_icon = icon; update(); }
    NCIcons::IconType iconType() const { return m_icon; }
    void setBaseColor(const QColor &c) { m_base = c; update(); }
    void setBgColor(const QColor &c) { m_bg = c; update(); }
    void setPadding(int px) { m_pad = px; update(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor c = m_base;
        if (isChecked() || underMouse()) c = QColor("#c20c0c");
        NCIcons::draw(m_icon, &p, rect().adjusted(m_pad, m_pad, -m_pad, -m_pad), c, m_bg);
    }
    void enterEvent(QEnterEvent *e) override { QPushButton::enterEvent(e); update(); }
    void leaveEvent(QEvent *e) override { QPushButton::leaveEvent(e); update(); }

private:
    NCIcons::IconType m_icon;
    QColor m_base = QColor("#dcdcdc");
    QColor m_bg = QColor("#2b2c2f");
    int m_pad = 4;
};

// Prev/next transport: thin circular ring + glyph, red on hover.
class CircleTransportButton : public QPushButton {
    Q_OBJECT
public:
    CircleTransportButton(bool isPrev, QWidget *parent = nullptr)
        : QPushButton(parent), m_prev(isPrev)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(34, 34);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = underMouse() ? QColor("#c20c0c") : QColor("#d2d2d2");
        QRectF r = rect().adjusted(2.6, 2.6, -2.6, -2.6);
        QPen pen(c);
        pen.setWidthF(width() * 1.5 / 34.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
        NCIcons::draw(m_prev ? NCIcons::Prev : NCIcons::Next, &p,
                      r.adjusted(3.5, 3.5, -3.5, -3.5), c);
    }
    void enterEvent(QEnterEvent *e) override { QPushButton::enterEvent(e); update(); }
    void leaveEvent(QEvent *e) override { QPushButton::leaveEvent(e); update(); }

private:
    bool m_prev;
};

// Queue button: playlist document glyph + count, inside a subtle dark pill.
class QueueButton : public QPushButton {
    Q_OBJECT
public:
    explicit QueueButton(QWidget *parent = nullptr) : QPushButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setCheckable(true);
        setFixedHeight(30);
        setCount(0);
    }
    void setCount(int n)
    {
        m_count = n;
        const QString s = QString::number(n);
        setFixedWidth(30 + 7 * s.size() + 18);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor pill = isChecked() ? QColor(20, 20, 24, 235)
                                        : QColor(0, 0, 0, underMouse() ? 70 : 40);
        p.setPen(Qt::NoPen);
        p.setBrush(pill);
        p.drawRoundedRect(rect().adjusted(1, 2, -1, -2), 6, 6);

        const QColor c = (underMouse() || isChecked()) ? QColor("#c20c0c")
                                                       : QColor("#d6d6d6");
        QRectF iconR(8, 5, 20, height() - 10);
        NCIcons::draw(NCIcons::Playlist, &p, iconR, c, pill);

        QFont f = p.font();
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(c);
        QRectF tr(iconR.right() + 3, 0, width() - iconR.right() - 10, height());
        p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, QString::number(m_count));
    }

private:
    int m_count = 0;
};

// Circular "词" desktop-lyrics toggle, red when active/hover.
class LyricsToggleButton : public QPushButton {
    Q_OBJECT
public:
    explicit LyricsToggleButton(QWidget *parent = nullptr) : QPushButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setCheckable(true);
        setFixedSize(33, 33);
        setToolTip(QStringLiteral("歌词"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = (isChecked() || underMouse()) ? QColor("#c20c0c")
                                                       : QColor("#b8b8b8");
        QRectF r = rect().adjusted(2.5, 2.5, -2.5, -2.5);
        QPen pen(c);
        pen.setWidthF(1.6);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(r);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(13);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("词"));
    }

private:
    bool m_prevHover = false;
};

// Thin NetEase-style slider for seek / volume. The handle (white ring with a red
// centre) only appears on hover or while dragging. Non-live mode (seek) reports
// the final value through seekFinished; live mode (volume) emits valueChanged
// for every movement.
class ThinSlider : public QWidget {
    Q_OBJECT
public:
    explicit ThinSlider(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumHeight(18);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setRange(int lo, int hi) { m_lo = lo; m_hi = hi; update(); }
    void setValue(int v)
    {
        m_val = qBound(m_lo, v, m_hi);
        update();
    }
    int value() const { return m_val; }
    void setFillColor(const QColor &c) { m_fill = c; update(); }
    void setLive(bool live) { m_live = live; }
    void setTrackColor(const QColor &c) { m_track = c; update(); }
    bool isSliding() const { return m_pressed; }

signals:
    void valueChanged(int v);
    void seekFinished(int v);

protected:
    void enterEvent(QEnterEvent *e) override { QWidget::enterEvent(e); update(); }
    void leaveEvent(QEvent *e) override { QWidget::leaveEvent(e); update(); }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        m_pressed = true;
        updateFromX(e->position().x());
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_pressed) updateFromX(e->position().x());
        else update();
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (!m_pressed) return;
        m_pressed = false;
        if (!m_live) emit seekFinished(m_val);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal margin = 7;
        const qreal cy = height() / 2.0;
        const bool hot = underMouse() || m_pressed;
        const qreal th = hot ? 4.5 : 3.2;
        const qreal x0 = margin, x1 = width() - margin;
        // track
        p.setPen(Qt::NoPen);
        p.setBrush(m_track);
        p.drawRoundedRect(QRectF(x0, cy - th / 2, x1 - x0, th), th / 2, th / 2);
        // fill
        const qreal frac = (m_hi > m_lo) ? qreal(m_val - m_lo) / (m_hi - m_lo) : 0.0;
        const qreal fx = x0 + (x1 - x0) * frac;
        if (fx > x0) {
            p.setBrush(m_fill);
            p.drawRoundedRect(QRectF(x0, cy - th / 2, fx - x0, th), th / 2, th / 2);
        }
        // handle
        if (hot) {
            p.setBrush(QColor("#f2f2f2"));
            p.drawEllipse(QPointF(fx, cy), 5.6, 5.6);
            p.setBrush(m_fill);
            p.drawEllipse(QPointF(fx, cy), 2.7, 2.7);
        }
    }

private:
    void updateFromX(qreal x)
    {
        const qreal margin = 7;
        const qreal x0 = margin, x1 = width() - margin;
        qreal frac = (x - x0) / (x1 - x0);
        frac = qBound(0.0, frac, 1.0);
        m_val = m_lo + qRound((m_hi - m_lo) * frac);
        update();
        emit valueChanged(m_val); // live apply (volume) or preview (seek)
    }

    int m_lo = 0, m_hi = 1000, m_val = 0;
    bool m_pressed = false, m_live = false;
    QColor m_fill = QColor("#c20c0c");
    QColor m_track = QColor(10, 10, 12, 120);
};

// Play-queue panel that opens above the player bar (NetEase style).
class QueuePanel : public QFrame {
    Q_OBJECT
public:
    explicit QueuePanel(QWidget *parent = nullptr) : QFrame(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        setStyleSheet(QStringLiteral(
            "QueuePanel { background: #2b2c2f; border: 1px solid #3a3b40; border-radius: 8px; }"));
        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->setContentsMargins(12, 10, 12, 10);
        lay->setSpacing(8);

        QHBoxLayout *header = new QHBoxLayout;
        header->setSpacing(8);
        QLabel *title = new QLabel(QStringLiteral("播放队列"));
        QFont tf = title->font();
        tf.setBold(true);
        tf.setPixelSize(14);
        title->setFont(tf);
        title->setStyleSheet("color: #ececec;");
        m_countLabel = new QLabel;
        m_countLabel->setStyleSheet("color: #8a8a8a; font-size: 12px;");
        header->addWidget(title);
        header->addWidget(m_countLabel);
        header->addStretch();

        auto mkBtn = [this](NCIcons::IconType ic, const QString &tip) {
            NCButton *b = new NCButton(ic, this);
            b->setFixedSize(27, 27);
            b->setPadding(5);
            b->setToolTip(tip);
            return b;
        };
        m_miniBtn = mkBtn(NCIcons::Mini, QStringLiteral("迷你模式"));
        m_modeBtn = mkBtn(NCIcons::Repeat, QStringLiteral("播放模式"));
        m_clearBtn = mkBtn(NCIcons::Trash, QStringLiteral("清空播放列表"));
        m_closeBtn = mkBtn(NCIcons::Close, QStringLiteral("关闭"));
        header->addWidget(m_miniBtn);
        header->addWidget(m_modeBtn);
        header->addWidget(m_clearBtn);
        header->addWidget(m_closeBtn);
        lay->addLayout(header);

        m_tree = new QTreeWidget;
        m_tree->setColumnCount(2);
        m_tree->setRootIsDecorated(false);
        m_tree->header()->setVisible(false);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setMouseTracking(true);
        m_tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_tree->setStyleSheet(QStringLiteral(R"(
            QTreeWidget { background: transparent; border: none; outline: none; font-size: 12.5px; }
            QTreeWidget::item { color: #c4c4c4; padding: 6px 4px; border-bottom: 1px solid rgba(255,255,255,0.05); }
            QTreeWidget::item:hover { background: #34353b; }
            QTreeWidget::item:selected { background: #34353b; color: #ffffff; }
        )"));
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        lay->addWidget(m_tree, 1);

        connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it) {
            if (it) emit playRequested(it->data(0, Qt::UserRole).toInt());
        });
        connect(m_closeBtn, &QPushButton::clicked, this, &QueuePanel::closed);
        connect(m_modeBtn, &QPushButton::clicked, this, &QueuePanel::modeRequested);
        connect(m_clearBtn, &QPushButton::clicked, this, &QueuePanel::clearRequested);
        connect(m_miniBtn, &QPushButton::clicked, this, &QueuePanel::miniRequested);
    }

    void setModeIcon(NCIcons::IconType t) { m_modeBtn->setIconType(t); }

    void refresh(const QList<SongInfo> &songs, int currentIndex)
    {
        int scrollPos = m_tree->verticalScrollBar()->value();
        m_tree->clear();
        for (int i = 0; i < songs.size(); ++i) {
            const SongInfo &s = songs[i];
            const bool cur = (i == currentIndex);
            QString left;
            if (cur) left = QStringLiteral("♪  ") + s.title + QStringLiteral(" - ") + s.artist;
            else left = QString::number(i + 1).rightJustified(2, ' ') + QStringLiteral(".  ")
                        + s.title + QStringLiteral(" - ") + s.artist;
            QTreeWidgetItem *it = new QTreeWidgetItem(m_tree, QStringList()
                << left << (s.duration > 0 ? formatMs(s.duration) : QString()));
            it->setData(0, Qt::UserRole, i);
            QBrush b(cur ? QColor("#c20c0c") : QColor(i % 2 ? "#c4c4c4" : "#b6b6b6"));
            it->setForeground(0, b);
            it->setForeground(1, cur ? QColor("#c20c0c") : QColor("#7d7d7d"));
            QFont f = it->font(0);
            f.setBold(cur);
            it->setFont(0, f);
            if (cur) m_tree->setCurrentItem(it);
        }
        m_countLabel->setText(QStringLiteral("(%1 首)").arg(songs.size()));
        m_tree->verticalScrollBar()->setValue(scrollPos);
    }

signals:
    void playRequested(int index);
    void modeRequested();
    void clearRequested();
    void miniRequested();
    void closed();

private:
    static QString formatMs(qint64 ms)
    {
        qint64 sec = ms / 1000;
        return QStringLiteral("%1:%2")
            .arg(sec / 60, 2, 10, QChar('0'))
            .arg(sec % 60, 2, 10, QChar('0'));
    }

    QTreeWidget *m_tree;
    QLabel *m_countLabel;
    NCButton *m_miniBtn, *m_modeBtn, *m_clearBtn, *m_closeBtn;
};

#endif // PLAYERWIDGETS_H
