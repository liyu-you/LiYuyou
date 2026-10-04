#include "logindialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QScrollArea>
#include <QComboBox>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QLinearGradient>
#include <QCheckBox>
#include <QButtonGroup>
#include <QTimer>
#include <QRegularExpression>
#include <QProgressBar>
#include <QFile>
#include <QPainterPath>
#include <QStandardPaths>
#include <QDir>
#include <QByteArray>
#include <QSysInfo>
#include <QAction>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dpapi.h>
#endif

static const char *GREEN_BTN_STYLE =
    "QPushButton { background-color: #07c160; color: #ffffff; border: none; "
    "border-radius: 4px; padding: 12px; font-size: 15px; font-weight: 500; min-height: 20px; }"
    "QPushButton:hover { background-color: #06ad56; }"
    "QPushButton:pressed { background-color: #059849; }"
    "QPushButton:disabled { background-color: #a0d8b8; color: #e8f5ec; }";

static const char *LINK_BTN_STYLE =
    "QPushButton { background: transparent; color: #576b95; border: none; padding: 4px; font-size: 13px; }"
    "QPushButton:hover { text-decoration: underline; }";

// "Remember password" storage.
//
// The old scheme XORed the password with QSysInfo::machineUniqueId() plus a fixed
// string baked into the binary. That is obfuscation, not encryption: any process
// can read the machine id, so the stored password was recoverable in clear.
// DPAPI ties the ciphertext to the current Windows user account instead. The old
// format is still readable once, so an already-saved password keeps working and is
// rewritten in the new format on the next successful login.
static QString protectSecret(const QString &secret)
{
#ifdef Q_OS_WIN
    QByteArray in = secret.toUtf8();
    DATA_BLOB input{ static_cast<DWORD>(in.size()), reinterpret_cast<BYTE *>(in.data()) };
    DATA_BLOB output{ 0, nullptr };
    if (!CryptProtectData(&input, L"LiYuYou saved login", nullptr, nullptr, nullptr, 0, &output))
        return QString();
    const QByteArray blob(reinterpret_cast<const char *>(output.pbData),
                          static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return QStringLiteral("dpapi:") + QString::fromLatin1(blob.toBase64());
#else
    Q_UNUSED(secret);
    return QString();
#endif
}

static QString unprotectSecret(const QString &stored)
{
    if (stored.isEmpty()) return QString();
#ifdef Q_OS_WIN
    if (stored.startsWith(QLatin1String("dpapi:"))) {
        const QByteArray blob = QByteArray::fromBase64(stored.mid(6).toLatin1());
        DATA_BLOB input{ static_cast<DWORD>(blob.size()),
                         reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData())) };
        DATA_BLOB output{ 0, nullptr };
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output))
            return QString(); // different Windows user or machine: ask again
        const QByteArray plain(reinterpret_cast<const char *>(output.pbData),
                               static_cast<int>(output.cbData));
        LocalFree(output.pbData);
        return QString::fromUtf8(plain);
    }
#endif
    if (stored.startsWith(QLatin1String("enc:"))) {
        // Legacy XOR obfuscation, read once so the saved password survives.
        QByteArray data = QByteArray::fromBase64(stored.mid(4).toLatin1());
        QByteArray key = QSysInfo::machineUniqueId();
        if (key.isEmpty()) key = QByteArray("LiYuYou_Fallback_Key");
        key += QByteArray("_LiYuYou_2026_Secret");
        for (int i = 0; i < data.size(); ++i)
            data[i] = char(data.at(i) ^ key.at(i % key.size()));
        const QString result = QString::fromUtf8(data);
        for (QChar c : result) {
            if (c.category() == QChar::Other_Control && c != '\n' && c != '\r' && c != '\t')
                return QString(); // key changed - the user has to re-enter it
        }
        return result;
    }
    return stored; // an even older build stored it in clear; keep it working once
}

