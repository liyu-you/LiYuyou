// UI test for the account dialog's cover-background controls.
//
// Covers the "restore default background" entry that the dialog was missing:
// once a cover was set there was no way back to the plain white canvas from
// the UI. Runs with QT_QPA_PLATFORM=offscreen against LIYIYOU_DATA_DIR, so a
// real window is never shown and the real account store is never touched.
//
// Expected production behaviour asserted here:
//   * the cover area exposes a reset entry (QPushButton#resetCoverBtn)
//   * with a cover applied the entry is enabled; clicking it returns the
//     dialog to the plain white canvas immediately
//   * saving persists the cleared accountCoverPath and leaves the startup
//     background (backgroundPath) alone
//   * reopening the dialog shows the blank canvas again
//   * without a cover the entry exists but is disabled (restore is a no-op)

#include "accountdialog.h"
#include "userauth.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>

static int g_failures = 0;

static void check(bool ok, const QString &what)
{
    qInfo().noquote() << (ok ? "  PASS  " : "  FAIL  ") << what;
    if (!ok) ++g_failures;
}

// onSave() pops a modal information box; offscreen there is no human to click
// it, so a ticking timer auto-accepts any modal QMessageBox.
static void installAutoDialogCloser()
{
    QTimer *t = new QTimer(qApp);
    QObject::connect(t, &QTimer::timeout, qApp, [] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(w)) box->accept();
        }
    });
    t->start(20);
}

static bool isPlainWhite(const AccountDialog &dlg)
{
    const QBrush b = dlg.palette().brush(QPalette::Window);
    return b.style() == Qt::SolidPattern && b.color() == QColor(Qt::white);
}

static QPushButton *resetBtn(const AccountDialog &dlg)
{
    return dlg.findChild<QPushButton *>(QStringLiteral("resetCoverBtn"));
}

static QString makeCoverFile(const QString &dir)
{
    const QString path = dir + "/assets/covers/cover.png";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QPixmap pix(200, 120);
    pix.fill(QColor(200, 30, 30));
    pix.save(path, "PNG");
    return path;
}

static QString storeText(const QString &dir)
{
    QFile f(dir + "/users.json");
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    installAutoDialogCloser();

    const QString dir = qEnvironmentVariable("LIYIYOU_TEST_DIR",
                                             QDir::tempPath() + "/liyiyou_account_test");
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    qputenv("LIYIYOU_DATA_DIR", dir.toLocal8Bit());
    qInfo().noquote() << "scratch store:" << dir;

    const QString coverPath = makeCoverFile(dir);

    // ---- [1] entry exists and reflects the cover state --------------------
    qInfo().noquote() << "\n[1] reset entry exists on the cover area";
    UserAuth auth;
    QString error;
    check(auth.registerUser("erin", "erin@example.com", "Passw0rd1", "q", "a", &error),
          "register erin (" + error + ")");
    check(auth.login("erin", "Passw0rd1", &error), "login erin");

    {
        AccountDialog noCover(&auth);
        QPushButton *btn = resetBtn(noCover);
        check(btn != nullptr, "reset entry exists without a cover");
        if (btn) check(!btn->isEnabled(), "reset entry disabled when there is no cover");
    }

    // ---- [2] with a saved cover the dialog shows it and the entry is live -
    qInfo().noquote() << "\n[2] cover applied: entry enabled, canvas not white";
    check(auth.updateAccountCover("erin", coverPath, &error), "seed a cover (" + error + ")");
    check(auth.updateBackground("erin", "C:/fake/splash.png", &error),
          "seed a startup background (" + error + ")");
    {
        AccountDialog withCover(&auth);
        QPushButton *btn = resetBtn(withCover);
        check(btn != nullptr, "reset entry exists with a cover");
        if (btn) check(btn->isEnabled(), "reset entry enabled when a cover is shown");
        check(!isPlainWhite(withCover), "cover canvas is not plain white");
    }

    // ---- [3] clicking reset returns to the white canvas immediately -------
    qInfo().noquote() << "\n[3] clicking reset restores the plain canvas";
    {
        AccountDialog dlg(&auth);
        QPushButton *btn = resetBtn(dlg);
        if (btn) {
            btn->click();
            check(isPlainWhite(dlg), "canvas is plain white right after the reset click");
        } else {
            check(false, "canvas is plain white right after the reset click (no entry to click)");
        }
    }

    // ---- [4] saving persists the cleared cover, startup bg untouched ------
    qInfo().noquote() << "\n[4] save commits the reset";
    {
        AccountDialog dlg(&auth);
        QPushButton *btn = resetBtn(dlg);
        QPushButton *save = dlg.findChild<QPushButton *>(QStringLiteral("greenBtn"));
        if (btn && save) {
            btn->click();
            save->click();
            check(auth.currentUser().accountCoverPath.isEmpty(), "cover path cleared on disk state");
            check(auth.currentUser().backgroundPath == "C:/fake/splash.png",
                  "startup background kept");
            check(!storeText(dir).contains("cover.png"), "users.json no longer references the cover");
            check(storeText(dir).contains("splash.png"), "users.json keeps the startup background");
        } else {
            check(false, "save commits the reset (reset or save entry missing)");
        }
    }

    // ---- [5] reopening shows the blank canvas again -----------------------
    qInfo().noquote() << "\n[5] blank canvas survives a reopen";
    {
        UserAuth reloaded;
        check(reloaded.login("erin", "Passw0rd1", &error), "re-login erin");
        AccountDialog dlg(&reloaded);
        check(isPlainWhite(dlg), "reopened dialog is plain white");
        QPushButton *btn = resetBtn(dlg);
        if (btn) check(!btn->isEnabled(), "reset entry disabled again after the cover is gone");
    }

    qInfo().noquote() << (g_failures == 0 ? "\nALL CHECKS PASSED"
                                          : QString("\n%1 CHECK(S) FAILED").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}
