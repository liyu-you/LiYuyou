// Tray menu UI test.
//
//   1. Palette sanity: text/accent/dim colors meet WCAG contrast ratios
//      against their backgrounds in BOTH light and dark schemes.
//   2. Stylesheet actually embeds the palette colors.
//   3. Every hand-drawn icon is non-null and its hovered (Active) variant
//      really differs from the normal one.
//   4. TrayController builds the full menu without a system tray (offscreen
//      platform), with plain-text actions carrying themed icons.
//   5. Renders the menu in both schemes into one side-by-side preview PNG
//      (path given as argv[1], defaults to ./tray_preview.png).
//
// Exit code 0 = all checks passed.

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QMenu>
#include <QPainter>

#include "traycontroller.h"
#include "traymenustyle.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            qWarning("FAIL: %s  (line %d)", #cond, __LINE__);              \
        } else {                                                           \
            qInfo("ok: %s", #cond);                                        \
        }                                                                  \
    } while (0)

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    using namespace traymenu;

    // ---- 1. palette contrast (WCAG AA: 4.5 text / 3.0 graphics) ----------
    for (bool dark : { true, false }) {
        const Palette p = palette(dark);
        qInfo("%s palette:", dark ? "dark " : "light");
        CHECK(contrastRatio(p.text, p.bg) >= 7.0);          // AAA for body text
        CHECK(contrastRatio(p.textDim, p.bg) >= 3.0);
        CHECK(contrastRatio(p.accent, p.bg) >= 3.0);        // icons = graphics
        CHECK(contrastRatio(p.accent, p.hoverBg) >= 3.0);   // hovered icon
        CHECK(contrastRatio(p.text, p.hoverBg) >= 4.5);     // safety: text on hover
    }
    CHECK(paletteDark().bg != paletteLight().bg);
    CHECK(paletteDark().accent != paletteLight().accent);

    // ---- 2. stylesheet embeds the palette --------------------------------
    for (bool dark : { true, false }) {
        const Palette p = palette(dark);
        const QString css = menuStyleSheet(p);
        CHECK(css.contains(p.bg.name()));
        CHECK(css.contains(p.border.name()));
        CHECK(css.contains(p.hoverBg.name()));
        CHECK(css.contains(p.accent.name()));
        CHECK(css.contains(p.separator.name()));
    }

    // ---- 3. icons ---------------------------------------------------------
    for (IconKind k : { IconKind::ShowWindow, IconKind::HideToTray,
                        IconKind::Settings, IconKind::Account, IconKind::Quit }) {
        const QIcon ic = icon(k, paletteDark());
        CHECK(!ic.isNull());
        const QImage normal = ic.pixmap(32, 32, QIcon::Normal).toImage();
        const QImage active = ic.pixmap(32, 32, QIcon::Active).toImage();
        CHECK(!normal.isNull());
        CHECK(normal != active);   // hover recolor really happens
    }

    // ---- 4. controller builds the menu without a tray --------------------
    TrayController::Hooks hooks;
    // Empty callbacks: actions must be safe. windowIcon is provided so the
    // preview shows a realistic header icon instead of the fallback square.
    hooks.windowIcon = [] {
        QPixmap pm(64, 64);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor("#66c0f4"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
        p.setPen(QPen(QColor("#16202d"), 5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(20, 32), QPointF(32, 44));
        p.drawLine(QPointF(32, 44), QPointF(46, 20));
        return QIcon(pm);
    };
    TrayController ctl(hooks);
    // setup() must no-op gracefully where no tray exists; on platforms that
    // DO have a tray we skip it so the test never flashes a real tray icon.
    if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
        ctl.setup();
        CHECK(!ctl.initialized());
    } else {
        qInfo("(setup() no-op check skipped on '%s' — would create a real tray icon)",
              qPrintable(QGuiApplication::platformName()));
    }

    QMenu *menu = ctl.buildMenu();
    CHECK(menu != nullptr);
    CHECK(ctl.buildMenu() == menu);          // idempotent
    CHECK(menu->actions().size() >= 8);      // header + 3 seps + 5 actions
    int withIcon = 0;
    for (QAction *a : menu->actions())
        if (!a->isSeparator() && !a->icon().isNull())
            ++withIcon;
    CHECK(withIcon == 5);
    // no leftover emoji glyphs in action texts
    for (QAction *a : menu->actions())
        CHECK(!a->text().contains(QChar(0xD83D)));  // high surrogate of 📂📥👤🚪

    // ---- 5. visual previews ----------------------------------------------
    const QString outPath = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                       : QStringLiteral("tray_preview.png");

    auto grabMenu = [&](bool dark) -> QPixmap {
        ctl.applyTheme(dark);
        menu->ensurePolished();
        menu->resize(menu->sizeHint());
        const qreal dpr = 2.0;
        QPixmap shot(QSizeF(menu->size()).toSize() * dpr);
        shot.setDevicePixelRatio(dpr);
        shot.fill(Qt::transparent);
        menu->render(&shot);
        return shot;
    };

    const QPixmap darkShot  = grabMenu(true);
    const QPixmap lightShot = grabMenu(false);
    CHECK(!darkShot.isNull());
    CHECK(!lightShot.isNull());

    const qreal dpr = darkShot.devicePixelRatio();
    const QSize item = darkShot.size() / dpr;
    const int pad = 26, labelH = 30, gap = 20;
    QPixmap board(QSize(item.width() * 2 + pad * 2 + gap,
                        item.height() + pad * 2 + labelH) * dpr);
    board.setDevicePixelRatio(dpr);
    board.fill(QColor("#20262d"));
    QPainter pt(&board);
    pt.setRenderHint(QPainter::Antialiasing);
    // backdrops hint at the surrounding OS tone
    pt.fillRect(QRect(pad, pad + labelH, item.width(), item.height()), QColor("#10161d"));
    pt.fillRect(QRect(pad + item.width() + gap, pad + labelH,
                      item.width(), item.height()), QColor("#e4e9ee"));
    pt.setPen(QColor("#9fb2c5"));
    QFont f = pt.font(); f.setPixelSize(14); f.setBold(true); pt.setFont(f);
    pt.drawText(QRect(pad, pad, item.width(), labelH),
                Qt::AlignLeft | Qt::AlignVCenter, "深色主题 (Dark)");
    pt.drawText(QRect(pad + item.width() + gap, pad, item.width(), labelH),
                Qt::AlignLeft | Qt::AlignVCenter, "浅色主题 (Light)");
    pt.drawPixmap(pad, pad + labelH, darkShot);
    pt.drawPixmap(pad + item.width() + gap, pad + labelH, lightShot);
    pt.end();

    CHECK(board.save(outPath));
    qInfo("preview written to %s", qPrintable(QDir(outPath).absolutePath()));

    if (g_failures == 0) {
        qInfo("ALL TRAY TESTS PASSED");
        return 0;
    }
    qWarning("%d CHECK(S) FAILED", g_failures);
    return 1;
}