// Caps Lock makes a correct password look wrong, which is one of the most common
// login failures. Windows exposes the toggle state directly.
static bool capsLockOn()
{
#ifdef Q_OS_WIN
    return (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
#else
    return false;
#endif
}

// A visible eye icon for the show/hide password action. The previous code passed
// an empty QIcon, which left the toggle invisible and effectively unclickable.
static QIcon makeEyeIcon(bool open)
{
    const int s = 18;
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0x88, 0x88, 0x88));
    pen.setWidthF(1.4);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Eye outline: two arcs meeting at the corners.
    QPainterPath eye;
    eye.moveTo(2.0, s / 2.0);
    eye.quadTo(s / 2.0, 2.0, s - 2.0, s / 2.0);
    eye.quadTo(s / 2.0, s - 2.0, 2.0, s / 2.0);
    p.drawPath(eye);
    p.setBrush(open ? QColor(0x66, 0x66, 0x66) : Qt::NoBrush);
    p.drawEllipse(QPointF(s / 2.0, s / 2.0), 2.6, 2.6);
    if (!open) {
        p.setPen(QPen(QColor(0xcc, 0x44, 0x44), 1.6));
        p.drawLine(4, s - 4, s - 4, 4); // struck through when hidden
    }
    p.end();
    return QIcon(pm);
}

