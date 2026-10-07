#ifndef TRAYMENUSTYLE_H
#define TRAYMENUSTYLE_H

// Visual design of the system-tray context menu: light/dark palettes that
// follow the OS theme, the QMenu stylesheet, and the hand-drawn monochrome
// line icons (the old emoji glyphs rendered inconsistently across Windows
// versions — some colored, some as grey boxes).
//
// Header-only on purpose: the app (traycontroller.cpp) and the standalone
// tray UI test (_tray_test) share this single source of truth, and no .cpp
// means no CMake changes for either side.

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>
#include <QStyleHints>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace traymenu {

struct Palette {
    QColor bg;        // menu background
    QColor border;    // outer 1px border
    QColor text;      // item text / normal icon stroke
    QColor textDim;   // secondary text (header version, disabled items)
    QColor accent;    // brand blue — title, hovered text, hovered icon stroke
    QColor hoverBg;   // hovered item background
    QColor separator;
};

// Refined version of the original Steam-like dark theme (#16202d family),
// kept close to the shipped look so existing users recognize it.
inline Palette paletteDark()
{
    return { QColor("#1b2837"), QColor("#33475c"), QColor("#d8e4ee"),
             QColor("#8296a9"), QColor("#66c0f4"), QColor("#2a475e"),
             QColor("#2c4055") };
}

inline Palette paletteLight()
{
    return { QColor("#fbfcfd"), QColor("#d7dfe7"), QColor("#2c3a47"),
             QColor("#7d8d9c"), QColor("#0f6cb5"), QColor("#e9f2f9"),
             QColor("#e3e9ef") };
}

inline Palette palette(bool dark)
{
    return dark ? paletteDark() : paletteLight();
}

// OS theme detection. Qt 6.5+ reports the real Windows/macOS color scheme;
// older toolkits fall back to judging the default window color.
inline bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)  return true;
    if (scheme == Qt::ColorScheme::Light) return false;
#endif
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

inline QString menuStyleSheet(const Palette &p)
{
    return QStringLiteral(
        "QMenu {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 10px;"
        "  padding: 5px;"
        "  font-size: 13px;"
        "}"
        "QMenu::item {"
        "  color: %3;"
        "  background: transparent;"
        "  padding: 7px 26px 7px 10px;"
        "  border-radius: 6px;"
        "  margin: 1px 4px;"
        "}"
        "QMenu::item:selected { background: %4; color: %5; }"
        "QMenu::item:disabled { color: %6; }"
        "QMenu::separator { height: 1px; background: %7; margin: 6px 12px; }"
        "QMenu::icon { padding-left: 8px; }")
        .arg(p.bg.name(), p.border.name(), p.text.name(), p.hoverBg.name(),
             p.accent.name(), p.textDim.name(), p.separator.name());
}

// ---- contrast helpers (the tray test enforces WCAG ratios with these) ----

inline double relativeLuminance(const QColor &c)
{
    auto linearize = [](double v) {
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearize(c.redF())
         + 0.7152 * linearize(c.greenF())
         + 0.0722 * linearize(c.blueF());
}

inline double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// ---- icons ---------------------------------------------------------------

enum class IconKind { ShowWindow, HideToTray, Settings, Account, Quit };

// Stroke-only line icons drawn in a 16x16 logical grid, rendered at 2x so
// they stay crisp on high-DPI displays. All shapes share one pen so the set
// reads as a family.
inline QPixmap drawIcon(IconKind kind, const QColor &color,
                        int logical = 16, qreal dpr = 2.0)
{
    constexpr double kPi = 3.14159265358979323846;

    QPixmap pm(QSize(qRound(logical * dpr), qRound(logical * dpr)));
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);

    QPainter pt(&pm);
    pt.setRenderHint(QPainter::Antialiasing);
    pt.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    pt.setBrush(Qt::NoBrush);

    switch (kind) {
    case IconKind::ShowWindow:
        // app window: rounded body + title-bar divider
        pt.drawRoundedRect(QRectF(2.6, 3.0, 10.8, 10.0), 1.6, 1.6);
        pt.drawLine(QPointF(2.6, 6.1), QPointF(13.4, 6.1));
        break;
    case IconKind::HideToTray: {
        // down-arrow onto a baseline ("drop it into the tray")
        pt.drawLine(QPointF(8.0, 2.6), QPointF(8.0, 9.8));
        QPainterPath head;
        head.moveTo(5.1, 7.3);
        head.lineTo(8.0, 10.2);
        head.lineTo(10.9, 7.3);
        pt.drawPath(head);
        pt.drawLine(QPointF(3.4, 13.3), QPointF(12.6, 13.3));
        break;
    }
    case IconKind::Settings:
        // gear: hub + 8 spokes
        pt.drawEllipse(QPointF(8.0, 8.0), 2.3, 2.3);
        for (int i = 0; i < 8; ++i) {
            const double a = i * kPi / 4.0;
            pt.drawLine(QPointF(8.0 + std::cos(a) * 4.0, 8.0 + std::sin(a) * 4.0),
                        QPointF(8.0 + std::cos(a) * 5.6, 8.0 + std::sin(a) * 5.6));
        }
        break;
    case IconKind::Account:
        // head + shoulders
        pt.drawEllipse(QPointF(8.0, 5.8), 2.4, 2.4);
        pt.drawArc(QRectF(3.2, 9.6, 9.6, 8.0), 0, 180 * 16);
        break;
    case IconKind::Quit:
        // power symbol: near-full ring with a gap at top + stem
        pt.drawArc(QRectF(3.6, 4.6, 8.8, 8.8), 125 * 16, 290 * 16);
        pt.drawLine(QPointF(8.0, 2.4), QPointF(8.0, 7.0));
        break;
    }
    return pm;
}

// Normal state uses the text color; the Active (hovered) state switches to
// the accent color, matching QMenu::item:selected in the stylesheet.
inline QIcon icon(IconKind kind, const Palette &p, qreal dpr = 2.0)
{
    QIcon ic;
    ic.addPixmap(drawIcon(kind, p.text, 16, dpr), QIcon::Normal, QIcon::Off);
    ic.addPixmap(drawIcon(kind, p.accent, 16, dpr), QIcon::Active, QIcon::Off);
    return ic;
}

} // namespace traymenu

#endif // TRAYMENUSTYLE_H
