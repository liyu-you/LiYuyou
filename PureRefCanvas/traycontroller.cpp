#include "traycontroller.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QWidgetAction>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QStyle>
#include <QTimer>
#include <QApplication>

TrayController::TrayController(Hooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks))
{}

void TrayController::setup()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("系统托盘不可用");
        return;
    }

    m_tray = new QSystemTrayIcon(this);

    QIcon icon = m_hooks.windowIcon ? m_hooks.windowIcon() : QIcon();
    if (icon.isNull()) {
        QPixmap pm(32, 32);
        pm.fill(QColor(102, 192, 244));
        icon = QIcon(pm);
    }
    m_tray->setIcon(icon);
    m_tray->setToolTip("里与由 工作区");

    QMenu *trayMenu = new QMenu();
    trayMenu->setStyleSheet(R"(
        QMenu {
            background: #16202d;
            color: #c7d5e0;
            border: 1px solid #2a475e;
            border-radius: 8px;
            padding: 6px;
            font-size: 13px;
        }
        QMenu::item {
            padding: 8px 28px 8px 12px;
            border-radius: 5px;
            margin: 1px 2px;
        }
        QMenu::item:selected {
            background: #2a475e;
            color: #66c0f4;
        }
        QMenu::separator {
            height: 1px;
            background: #2a475e;
            margin: 5px 10px;
        }
        QMenu::icon {
            padding-left: 8px;
        }
    )");

    // Header
    QWidgetAction *headerAction = new QWidgetAction(trayMenu);
    QWidget *headerWidget = new QWidget;
    headerWidget->setFixedHeight(36);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(12, 4, 12, 4);
    QLabel *headerIcon = new QLabel;
    headerIcon->setFixedSize(20, 20);
    QPixmap headerPm = icon.pixmap(20, 20);
    if (!headerPm.isNull()) headerIcon->setPixmap(headerPm);
    headerLayout->addWidget(headerIcon);
    QLabel *headerLabel = new QLabel("里与由 工作区");
    headerLabel->setStyleSheet("color: #66c0f4; font-size: 13px; font-weight: 600;");
    headerLayout->addWidget(headerLabel);
    headerLayout->addStretch();
    headerAction->setDefaultWidget(headerWidget);
    trayMenu->addAction(headerAction);
    trayMenu->addSeparator();

    QAction *showAct = trayMenu->addAction("📂  显示主窗口");
    showAct->setShortcut(QKeySequence("Ctrl+Shift+W"));
    connect(showAct, &QAction::triggered, this, [this]() { if (m_hooks.showWindow) m_hooks.showWindow(); });

    QAction *hideAct = trayMenu->addAction("📥  隐藏到托盘");
    hideAct->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(hideAct, &QAction::triggered, this, [this]() { if (m_hooks.hideWindow) m_hooks.hideWindow(); });

    trayMenu->addSeparator();

    QAction *settingsAct = trayMenu->addAction("⚙  偏好设置...");
    settingsAct->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAct, &QAction::triggered, this, [this]() {
        if (m_hooks.showWindow) m_hooks.showWindow();
        QTimer::singleShot(100, this, [this]() { if (m_hooks.openSettings) m_hooks.openSettings(); });
    });

    QAction *accountAct = trayMenu->addAction("👤  账户信息");
    connect(accountAct, &QAction::triggered, this, [this]() {
        if (m_hooks.showWindow) m_hooks.showWindow();
        if (m_hooks.showAccount) m_hooks.showAccount();
    });

    trayMenu->addSeparator();

    QAction *quitAct = trayMenu->addAction("🚪  退出");
    quitAct->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAct, &QAction::triggered, this, [this]() {
        m_initialized = false;
        if (m_hooks.quit) m_hooks.quit();
    });

    m_tray->setContextMenu(trayMenu);

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