LoginDialog::LoginDialog(UserAuth *auth, QWidget *parent)
    : QDialog(parent), m_auth(auth), m_connectProgress(nullptr), m_connectTimer(nullptr), m_connectValue(0), m_autoLoginPreValidated(false), m_autoLoginWillSucceed(false)
{
    setWindowTitle("里与由 - 登录");
    setFixedSize(380, 600);
    setStyleSheet(R"(
        QDialog { background: #f7f7f7; }
        QLabel { color: #333; }
        QLineEdit { background: #fff; color: #333; border: 1px solid #e0e0e0; border-radius: 4px; padding: 9px 14px; font-size: 14px; }
        QLineEdit:focus { border-color: #07c160; }
        QComboBox { background: #fff; color: #333; border: 1px solid #e0e0e0; border-radius: 4px; padding: 9px 30px 9px 14px; font-size: 14px; }
        QComboBox:focus { border-color: #07c160; }
        QComboBox:hover { border-color: #ccc; }
        QComboBox::drop-down { border: none; width: 26px; }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #999;
            width: 0; height: 0; margin-right: 6px;
        }
        QComboBox:hover::down-arrow { border-top-color: #07c160; }
        QComboBox QAbstractItemView {
            background: #fff; color: #333; border: 1px solid #e0e0e0; border-radius: 4px;
            padding: 4px; outline: none; selection-background-color: #e8f5e9; selection-color: #07c160;
        }
        QComboBox QAbstractItemView::item { padding: 8px 12px; border-radius: 3px; }
        QComboBox QAbstractItemView::item:hover { background: #f5f5f5; }
    )");

    // Load last user and saved credentials
    QSettings settings("PureRefCanvas", "里与由");
    m_lastUser = settings.value("lastUser", "").toString();
    QString savedAccount = settings.value("login/account", "").toString();
    QString savedPassword = unprotectSecret(settings.value("login/password", "").toString());
    bool remember = settings.value("login/remember", false).toBool();
    bool autoLogin = settings.value("login/autoLogin", false).toBool();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Error label
    m_errorLabel = new QLabel;
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet("color: #fa5151; font-size: 12px; padding: 4px; min-height: 20px;");
    mainLayout->addWidget(m_errorLabel);

    // Stacked pages
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("background: transparent;");
    setupConnectPage();
    setupQuickPage();
    setupLoginPage();
    setupRegisterPage();
    setupForgotPage();
    mainLayout->addWidget(m_stack, 1);

    // Restore saved credentials
    if (!savedAccount.isEmpty()) m_loginAccount->setText(savedAccount);
    if (remember && !savedPassword.isEmpty()) {
        m_loginPassword->setText(savedPassword);
        if (m_rememberPwd) m_rememberPwd->setChecked(true);
    }
    if (m_autoLogin) m_autoLogin->setChecked(autoLogin);

    // Decide start page
    if (autoLogin && !savedAccount.isEmpty() && !savedPassword.isEmpty()) {
        // Steam-style: show connecting page then auto login
        QString display = m_auth ? m_auth->displayName(m_lastUser.isEmpty() ? savedAccount : m_lastUser) : (m_lastUser.isEmpty() ? savedAccount : m_lastUser);
        m_quickNameLabel->setText(display);
        m_avatarLabel->setPixmap(loadUserAvatar(m_lastUser.isEmpty() ? savedAccount : m_lastUser, 80));
        if (m_connectingLabel) m_connectingLabel->setText(QString("正在登录为 %1...").arg(display));
        m_stack->setCurrentIndex(0); // connecting page
        // Start progress bar animation: 0->100 in 700ms
        m_connectValue = 0;
        m_autoLoginPreValidated = false;
        m_autoLoginWillSucceed = false;
        if (m_connectProgress) m_connectProgress->setValue(0);
        m_savedAutoAccount = savedAccount;
        m_savedAutoPassword = savedPassword;
        if (!m_connectTimer) {
            m_connectTimer = new QTimer(this);
            connect(m_connectTimer, &QTimer::timeout, this, &LoginDialog::onConnectTick);
        }
        m_connectTimer->start(7); // 7ms * 100 steps = 700ms
    } else if (!m_lastUser.isEmpty() && m_auth->userExists(m_lastUser)) {
        QString display = m_auth ? m_auth->displayName(m_lastUser) : m_lastUser;
        m_quickNameLabel->setText(display);
        m_avatarLabel->setPixmap(loadUserAvatar(m_lastUser, 80));
        m_stack->setCurrentIndex(1); // quick page
    } else {
        m_stack->setCurrentIndex(2); // login page
    }

    // Surface storage problems immediately: a damaged store refuses writes, and a
    // blocked Documents folder means the data had to move. Both otherwise look
    // like "my changes don't stick".
    if (m_auth && !m_auth->storeError().isEmpty())
        showError(m_auth->storeError());
}

QPixmap LoginDialog::makeAvatar(const QString &name, int size) const
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0, 0, size, size);
    int hue = (name.isEmpty() ? 0 : name.at(0).unicode()) % 360;
    grad.setColorAt(0, QColor::fromHsl(hue, 120, 160));
    grad.setColorAt(1, QColor::fromHsl((hue + 40) % 360, 120, 140));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, size, size, 12, 12);
    p.setPen(QColor(255, 255, 255, 230));
    QFont font;
    font.setPointSize(size / 3);
    font.setBold(true);
    p.setFont(font);
    QString ch = name.isEmpty() ? QString("?") : QString(name.at(0).toUpper());
    p.drawText(pix.rect(), Qt::AlignCenter, ch);
    return pix;
}

QPixmap LoginDialog::loadUserAvatar(const QString &username, int size) const
{
    // Try user's custom avatar from UserAuth
    if (m_auth) {
        QString avatarPath = m_auth->avatarPathForUser(username);
        if (!avatarPath.isEmpty() && QFile::exists(avatarPath)) {
            QPixmap src(avatarPath);
            if (!src.isNull()) {
                QPixmap result(size, size);
                result.fill(Qt::transparent);
                QPainter p(&result);
                p.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addEllipse(0, 0, size, size);
                p.setClipPath(path);
                p.drawPixmap(0, 0, size, size, src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                return result;
            }
        }
    }
    QString display = m_auth ? m_auth->displayName(username) : username;
    return makeAvatar(display, size);
}

void LoginDialog::setupConnectPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 60, 40, 40);
    layout->setSpacing(0);

    // Avatar
    m_avatarLabel = new QLabel;
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setFixedSize(80, 80);
    layout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    // Username
    m_quickNameLabel = new QLabel;
    m_quickNameLabel->setAlignment(Qt::AlignCenter);
    m_quickNameLabel->setStyleSheet("font-size: 20px; font-weight: 500; color: #191919;");
    layout->addWidget(m_quickNameLabel);
    layout->addSpacing(24);

    // Connecting label
    m_connectingLabel = new QLabel("正在连接...");
    m_connectingLabel->setAlignment(Qt::AlignCenter);
    m_connectingLabel->setStyleSheet("font-size: 14px; color: #888;");
    layout->addWidget(m_connectingLabel);
    layout->addSpacing(12);

    // Progress bar (determinate, animates during auto-login)
    m_connectProgress = new QProgressBar;
    m_connectProgress->setRange(0, 100);
    m_connectProgress->setValue(0);
    m_connectProgress->setTextVisible(false);
    m_connectProgress->setFixedHeight(6);
    m_connectProgress->setStyleSheet(R"(
        QProgressBar { background: rgba(255,255,255,0.15); border: none; border-radius: 3px; }
        QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #07c160, stop:1 #0ae070); border-radius: 3px; }
    )");
    layout->addWidget(m_connectProgress);
    layout->addSpacing(20);

    // Cancel / switch account button
    QPushButton *cancelBtn = new QPushButton("切换账号");
    cancelBtn->setStyleSheet("QPushButton { background: transparent; color: #576b95; border: 1px solid #d0d0d0; border-radius: 4px; padding: 8px 24px; font-size: 13px; } QPushButton:hover { background: #f0f0f0; }");
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_stack->setCurrentIndex(2); // go to login page
    });
    layout->addWidget(cancelBtn, 0, Qt::AlignCenter);

    layout->addStretch();
    m_stack->addWidget(page);
}

void LoginDialog::setupQuickPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 50, 40, 30);
    layout->setSpacing(0);

    // Avatar (shared with connect page, but we need separate widgets)
    QLabel *avatar = new QLabel;
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(80, 80);
    avatar->setPixmap(loadUserAvatar(m_lastUser, 80));
    layout->addWidget(avatar, 0, Qt::AlignCenter);
    layout->addSpacing(16);

    // Username
    QString display = m_auth ? m_auth->displayName(m_lastUser) : m_lastUser;
    QLabel *nameLabel = new QLabel(display);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("font-size: 20px; font-weight: 500; color: #191919;");
    layout->addWidget(nameLabel);
    layout->addSpacing(40);

    // Login button
    QPushButton *loginBtn = new QPushButton("确认登录");
    loginBtn->setStyleSheet(GREEN_BTN_STYLE);
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::doQuickLogin);
    layout->addWidget(loginBtn);
    layout->addSpacing(16);

    // Bottom links
    QHBoxLayout *links = new QHBoxLayout;
    QPushButton *switchBtn = new QPushButton("切换账号");
    switchBtn->setStyleSheet(LINK_BTN_STYLE);
    connect(switchBtn, &QPushButton::clicked, this, [this]() {
        m_loginAccount->clear();
        m_loginPassword->clear();
        m_stack->setCurrentIndex(2);
        clearError();
    });
    QPushButton *regBtn = new QPushButton("注册账号");
    regBtn->setStyleSheet(LINK_BTN_STYLE);
    connect(regBtn, &QPushButton::clicked, this, [this]() {
        m_stack->setCurrentIndex(3);
        clearError();
    });
    links->addWidget(switchBtn);
    links->addStretch();
    links->addWidget(regBtn);
    layout->addLayout(links);
    layout->addStretch();

    m_stack->addWidget(page);
}

