#include "traycontroller.h"
#include "traymenustyle.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QPixmap>
#include <QStyle>
#include <QStyleHints>
#include <QTimer>
#include <QGuiApplication>

TrayController::TrayController(Hooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks))
{}

QIcon TrayController::resolveIcon() const
{
    QIcon icon = m_hooks.windowIcon ? m_hooks.windowIcon() : QIcon();
    if (icon.isNull()) {
        QPixmap pm(32, 32);
        pm.fill(QColor(102, 192, 244));
        icon = QIcon(pm);
    }
    return icon;
}

void TrayController::setup()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("系统托盘不可用");
        return;
    }

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(resolveIcon());
    m_tray->setToolTip("里与由 工作区");

    m_tray->setContextMenu(buildMenu());

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if ((reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            && m_hooks.toggleWindow) {
            m_hooks.toggleWindow();
        }
    });

    m_tray->show();
    m_initialized = true;
    qDebug("托盘图标已创建");
}

QMenu *TrayController::buildMenu()
{
    if (m_menu)
        return m_menu;

    m_menu = new QMenu();

    // ---- header: app icon + name + version -------------------------------
    auto *headerAction = new QWidgetAction(m_menu);
    auto *headerWidget = new QWidget;
    headerWidget->setFixedHeight(38);
    headerWidget->setStyleSheet("background: transparent;");
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 4, 12, 4);
    headerLayout->setSpacing(8);

    auto *headerIcon = new QLabel;
    headerIcon->setFixedSize(20, 20);
    const QPixmap headerPm = resolveIcon().pixmap(20, 20);
    if (!headerPm.isNull())
        headerIcon->setPixmap(headerPm);
    headerLayout->addWidget(headerIcon);

    m_headerTitle = new QLabel("里与由 工作区");
    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();

#ifdef APP_VERSION
    m_headerVersion = new QLabel(QStringLiteral("v" APP_VERSION));
#else
    m_headerVersion = new QLabel;
#endif
    headerLayout->addWidget(m_headerVersion);

    headerAction->setDefaultWidget(headerWidget);
    m_menu->addAction(headerAction);
    m_menu->addSeparator();

    // ---- window actions ---------------------------------------------------
    m_showAct = m_menu->addAction("显示主窗口");
    m_showAct->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(m_showAct, &QAction::triggered, this, [this]() { if (m_hooks.showWindow) m_hooks.showWindow(); });

    m_hideAct = m_menu->addAction("隐藏到托盘");
    m_hideAct->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(m_hideAct, &QAction::triggered, this, [this]() { if (m_hooks.hideWindow) m_hooks.hideWindow(); });

    m_menu->addSeparator();

    // ---- app actions ------------------------------------------------------
    m_settingsAct = m_menu->addAction("偏好设置...");
    m_settingsAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(m_settingsAct, &QAction::triggered, this, [this]() {
        if (m_hooks.showWindow) m_hooks.showWindow();
        QTimer::singleShot(100, this, [this]() { if (m_hooks.openSettings) m_hooks.openSettings(); });
    });

    m_accountAct = m_menu->addAction("账户信息");
    connect(m_accountAct, &QAction::triggered, this, [this]() {
        if (m_hooks.showWindow) m_hooks.showWindow();
        if (m_hooks.showAccount) m_hooks.showAccount();
    });

    m_menu->addSeparator();

    m_quitAct = m_menu->addAction("退出");
    m_quitAct->setShortcut(QKeySequence("Ctrl+Q"));
    connect(m_quitAct, &QAction::triggered, this, [this]() {
        m_initialized = false;
        if (m_hooks.quit) m_hooks.quit();
    });

    // ---- theme ------------------------------------------------------------
    // Re-check the OS scheme every time the menu opens (cheap: five 32px
    // pixmaps + one stylesheet) so a theme flip while the app sits in the
    // tray is picked up even if the change signal was missed.
    connect(m_menu, &QMenu::aboutToShow, this, [this]() {
        applyTheme(traymenu::systemPrefersDark());
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme scheme) {
        const bool dark = (scheme == Qt::ColorScheme::Dark) ||
                          (scheme == Qt::ColorScheme::Unknown && traymenu::systemPrefersDark());
        applyTheme(dark);
    });
#endif
    applyTheme(traymenu::systemPrefersDark());

    return m_menu;
}

void TrayController::applyTheme(bool dark)
{
    if (!m_menu)
        return;

    const traymenu::Palette pal = traymenu::palette(dark);
    m_menu->setStyleSheet(traymenu::menuStyleSheet(pal));

    using K = traymenu::IconKind;
    if (m_showAct)     m_showAct->setIcon(traymenu::icon(K::ShowWindow, pal));
    if (m_hideAct)     m_hideAct->setIcon(traymenu::icon(K::HideToTray, pal));
    if (m_settingsAct) m_settingsAct->setIcon(traymenu::icon(K::Settings, pal));
    if (m_accountAct)  m_accountAct->setIcon(traymenu::icon(K::Account, pal));
    if (m_quitAct)     m_quitAct->setIcon(traymenu::icon(K::Quit, pal));

    if (m_headerTitle)
        m_headerTitle->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 13px; font-weight: 600;").arg(pal.accent.name()));
    if (m_headerVersion)
        m_headerVersion->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px;").arg(pal.textDim.name()));
}

bool TrayController::handleCloseRequest(const QString &closeBehavior)
{
    if (closeBehavior == "tray" && m_initialized &&
        QSystemTrayIcon::isSystemTrayAvailable()) {
        if (m_hooks.hideWindow) m_hooks.hideWindow();
        if (m_tray) {
            m_tray->showMessage("里与由 工作区",
                                "程序已最小化到托盘，点击托盘图标恢复。",
                                QSystemTrayIcon::Information, 3000);
        }
        return true;  // intercepted
    }
    return false;
}
