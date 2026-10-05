#include "mainwindow.h"
#include "userauth.h"
#include "logindialog.h"
#include "logger.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QIcon>
#include <QSettings>
#include <QTranslator>
#include <QDir>
#include <QMessageBox>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QEventLoop>
#include <QDateTime>
#include <QRandomGenerator>
#include <QPushButton>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QFile>
#include <QSharedMemory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Steam-style loading window
class SteamLoadingWindow : public QWidget {
public:
    SteamLoadingWindow(const UserInfo &user, QWidget *parent = nullptr)
        : QWidget(parent), m_user(user), m_rotation(0)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        resize(860, 560);

        m_startTime = QDateTime::currentMSecsSinceEpoch();

        // Close button (top-right X)
        QPushButton *closeBtn = new QPushButton("×", this);
        closeBtn->setGeometry(815, 8, 36, 36);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(R"(
            QPushButton { background: transparent; color: rgba(255,255,255,160); border: none;
                font-size: 22px; font-weight: bold; border-radius: 18px; }
            QPushButton:hover { background: rgba(255,255,255,25); color: #fff; }
        )");
        connect(closeBtn, &QPushButton::clicked, this, &SteamLoadingWindow::close);

        // Animation timer - keeps requesting repaints, but angle is time-based
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            update();
        });
        m_timer->start(16); // ~60fps target

        // Progress auto-increment timer — smooth random growth, never stuck
        m_progressTimer = new QTimer(this);
        connect(m_progressTimer, &QTimer::timeout, this, [this]() {
            if (m_progress < 95) {
                // Random increment between 0.5 and 2.5 percent
                qreal inc = 0.5 + (QRandomGenerator::global()->bounded(20)) / 10.0;
                m_progress = qMin(95.0, m_progress + inc);
                update();
            }
        });
        m_progressTimer->start(50);
    }

    void finishLoading() {
        m_progressTimer->stop();
        m_progress = 100;
        update();
        m_timer->stop();
        // Brief flash of 100% then close
        QTimer::singleShot(80, this, &QWidget::close);
    }

    void setProgress(int percent, const QString &text = QString()) {
        m_progress = qBound(0.0, (qreal)percent, 100.0);
        if (!text.isEmpty()) m_statusText = text;
        update();
    }

    void setStatus(const QString &text) {
        m_statusText = text;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        int w = width(), h = height();

        // Rounded rect clip
        QPainterPath path;
        path.addRoundedRect(0, 0, w, h, 12, 12);
        p.setClipPath(path);

        // Background: user cover image or dark gradient
        if (!m_user.backgroundPath.isEmpty() && QFile::exists(m_user.backgroundPath)) {
            QPixmap bg(m_user.backgroundPath);
            bg = bg.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            p.drawPixmap(0, 0, bg);
            // Dark overlay for readability
            p.fillRect(0, 0, w, h, QColor(10, 15, 25, 170));
        } else {
            QLinearGradient grad(0, 0, w, h);
            grad.setColorAt(0, QColor("#1b2838"));
            grad.setColorAt(0.5, QColor("#16202d"));
            grad.setColorAt(1, QColor("#0f1923"));
            p.fillRect(0, 0, w, h, grad);
        }

        // ===== Center: circular avatar with rotating progress ring =====
        // Absolute fixed center position (qreal for sub-pixel precision)
        const qreal cx = w / 2.0;
        const qreal cy = h / 2.0 - 50.0;
        const qreal ringOuter = 66.0;
        const qreal ringOuter2 = 56.0;
        const qreal ringInner = 50.0;
        const int avatarSize = 84;

        // Static background ring (track) — absolute position
        p.setBrush(QColor(255, 255, 255, 12));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(cx - ringOuter, cy - ringOuter, ringOuter * 2, ringOuter * 2));

        // Main rotating arc ring (smooth gradient: bright head -> fading tail)
        // Time-based rotation: keeps correct angle even when main thread is blocked
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startTime;
        qreal rot = fmod(elapsed * 0.25, 360.0); // 0.25 deg/ms = 250 deg/sec
        p.save();
        p.translate(QPointF(cx, cy)); // absolute center
        p.rotate(rot);
        QConicalGradient conGrad(0, 0, 0);
        conGrad.setColorAt(0.00, QColor("#8fd4ff"));   // head: brightest
        conGrad.setColorAt(0.06, QColor("#66c0f4"));   // bright blue
        conGrad.setColorAt(0.15, QColor(102, 192, 244, 160)); // mid
        conGrad.setColorAt(0.24, QColor(102, 192, 244, 40));  // fading
        conGrad.setColorAt(0.30, Qt::transparent);       // tail: gone
        conGrad.setColorAt(1.00, Qt::transparent);
        p.setBrush(conGrad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(-ringOuter, -ringOuter, ringOuter * 2, ringOuter * 2));
        p.restore();

        // Second rotating thin ring (same speed, opposite position — stays 180° apart)
        p.save();
        p.translate(QPointF(cx, cy)); // same absolute center as outer ring
        p.rotate(fmod(rot, 360.0)); // same speed as outer ring, never catches up
        QConicalGradient conGrad2(0, 0, 180); // start at bottom (180° offset = opposite side)
        conGrad2.setColorAt(0.00, QColor("#66c0f4"));
        conGrad2.setColorAt(0.05, QColor(102, 192, 244, 180));
        conGrad2.setColorAt(0.12, QColor(102, 192, 244, 60));
        conGrad2.setColorAt(0.20, Qt::transparent);
        conGrad2.setColorAt(1.00, Qt::transparent);
        p.setBrush(conGrad2);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(-ringOuter2, -ringOuter2, ringOuter2 * 2, ringOuter2 * 2));
        p.restore();

        // Inner circle (cuts out center to make ring effect) — absolute position
        p.setBrush(QColor(18, 28, 40, 230));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(cx - ringInner, cy - ringInner, ringInner * 2, ringInner * 2));

        // Outer ring subtle border — absolute position
        p.setPen(QPen(QColor(102, 192, 244, 60), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(cx - ringOuter, cy - ringOuter, ringOuter * 2, ringOuter * 2));

        // Avatar (circular)
        QPixmap avatarPix;
        if (!m_user.avatarPath.isEmpty() && QFile::exists(m_user.avatarPath)) {
            avatarPix = QPixmap(m_user.avatarPath);
        } else {
            avatarPix = QPixmap(avatarSize, avatarSize);
            avatarPix.fill(QColor("#2a475e"));
            QPainter ap(&avatarPix);
            ap.setPen(QColor("#66c0f4"));
            QFont af = ap.font();
            af.setPointSize(36);
            af.setBold(true);
            ap.setFont(af);
            ap.drawText(avatarPix.rect(), Qt::AlignCenter,
                         (m_user.nickname.isEmpty() ? m_user.username : m_user.nickname).left(1).toUpper());
        }
        QPixmap circularAvatar(avatarSize, avatarSize);
        circularAvatar.fill(Qt::transparent);
        QPainter cp(&circularAvatar);
        cp.setRenderHint(QPainter::Antialiasing);
        cp.setRenderHint(QPainter::SmoothPixmapTransform);
        cp.setBrush(QBrush(avatarPix.scaled(avatarSize, avatarSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
        cp.setPen(Qt::NoPen);
        cp.drawEllipse(0, 0, avatarSize, avatarSize);
        p.drawPixmap(QRectF(cx - avatarSize / 2.0, cy - avatarSize / 2.0, avatarSize, avatarSize),
                     circularAvatar, QRectF(0, 0, avatarSize, avatarSize));

        // Loading status text
        p.setPen(QColor("#c7d5e0"));
        QFont textFont("Microsoft YaHei", 15);
        p.setFont(textFont);
        p.drawText(QRect(0, cy + ringOuter + 12, w, 28), Qt::AlignCenter, m_statusText);

        // Progress bar
        int barW = 320, barH = 6;
        int barX = (w - barW) / 2;
        int barY = cy + ringOuter + 48;
        // Track
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 40));
        p.drawRoundedRect(barX, barY, barW, barH, 3, 3);
        // Fill
        if (m_progress > 0) {
            qreal fillW = barW * m_progress / 100.0;
            QLinearGradient barGrad(barX, 0, barX + fillW, 0);
            barGrad.setColorAt(0, QColor("#66c0f4"));
            barGrad.setColorAt(1, QColor("#4a9fd4"));
            p.setBrush(barGrad);
            p.drawRoundedRect(QRectF(barX, barY, fillW, barH), 3, 3);
        }
        // Percent text
        p.setPen(QColor("#8f98a0"));
        QFont pctFont("Microsoft YaHei", 11);
        p.setFont(pctFont);
        p.drawText(QRect(0, barY + 12, w, 20), Qt::AlignCenter, QString("%1%").arg((int)m_progress));

        // ===== Bottom: user info =====
        int bottomY = h - 100;
        QString displayName = m_user.nickname.isEmpty() ? m_user.username : m_user.nickname;

        // Small avatar (circular, 32x32)
        QPixmap smallAvatar(32, 32);
        if (!m_user.avatarPath.isEmpty() && QFile::exists(m_user.avatarPath)) {
            smallAvatar = QPixmap(m_user.avatarPath).scaled(32, 32, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        } else {
            smallAvatar.fill(QColor("#2a475e"));
            QPainter sap(&smallAvatar);
            sap.setPen(QColor("#66c0f4"));
            QFont sf = sap.font();
            sf.setPointSize(14);
            sf.setBold(true);
            sap.setFont(sf);
            sap.drawText(smallAvatar.rect(), Qt::AlignCenter, displayName.left(1).toUpper());
        }
        QPixmap smallCircular(32, 32);
        smallCircular.fill(Qt::transparent);
        QPainter scp(&smallCircular);
        scp.setRenderHint(QPainter::Antialiasing);
        scp.setRenderHint(QPainter::SmoothPixmapTransform);
        scp.setBrush(QBrush(smallAvatar));
        scp.setPen(Qt::NoPen);
        scp.drawEllipse(0, 0, 32, 32);
        int userInfoWidth = 32 + 8 + p.fontMetrics().horizontalAdvance(displayName);
        int userInfoX = (w - userInfoWidth) / 2;
        p.drawPixmap(userInfoX, bottomY, smallCircular);

        // Username
        p.setPen(QColor("#c7d5e0"));
        QFont userFont;
        userFont.setFamily("Microsoft YaHei");
        userFont.setPixelSize(14);
        p.setFont(userFont);
        p.drawText(QRect(userInfoX + 40, bottomY - 2, userInfoWidth - 40, 36),
                   Qt::AlignVCenter | Qt::AlignLeft, displayName);
    }

private:
    UserInfo m_user;
    QTimer *m_timer;
    QTimer *m_progressTimer;
    int m_rotation;
    qreal m_progress = 0.0;
    QString m_statusText = "正在初始化...";
    qint64 m_startTime = 0;
};

int main(int argc, char *argv[])
{
    // Migrate old settings (PureRef/Canvas) to new location (PureRefCanvas/里与由)
    {
        QSettings oldSettings("PureRef", "Canvas");
        QSettings newSettings("PureRefCanvas", "里与由");
        if (newSettings.allKeys().isEmpty() && !oldSettings.allKeys().isEmpty()) {
            for (const QString &key : oldSettings.allKeys())
                newSettings.setValue(key, oldSettings.value(key));
        }
    }

    // Read settings that must be applied before QApplication is created
    QSettings settings("PureRefCanvas", "里与由");

    // UI scale factor
    int uiScale = settings.value("uiScale", 100).toInt();
    if (uiScale != 100 && uiScale > 0) {
        qputenv("QT_SCALE_FACTOR", QString::number(uiScale / 100.0, 'f', 2).toUtf8());
    }

    // Hardware acceleration
    bool hwAccel = settings.value("hardwareAcceleration", true).toBool();
    if (!hwAccel) {
        QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    } else {
        // Force hardware-accelerated rendering pipeline.
        // - AA_ShareOpenGLContexts: lets QOpenGLWidget / RHI share one GL context
        // - AA_UseDesktopOpenGL: use native desktop OpenGL (better perf than ANGLE on Win10/11)
        // - QT_ANGLE_PLATFORM=d3d11: fallback via Direct3D 11 if desktop GL missing
        QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
        qputenv("QT_OPENGL", "desktop");
        qputenv("QT_ANGLE_PLATFORM", "d3d11");
        qputenv("QT_D3D_ADAPTER_INDEX", "0");

        // 4x multisample antialiasing + double buffer for smoother scrolling.
        QSurfaceFormat fmt;
        fmt.setRenderableType(QSurfaceFormat::OpenGL);
        fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        fmt.setSamples(4);
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication app(argc, argv);
    app.setApplicationName("里与由");
    app.setOrganizationName("PureRefCanvas");
    app.setWindowIcon(QIcon(":/app.ico"));

    // Install file logger as early as possible so we capture startup warnings.
    Logger::install(settings.value("logging/enabled", true).toBool());
    qDebug() << "=== 里与由 v1.0.0 starting ===";

    // Force Fusion style: on Windows, the native Windows style draws QToolTip
    // as a system-native dark popup that ignores stylesheet AND palette.
    // Fusion paints everything in Qt, so our QToolTip QSS actually takes effect.
    app.setStyle("Fusion");

    // Global Google Material tooltip style (QToolTip is a top-level widget,
    // widget-level stylesheets can't reach it — must be set on QApplication).
    // Use both stylesheet AND palette: on Windows, QToolTip sometimes ignores
    // stylesheet background and falls back to the system tooltip color, so we
    // force the palette too.
    app.setStyleSheet(R"(
        QToolTip {
            background-color: #ffffff;
            color: #202124;
            border: 1px solid #dadce0;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }
    )");
    {
        QPalette tipPal = QApplication::palette("QToolTip");
        tipPal.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
        tipPal.setColor(QPalette::ToolTipText, QColor("#202124"));
        tipPal.setColor(QPalette::Window, QColor("#ffffff"));
        tipPal.setColor(QPalette::WindowText, QColor("#202124"));
        tipPal.setColor(QPalette::Base, QColor("#ffffff"));
        tipPal.setColor(QPalette::Text, QColor("#202124"));
        QApplication::setPalette(tipPal, "QToolTip");
    }

    // Single instance check
    bool singleInstance = settings.value("singleInstance", false).toBool();
    if (singleInstance) {
        QSharedMemory *sharedMem = new QSharedMemory("LiYuYou_SingleInstance_Mutex");
        if (!sharedMem->create(1)) {
            // Another instance is running - activate it and exit
#ifdef Q_OS_WIN
            HWND hwnd = FindWindowW(nullptr, L"里与由");
            if (hwnd) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
            }
#endif
            delete sharedMem;
            return 0;
        }
        // Keep sharedMem alive for the lifetime of the app (leak intentionally)
    }

    // Language
    QString language = settings.value("language", "zh").toString();
    QTranslator translator;
    if (language == "en") {
        if (translator.load(":/translations/pureref_en.qm")) {
            app.installTranslator(&translator);
        }
    }

    // User authentication
    UserAuth auth;
    LoginDialog loginDlg(&auth);
    if (loginDlg.exec() != QDialog::Accepted) {
        return 0;
    }

    // ===== Steam-style loading window (shows immediately after login) =====
    SteamLoadingWindow loading(auth.currentUser());
    loading.show();
    loading.raise();
    loading.setProgress(0, "正在初始化...");
    app.processEvents();

    // Brief preload to let the loading window render
    QEventLoop preloadLoop;
    QTimer preloadTimer;
    preloadTimer.setSingleShot(true);
    QObject::connect(&preloadTimer, &QTimer::timeout, &preloadLoop, &QEventLoop::quit);
    preloadTimer.start(30);
    preloadLoop.exec();

    // Create main window (fast now — heavy work is deferred)
    loading.setStatus("正在加载工作区...");
    app.processEvents();
    MainWindow w;
    w.hide();  // force-hidden during construction — nothing should show the window yet
    w.setCurrentUser(auth.currentUser());
    w.setUserAuth(&auth);
    app.processEvents();
    qDebug() << "[BOOT] after MainWindow ctor, isVisible=" << w.isVisible();

    // Show main window and close loading
    loading.setStatus("正在准备界面...");
    app.processEvents();

    // Finish the loading animation BEFORE showing the main window. Otherwise
    // the half-rendered main window leaks through behind the frameless loading
    // splash (which is only stays-on-top, not a true modal).
    qDebug() << "[BOOT] before finishLoading, w.isVisible =" << w.isVisible();
    loading.finishLoading();
    qDebug() << "[BOOT] after finishLoading, w.isVisible =" << w.isVisible();
    app.processEvents();
    qDebug() << "[BOOT] after processEvents, w.isVisible =" << w.isVisible();
    QTimer::singleShot(150, &app, [&w]() {
        qDebug() << "[BOOT] timer firing, w.isVisible =" << w.isVisible();
        w.show();
        qDebug() << "[BOOT] after w.show(), w.isVisible =" << w.isVisible();
        w.raise();
        w.activateWindow();
    });

    // Re-assert QToolTip style AFTER MainWindow's setStyleSheet has run.
    // QToolTip is a top-level popup that inherits palette from QWidget, and
    // MainWindow's blanket `QWidget { background: ... }` was winning the cascade.
    // Appending this last makes it the final word in the QSS chain.
    app.setStyleSheet(app.styleSheet() + R"(
        QToolTip {
            background-color: #ffffff !important;
            color: #202124 !important;
            border: 1px solid #dadce0 !important;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }
    )");

    // Handle logout: hide main window, show login dialog again
    QObject::connect(&w, &MainWindow::loggedOut, &app, [&w, &auth]() {
        w.hide();
        LoginDialog reloginDlg(&auth);
        if (reloginDlg.exec() == QDialog::Accepted) {
            w.setCurrentUser(auth.currentUser());
            w.show();
            w.raise();
            w.activateWindow();
        } else {
            QApplication::quit();
        }
    });

    return app.exec();
}