void LoginDialog::setupLoginPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(12);

    QLabel *title = new QLabel("登录");
    title->setStyleSheet("font-size: 22px; font-weight: 600; color: #191919; padding-bottom: 8px;");
    layout->addWidget(title);

    m_loginAccount = new QLineEdit;
    m_loginAccount->setPlaceholderText("用户名 / 邮箱");
    layout->addWidget(m_loginAccount);

    m_loginPassword = new QLineEdit;
    m_loginPassword->setPlaceholderText("密码");
    m_loginPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_loginPassword);

    // Show/hide password (the icon used to be empty, so the toggle was invisible
    // and effectively unclickable)
    QAction *toggleAct = m_loginPassword->addAction(makeEyeIcon(false), QLineEdit::TrailingPosition);
    toggleAct->setToolTip("显示/隐藏密码");
    bool pwdVisible = false;
    connect(toggleAct, &QAction::triggered, this, [this, &pwdVisible, toggleAct]() {
        pwdVisible = !pwdVisible;
        m_loginPassword->setEchoMode(pwdVisible ? QLineEdit::Normal : QLineEdit::Password);
        toggleAct->setIcon(makeEyeIcon(pwdVisible));
    });

    // Caps Lock turns a correct password into a rejected one, so say so instead of
    // letting the user collect "密码错误" messages.
    QLabel *capsHint = new QLabel("⚠ 大写锁定已开启");
    capsHint->setStyleSheet("color: #fa9d3b; font-size: 11px; padding: 0 2px;");
    capsHint->setVisible(false);
    layout->addWidget(capsHint);
    connect(m_loginPassword, &QLineEdit::textEdited, this, [this, capsHint](const QString &) {
        capsHint->setVisible(capsLockOn());
        clearError();
    });
    connect(m_loginAccount, &QLineEdit::textEdited, this, &LoginDialog::clearError);

    // Remember / Auto login
    QHBoxLayout *opts = new QHBoxLayout;
    m_rememberPwd = new QCheckBox("记住密码");
    m_rememberPwd->setStyleSheet("QCheckBox { color: #555; font-size: 12px; spacing: 6px; } QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #ccc; border-radius: 3px; background: #fff; } QCheckBox::indicator:checked { background: #07c160; border-color: #07c160; }");
    m_autoLogin = new QCheckBox("自动登录");
    m_autoLogin->setStyleSheet("QCheckBox { color: #555; font-size: 12px; spacing: 6px; } QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #ccc; border-radius: 3px; background: #fff; } QCheckBox::indicator:checked { background: #07c160; border-color: #07c160; }");
    connect(m_autoLogin, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_rememberPwd->setChecked(true);
    });
    opts->addWidget(m_rememberPwd);
    opts->addStretch();
    opts->addWidget(m_autoLogin);
    layout->addLayout(opts);

    layout->addSpacing(8);

    QPushButton *loginBtn = new QPushButton("确认登录");
    loginBtn->setStyleSheet(GREEN_BTN_STYLE);
    loginBtn->setDefault(true); // Enter submits from any field on this page
    connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::doLogin);
    connect(m_loginAccount, &QLineEdit::returnPressed, this, &LoginDialog::doLogin);
    connect(m_loginPassword, &QLineEdit::returnPressed, this, &LoginDialog::doLogin);
    layout->addWidget(loginBtn);

    QHBoxLayout *links = new QHBoxLayout;
    QPushButton *forgotBtn = new QPushButton("忘记密码？");
    forgotBtn->setStyleSheet(LINK_BTN_STYLE);
    forgotBtn->setAutoDefault(false); // must not steal Enter from the login button
    connect(forgotBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(4); clearError(); });
    QPushButton *regBtn = new QPushButton("注册账号");
    regBtn->setStyleSheet(LINK_BTN_STYLE);
    regBtn->setAutoDefault(false);
    connect(regBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(3); clearError(); });
    links->addWidget(forgotBtn);
    links->addStretch();
    links->addWidget(regBtn);
    layout->addLayout(links);

    layout->addStretch();
    m_stack->addWidget(page);
}

