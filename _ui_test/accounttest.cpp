// UI test for the account dialog's cover-background and dialog-chrome controls.
//
// Covers the things a user actually sees when the dialog talks back:
//   * the "restore default background" entry (missing before: no way back to
//     the plain white canvas from the UI once a cover was set)
//   * every popup the dialog raises carries the program name in its title
//     ("里与由 - <主题>") and shows Chinese button text, matching the login
//     window's "里与由 - 登录" convention
// Runs with QT_QPA_PLATFORM=offscreen against LIYYYOU_DATA_DIR, so a real
// window is never shown and the real account store is never touched.

#include "accountdialog.h"
#include "userauth.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>

static int g_failures = 0;

static void check(bool ok, const QString &what)
{
    qInfo().noquote() << (ok ? "  PASS  " : "  FAIL  ") << what;
    if (!ok) ++g_failures;
}

// ---- modal dialog capture -------------------------------------------------
// Offscreen there is no human to click the popups onSave/onLogout raise, so a
// ticking timer watches for the active modal QMessageBox/QDialog, records
// its title and button texts, then dismisses it: clicking the button named by
// g_clickText when one is set (miss => close + flag), plain close otherwise.
// close() is the safe fallback: for the logout question it means "not
// confirmed", never an accidental sign-out.
struct BoxRecord
{
    QString title;
    QStringList buttons;
};
static QList<BoxRecord> g_boxLog;
static QString g_clickText;
static bool g_clickMissed = false;
static QPointer<QWidget> g_handled;

static void recordAndDismiss(QWidget *w, const QList<QAbstractButton*> &buttons)
{
    if (g_handled.data() == w) return;
    g_handled = w;
    BoxRecord r;
    r.title = w->windowTitle();
    for (QAbstractButton *b : buttons) r.buttons << b->text();
    g_boxLog << r;
    if (!g_clickText.isEmpty()) {
        const QString want = g_clickText;
        g_clickText.clear();
        for (QAbstractButton *b : buttons) {
            if (b->text() == want) { b->click(); return; }
        }
        g_clickMissed = true;
    }
    w->close();
}

static void installDialogRecorder()
{
    QTimer *t = new QTimer(qApp);
    QObject::connect(t, &QTimer::timeout, qApp, [] {
        QWidget *w = QApplication::activeModalWidget();
        if (!w) return;
        if (QMessageBox *box = qobject_cast<QMessageBox *>(w)) {
            recordAndDismiss(box, box->buttons());
        } else if (QDialog *in = qobject_cast<QDialog *>(w)) {
            recordAndDismiss(in, in->findChildren<QAbstractButton *>());
        }
    });
    t->start(10);
}

// The recorder usually records during the button click's nested exec() loop,
// so waiting is expressed against an absolute expected count, not a baseline
// captured after the fact (which would already include the record).
static bool waitForBoxCount(int target, int timeoutMs = 3000)
{
    QElapsedTimer et;
    et.start();
    while (et.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (g_boxLog.size() >= target) return true;
    }
    return g_boxLog.size() >= target;
}

static const BoxRecord &lastBox() { return g_boxLog.last(); }

// ---- fixtures --------------------------------------------------------------
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
    // Same identity the real app sets in main.cpp; the dialog must take its
    // popup titles from this, not from a hard-coded string.
    app.setApplicationName(QString::fromUtf8("里与由"));
    installDialogRecorder();

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

    // ---- [6] save popup carries the app name and a Chinese button ---------
    qInfo().noquote() << "\n[6] save popup: app-name title, Chinese confirm";
    {
        AccountDialog dlg(&auth);
        QPushButton *save = dlg.findChild<QPushButton *>(QStringLiteral("greenBtn"));
        g_boxLog.clear();
        save->click();
        check(waitForBoxCount(1), "saving raised a popup");
        if (!g_boxLog.isEmpty()) {
            check(lastBox().title == QString::fromUtf8("里与由 - 保存成功"),
                  "title is 里与由 - 保存成功, got: " + lastBox().title);
            check(lastBox().buttons.contains(QString::fromUtf8("确定")),
                  "confirm button reads 确定, got: " + lastBox().buttons.join("|"));
        }
    }

    // ---- [7] logout popup: app-name title, Chinese buttons, cancel keeps
    //         the session ----------------------------------------------------
    qInfo().noquote() << "\n[7] logout popup: app-name title, 退出/取消, cancel is safe";
    {
        AccountDialog dlg(&auth);
        QPushButton *logout = dlg.findChild<QPushButton *>(QStringLiteral("redBtn"));
        g_boxLog.clear();
        g_clickMissed = false;
        g_clickText = QString::fromUtf8("取消");
        logout->click();
        check(waitForBoxCount(1), "logout raised a popup");
        if (!g_boxLog.isEmpty()) {
            check(lastBox().title == QString::fromUtf8("里与由 - 退出登录"),
                  "title is 里与由 - 退出登录, got: " + lastBox().title);
            check(lastBox().buttons.contains(QString::fromUtf8("退出"))
                      && lastBox().buttons.contains(QString::fromUtf8("取消")),
                  "buttons read 退出/取消, got: " + lastBox().buttons.join("|"));
        }
        check(!g_clickMissed, "the 取消 button was actually found and clicked");
        check(auth.isLoggedIn(), "canceling the popup keeps the user logged in");
    }

    // ---- [8] password prompt: app-name title, Chinese buttons, cancel is a
    //         no-op ----------------------------------------------------------
    qInfo().noquote() << "\n[8] password prompt: app-name title, Chinese buttons";
    {
        AccountDialog dlg(&auth);
        QPushButton *pwd = dlg.findChild<QPushButton *>(QStringLiteral("grayBtn"));
        g_boxLog.clear();
        g_clickMissed = false;
        g_clickText = QString::fromUtf8("取消");
        pwd->click();
        check(waitForBoxCount(1), "changing password raised an input popup");
        if (!g_boxLog.isEmpty()) {
            check(lastBox().title == QString::fromUtf8("里与由 - 修改密码"),
                  "title is 里与由 - 修改密码, got: " + lastBox().title);
            check(lastBox().buttons.contains(QString::fromUtf8("确定"))
                      && lastBox().buttons.contains(QString::fromUtf8("取消")),
                  "buttons read 确定/取消, got: " + lastBox().buttons.join("|"));
        }
        check(!g_clickMissed, "the 取消 button was actually found and clicked");
        check(auth.isLoggedIn(), "canceling the prompt changed nothing");
    }

    // ---- [9] confirming logout with the 退出 button signs out -------------
    qInfo().noquote() << "\n[9] logout popup: 退出 signs out";
    {
        AccountDialog dlg(&auth);
        QPushButton *logout = dlg.findChild<QPushButton *>(QStringLiteral("redBtn"));
        g_boxLog.clear();
        g_clickMissed = false;
        g_clickText = QString::fromUtf8("退出");
        logout->click();
        check(waitForBoxCount(1), "logout raised a popup");
        check(!g_clickMissed, "the 退出 button was actually found and clicked");
        check(!auth.isLoggedIn(), "clicking 退出 logs the user out");
    }

    qInfo().noquote() << (g_failures == 0 ? "\nALL CHECKS PASSED"
                                          : QString("\n%1 CHECK(S) FAILED").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}