void LoginDialog::setupRegisterPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    // Scroll area — register form has many fields, ensure all are reachable
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(R"(
        QScrollArea { background: #f7f7f7; border: none; }
        QScrollArea > QWidget > QWidget { background: #f7f7f7; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: #ccc; border-radius: 4px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #aaa; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
    scroll->viewport()->setStyleSheet("background: #f7f7f7;");

    QWidget *content = new QWidget;
    content->setStyleSheet("background: #f7f7f7;");
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 12, 20, 12);
    layout->setSpacing(4);

    QLabel *title = new QLabel("注册账号");
    title->setStyleSheet("font-size: 20px; font-weight: 600; color: #191919; padding-bottom: 2px;");
    layout->addWidget(title);

    m_regUsername = new QLineEdit;
    m_regUsername->setPlaceholderText("用户名（2-20位）");
    layout->addWidget(m_regUsername);

    m_regEmail = new QLineEdit;
    m_regEmail->setPlaceholderText("邮箱（用于找回密码）");
    layout->addWidget(m_regEmail);

    m_regPassword = new QLineEdit;
    m_regPassword->setPlaceholderText("密码（6-32位，含字母和数字）");
    m_regPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_regPassword);

    // Password strength: 5-segment bar + label (more reliable than QProgressBar)
    QWidget *strengthWidget = new QWidget;
    QHBoxLayout *strengthLay = new QHBoxLayout(strengthWidget);
    strengthLay->setContentsMargins(0, 0, 0, 0);
    strengthLay->setSpacing(6);
    QLabel *strengthLabel = new QLabel("密码强度：未输入");
    strengthLabel->setStyleSheet("color: #999; font-size: 11px;");
    strengthLay->addWidget(strengthLabel);
    strengthLay->addStretch();
    QList<QWidget*> strengthSegs;
    for (int i = 0; i < 5; ++i) {
        QWidget *seg = new QWidget;
        seg->setFixedSize(18, 4);
        seg->setStyleSheet("background: #e0e0e0; border-radius: 2px;");
        strengthLay->addWidget(seg);
        strengthSegs.append(seg);
    }
    layout->addWidget(strengthWidget);
    connect(m_regPassword, &QLineEdit::textChanged, this, [strengthSegs, strengthLabel](const QString &pwd) {
        if (pwd.isEmpty()) {
            strengthLabel->setText("密码强度：未输入"); strengthLabel->setStyleSheet("color: #999; font-size: 11px;");
            for (QWidget *s : strengthSegs) s->setStyleSheet("background: #e0e0e0; border-radius: 2px;");
            return;
        }
        int score = 0;
        if (pwd.length() >= 8) score++;
        if (pwd.length() >= 12) score++;
        if (pwd.contains(QRegularExpression("[a-z]")) && pwd.contains(QRegularExpression("[A-Z]"))) score++;
        if (pwd.contains(QRegularExpression("\\d"))) score++;
        if (pwd.contains(QRegularExpression("[^a-zA-Z0-9]"))) score++;
        QString color = score <= 1 ? "#fa5151" : score <= 3 ? "#fa8c16" : "#07c160";
        QString text = score <= 1 ? "弱" : score <= 3 ? "中" : "强";
        strengthLabel->setText(QString("密码强度：%1").arg(text));
        strengthLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(color));
        for (int i = 0; i < strengthSegs.size(); ++i)
            strengthSegs[i]->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(i < score ? color : "#e0e0e0"));
    });

    // Show/hide password (visible icon, see makeEyeIcon)
    QAction *regToggle = m_regPassword->addAction(makeEyeIcon(false), QLineEdit::TrailingPosition);
    regToggle->setToolTip("显示/隐藏密码");
    bool regPwdVisible = false;
    connect(regToggle, &QAction::triggered, this, [this, &regPwdVisible, regToggle]() {
        regPwdVisible = !regPwdVisible;
        m_regPassword->setEchoMode(regPwdVisible ? QLineEdit::Normal : QLineEdit::Password);
        regToggle->setIcon(makeEyeIcon(regPwdVisible));
    });

    m_regConfirmPassword = new QLineEdit;
    m_regConfirmPassword->setPlaceholderText("确认密码");
    m_regConfirmPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_regConfirmPassword);

    // Match indicator (hidden when empty to avoid extra spacing)
    QLabel *matchLabel = new QLabel;
    matchLabel->setStyleSheet("font-size: 11px; padding: 0 2px;");
    matchLabel->setVisible(false);
    layout->addWidget(matchLabel);
    connect(m_regConfirmPassword, &QLineEdit::textChanged, this, [this, matchLabel](const QString &pwd) {
        if (pwd.isEmpty()) { matchLabel->clear(); matchLabel->setVisible(false); return; }
        matchLabel->setVisible(true);
        if (pwd == m_regPassword->text()) { matchLabel->setText("✓ 两次密码一致"); matchLabel->setStyleSheet("color: #07c160; font-size: 11px; padding: 0 2px;"); }
        else { matchLabel->setText("✗ 两次密码不一致"); matchLabel->setStyleSheet("color: #fa5151; font-size: 11px; padding: 0 2px;"); }
    });

    m_regQuestion = new QComboBox;
    m_regQuestion->addItems({"你的出生地？", "你的母校？", "你的宠物名？", "你最喜欢的颜色？", "你父亲的名字？"});
    layout->addWidget(m_regQuestion);

    m_regAnswer = new QLineEdit;
    m_regAnswer->setPlaceholderText("安全问题答案（用于找回密码）");
    layout->addWidget(m_regAnswer);

    // Agree terms
    m_agreeTerms = new QCheckBox("我已阅读并同意《用户协议》和《隐私政策》");
    m_agreeTerms->setStyleSheet("QCheckBox { color: #555; font-size: 11px; spacing: 6px; } QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #ccc; border-radius: 3px; background: #fff; } QCheckBox::indicator:checked { background: #07c160; border-color: #07c160; }");
    layout->addWidget(m_agreeTerms);

    layout->addSpacing(4);

    QPushButton *regBtn = new QPushButton("确认注册");
    regBtn->setStyleSheet(GREEN_BTN_STYLE);
    regBtn->setDefault(true); // Enter submits from any field on this page
    connect(regBtn, &QPushButton::clicked, this, &LoginDialog::doRegister);
    connect(m_regConfirmPassword, &QLineEdit::returnPressed, this, &LoginDialog::doRegister);
    connect(m_regAnswer, &QLineEdit::returnPressed, this, &LoginDialog::doRegister);
    layout->addWidget(regBtn);

    QPushButton *backBtn = new QPushButton("已有账号？返回登录");
    backBtn->setStyleSheet(LINK_BTN_STYLE);
    backBtn->setAutoDefault(false);
    connect(backBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(2); clearError(); });
    layout->addWidget(backBtn);

    // No addStretch() here — in a QScrollArea it would force the widget
    // height to viewport height and clip overflow instead of scrolling.

    scroll->setWidget(content);
    pageLayout->addWidget(scroll);
    m_stack->addWidget(page);
}

void LoginDialog::setupForgotPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(10);

    QLabel *title = new QLabel("找回密码");
    title->setStyleSheet("font-size: 20px; font-weight: 600; color: #191919; padding-bottom: 4px;");
    layout->addWidget(title);

    QLabel *hint = new QLabel("请输入注册邮箱和安全问题答案以重置密码");
    hint->setStyleSheet("color: #888; font-size: 12px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    m_forgotEmail = new QLineEdit;
    m_forgotEmail->setPlaceholderText("注册邮箱");
    layout->addWidget(m_forgotEmail);

    m_forgotAnswer = new QLineEdit;
    m_forgotAnswer->setPlaceholderText("安全问题答案");
    layout->addWidget(m_forgotAnswer);

    m_forgotNewPassword = new QLineEdit;
    m_forgotNewPassword->setPlaceholderText("新密码（6-32位，含字母和数字）");
    m_forgotNewPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_forgotNewPassword);
    {
        QAction *t = m_forgotNewPassword->addAction(makeEyeIcon(false), QLineEdit::TrailingPosition);
        t->setToolTip("显示/隐藏密码");
        bool v = false;
        connect(t, &QAction::triggered, this, [this, &v, t]() {
            v = !v;
            m_forgotNewPassword->setEchoMode(v ? QLineEdit::Normal : QLineEdit::Password);
            t->setIcon(makeEyeIcon(v));
        });
    }

    m_forgotConfirmPassword = new QLineEdit;
    m_forgotConfirmPassword->setPlaceholderText("确认新密码");
    m_forgotConfirmPassword->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_forgotConfirmPassword);
    {
        QAction *t = m_forgotConfirmPassword->addAction(makeEyeIcon(false), QLineEdit::TrailingPosition);
        t->setToolTip("显示/隐藏密码");
        bool v = false;
        connect(t, &QAction::triggered, this, [this, &v, t]() {
            v = !v;
            m_forgotConfirmPassword->setEchoMode(v ? QLineEdit::Normal : QLineEdit::Password);
            t->setIcon(makeEyeIcon(v));
        });
    }

    layout->addSpacing(8);

    QPushButton *resetBtn = new QPushButton("确认重置");
    resetBtn->setStyleSheet(GREEN_BTN_STYLE);
    resetBtn->setDefault(true); // Enter submits from any field on this page
    connect(resetBtn, &QPushButton::clicked, this, &LoginDialog::doResetPassword);
    connect(m_forgotConfirmPassword, &QLineEdit::returnPressed, this, &LoginDialog::doResetPassword);
    layout->addWidget(resetBtn);

    QPushButton *backBtn = new QPushButton("返回登录");
    backBtn->setStyleSheet(LINK_BTN_STYLE);
    backBtn->setAutoDefault(false);
    connect(backBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(2); clearError(); });
    layout->addWidget(backBtn);

    layout->addStretch();
    m_stack->addWidget(page);
}

void LoginDialog::doQuickLogin()
{
    clearError();
    m_loginAccount->setText(m_lastUser);
    m_stack->setCurrentIndex(2);
    m_loginPassword->setFocus();
}

void LoginDialog::doLogin()
{
    clearError();
    QString error;
    if (m_auth->login(m_loginAccount->text(), m_loginPassword->text(), &error)) {
        m_loggedInUser = m_auth->currentUser();
        QSettings settings("PureRefCanvas", "里与由");
        settings.setValue("lastUser", m_loggedInUser.username);
        settings.setValue("login/account", m_loginAccount->text());
        if (m_rememberPwd && m_rememberPwd->isChecked()) {
            const QString protectedPwd = protectSecret(m_loginPassword->text());
            if (!protectedPwd.isEmpty()) {
                settings.setValue("login/password", protectedPwd);
                settings.setValue("login/remember", true);
            } else {
                // Never fall back to writing the password unprotected.
                settings.remove("login/password");
                settings.setValue("login/remember", false);
            }
        } else {
            settings.remove("login/password");
            settings.setValue("login/remember", false);
        }
        settings.setValue("login/autoLogin", m_autoLogin && m_autoLogin->isChecked());
        accept();
    } else {
        showError(error);
        // If auto-login failed, go to login page
        if (m_stack->currentIndex() == 0) {
            m_stack->setCurrentIndex(2);
        }
    }
}

void LoginDialog::doRegister()
{
    clearError();
    if (!m_agreeTerms->isChecked()) {
        showError("请先阅读并同意《用户协议》和《隐私政策》");
        return;
    }
    if (m_regPassword->text() != m_regConfirmPassword->text()) {
        showError("两次密码不一致");
        return;
    }
    QString error;
    if (m_auth->registerUser(m_regUsername->text(), m_regEmail->text(),
                              m_regPassword->text(), m_regQuestion->currentText(),
                              m_regAnswer->text(), &error)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("注册成功");
        msgBox.setText(QString("账号「%1」注册成功！\n\n绑定邮箱：%2\n\n请使用账号密码登录。").arg(m_regUsername->text()).arg(m_regEmail->text()));
        msgBox.setStyleSheet("QMessageBox { background: #fff; } QLabel { color: #333; font-size: 13px; } QPushButton { background-color: #07c160; color: #fff; border: none; border-radius: 4px; padding: 8px 24px; font-size: 13px; } QPushButton:hover { background-color: #06ad56; }");
        msgBox.addButton("确认登录", QMessageBox::AcceptRole);
        msgBox.exec();

        m_loginAccount->setText(m_regUsername->text());
        m_loginPassword->clear();
        m_stack->setCurrentIndex(2);
    } else {
        showError(error);
    }
}

void LoginDialog::doResetPassword()
{
    clearError();
    if (m_forgotNewPassword->text() != m_forgotConfirmPassword->text()) {
        showError("两次密码不一致");
        return;
    }
    QString error;
    if (m_auth->resetPassword(m_forgotEmail->text(), m_forgotAnswer->text(),
                               m_forgotNewPassword->text(), &error)) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("重置成功");
        msgBox.setText("密码已成功重置！\n\n请使用新密码登录。");
        msgBox.setStyleSheet("QMessageBox { background: #fff; } QLabel { color: #333; font-size: 13px; } QPushButton { background-color: #07c160; color: #fff; border: none; border-radius: 4px; padding: 8px 24px; font-size: 13px; } QPushButton:hover { background-color: #06ad56; }");
        msgBox.addButton("确认", QMessageBox::AcceptRole);
        msgBox.exec();
        m_stack->setCurrentIndex(2);
    } else {
        showError(error);
    }
}

void LoginDialog::showError(const QString &msg)
{
    m_errorLabel->setText(msg);
}

void LoginDialog::clearError()
{
    m_errorLabel->clear();
}

void LoginDialog::onConnectTick()
{
    if (m_stack->currentIndex() != 0) {
        // User switched away (cancelled auto-login)
        if (m_connectTimer) m_connectTimer->stop();
        return;
    }
    m_connectValue++;
    if (m_connectProgress) m_connectProgress->setValue(m_connectValue);

    // Pre-validate credentials at 50% (pure verify, no side effects)
    if (m_connectValue == 50 && !m_autoLoginPreValidated) {
        m_autoLoginPreValidated = true;
        m_autoLoginWillSucceed = m_auth->verifyLogin(m_savedAutoAccount, m_savedAutoPassword);
    }

    if (m_connectValue >= 100) {
        if (m_connectTimer) m_connectTimer->stop();
        if (m_autoLoginWillSucceed) {
            // Pre-validated: fast login without re-checking password
            m_auth->loginAsUser(m_savedAutoAccount);
            m_loggedInUser = m_auth->currentUser();
            accept();
        } else if (m_auth->login(m_savedAutoAccount, m_savedAutoPassword, nullptr)) {
            m_loggedInUser = m_auth->currentUser();
            accept();
        } else {
            showError("自动登录失败，请重新登录");
            m_stack->setCurrentIndex(2);
        }
    }
}
