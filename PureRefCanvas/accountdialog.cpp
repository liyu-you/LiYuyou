#include "accountdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSettings>

AccountDialog::AccountDialog(UserAuth *auth, QWidget *parent)
    : QDialog(parent), m_auth(auth)
{
    setWindowTitle("账号信息");
    setFixedSize(380, 600);

    UserInfo user = m_auth->currentUser();
    QString displayName = user.nickname.isEmpty() ? user.username : user.nickname;

    // Load cover pixmap for full-dialog background
    QPixmap coverPix;
    if (!user.backgroundPath.isEmpty() && QFile::exists(user.backgroundPath)) {
        coverPix = QPixmap(user.backgroundPath).scaled(380, 600, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    } else {
        coverPix = QPixmap(380, 600);
        QPainter bp(&coverPix);
        QLinearGradient grad(0, 0, 380, 600);
        int hue = displayName.isEmpty() ? 0 : displayName.at(0).unicode() % 360;
        grad.setColorAt(0, QColor::fromHsl(hue, 80, 100));
        grad.setColorAt(1, QColor::fromHsl((hue + 60) % 360, 80, 70));
        bp.fillRect(coverPix.rect(), grad);
    }
    // Set cover as full-dialog background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setBrush(QPalette::Window, QBrush(coverPix));
    setPalette(pal);

    setStyleSheet(R"(
        QLabel { color: #fff; }
        QLineEdit { background: rgba(255,255,255,0.15); color: #fff; border: 1px solid rgba(255,255,255,0.2); border-radius: 4px; padding: 10px 14px; font-size: 14px; }
        QLineEdit:focus { border-color: #07c160; background: rgba(255,255,255,0.2); }
        QLineEdit:disabled { background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.5); }
        QPushButton#greenBtn { background-color: #07c160; color: #fff; border: none; border-radius: 4px; padding: 10px; font-size: 14px; font-weight: 500; }
        QPushButton#greenBtn:hover { background-color: #06ad56; }
        QPushButton#grayBtn { background: rgba(255,255,255,0.15); color: #fff; border: 1px solid rgba(255,255,255,0.2); border-radius: 4px; padding: 10px; font-size: 14px; }
        QPushButton#grayBtn:hover { background: rgba(255,255,255,0.25); }
        QPushButton#redBtn { background: rgba(255,255,255,0.15); color: #ff6b6b; border: 1px solid rgba(255,107,107,0.3); border-radius: 4px; padding: 10px; font-size: 14px; }
        QPushButton#redBtn:hover { background: rgba(255,107,107,0.2); }
        QPushButton#avatarBtn { background: transparent; border: 2px solid rgba(255,255,255,0.8); padding: 0; border-radius: 36px; }
        QPushButton#avatarBtn:hover { border-color: #07c160; }
        QPushButton#bgBtn { background: transparent; border: none; padding: 0; }
    )");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ===== Top cover area (transparent, click to change cover) =====
    QPushButton *bgBtn = new QPushButton;
    bgBtn->setObjectName("bgBtn");
    bgBtn->setFixedHeight(140);
    bgBtn->setCursor(Qt::PointingHandCursor);
    bgBtn->setToolTip("点击更换封面背景");
    connect(bgBtn, &QPushButton::clicked, this, &AccountDialog::onChangeBackground);

    // Background hint
    QLabel *bgHint = new QLabel("点击更换封面", bgBtn);
    bgHint->setGeometry(280, 8, 90, 24);
    bgHint->setAlignment(Qt::AlignCenter);
    bgHint->setStyleSheet("color: rgba(255,255,255,200); font-size: 11px; background: rgba(0,0,0,0.25); border-radius: 12px; padding: 2px 8px;");
    bgHint->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Avatar overlaid on cover
    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(72, 72);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setStyleSheet("border-radius: 36px;");
    m_avatarLabel->setPixmap(loadAvatar(user, 72));

    QPushButton *avatarBtn = new QPushButton;
    avatarBtn->setObjectName("avatarBtn");
    avatarBtn->setFixedSize(72, 72);
    avatarBtn->setCursor(Qt::PointingHandCursor);
    avatarBtn->setToolTip("点击更换头像");
    avatarBtn->setParent(bgBtn);
    avatarBtn->move(24, 34); // 垂直居中: (140-72)/2 = 34
    QVBoxLayout *avatarBtnLayout = new QVBoxLayout(avatarBtn);
    avatarBtnLayout->setContentsMargins(0, 0, 0, 0);
    avatarBtnLayout->addWidget(m_avatarLabel);
    connect(avatarBtn, &QPushButton::clicked, this, &AccountDialog::onChangeAvatar);

    // Nickname on cover (vertical center with avatar)
    QLabel *coverName = new QLabel(displayName, bgBtn);
    coverName->setGeometry(108, 44, 200, 28);
    coverName->setStyleSheet("color: #fff; font-size: 18px; font-weight: 600;");
    coverName->setAttribute(Qt::WA_TransparentForMouseEvents);

    // User ID on cover
    m_coverIdLabel = new QLabel("ID: " + user.userId, bgBtn);
    m_coverIdLabel->setGeometry(108, 74, 200, 16);
    m_coverIdLabel->setStyleSheet("color: rgba(255,255,255,180); font-size: 11px;");
    m_coverIdLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    mainLayout->addWidget(bgBtn);

    // ===== Form area (glass transparent) =====
    QWidget *formWidget = new QWidget;
    formWidget->setStyleSheet("background: rgba(15, 15, 25, 0.72); border-top: 1px solid rgba(255,255,255,0.1);");
    formWidget->setAutoFillBackground(true);
    QVBoxLayout *formLayout = new QVBoxLayout(formWidget);
    formLayout->setContentsMargins(24, 20, 24, 20);
    formLayout->setSpacing(10);

    // Nickname
    QLabel *nickLabel = new QLabel("昵称");
    nickLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px; padding-left: 2px;");
    formLayout->addWidget(nickLabel);
    m_nicknameEdit = new QLineEdit(displayName);
    m_nicknameEdit->setPlaceholderText("显示的昵称");
    formLayout->addWidget(m_nicknameEdit);

    // Username
    QLabel *userLabel = new QLabel("用户名（登录账号）");
    userLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px; padding-left: 2px;");
    formLayout->addWidget(userLabel);
    m_usernameEdit = new QLineEdit(user.username);
    formLayout->addWidget(m_usernameEdit);

    // Email
    QLabel *emailLabel = new QLabel("邮箱");
    emailLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px; padding-left: 2px;");
    formLayout->addWidget(emailLabel);
    m_emailEdit = new QLineEdit(user.email);
    m_emailEdit->setPlaceholderText("绑定邮箱用于找回密码");
    formLayout->addWidget(m_emailEdit);

    // User ID (editable - custom ID, 30-day cooldown)
    QLabel *idLabel = new QLabel("账户ID（字母开头，字母+数字，6-20位，每30天可改一次）");
    idLabel->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 12px; padding-left: 2px;");
    formLayout->addWidget(idLabel);
    m_userIdEdit = new QLineEdit(user.userId);
    m_userIdEdit->setPlaceholderText("自定义ID，如 liyu2024");
    formLayout->addWidget(m_userIdEdit);
    // Cooldown hint
    if (user.lastIdChangeTime.isValid()) {
        qint64 daysLeft = 30 - user.lastIdChangeTime.daysTo(QDateTime::currentDateTime());
        if (daysLeft > 0) {
            QLabel *cdHint = new QLabel(QString("距离下次可修改还有 %1 天").arg(daysLeft));
            cdHint->setStyleSheet("color: #ffb84d; font-size: 11px; padding-left: 2px;");
            formLayout->addWidget(cdHint);
            m_userIdEdit->setEnabled(false);
            m_userIdEdit->setStyleSheet("background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.5); border: 1px solid rgba(255,255,255,0.1); border-radius: 4px; padding: 10px 14px;");
        }
    }

    m_createdLabel = new QLabel("注册时间：" + user.createdAt.toString("yyyy-MM-dd hh:mm"));
    m_createdLabel->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 11px; padding-left: 2px;");
    formLayout->addWidget(m_createdLabel);

    m_lastLoginLabel = new QLabel("上次登录：" + user.lastLogin.toString("yyyy-MM-dd hh:mm"));
    m_lastLoginLabel->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 11px; padding-left: 2px;");
    formLayout->addWidget(m_lastLoginLabel);

    formLayout->addStretch();

    // Buttons
    QHBoxLayout *btnRow1 = new QHBoxLayout;
    QPushButton *pwdBtn = new QPushButton("修改密码");
    pwdBtn->setObjectName("grayBtn");
    connect(pwdBtn, &QPushButton::clicked, this, &AccountDialog::onChangePassword);
    btnRow1->addWidget(pwdBtn);
    formLayout->addLayout(btnRow1);

    QHBoxLayout *btnRow2 = new QHBoxLayout;
    QPushButton *logoutBtn = new QPushButton("退出登录");
    logoutBtn->setObjectName("redBtn");
    connect(logoutBtn, &QPushButton::clicked, this, &AccountDialog::onLogout);
    QPushButton *saveBtn = new QPushButton("保存修改");
    saveBtn->setObjectName("greenBtn");
    connect(saveBtn, &QPushButton::clicked, this, &AccountDialog::onSave);
    btnRow2->addWidget(logoutBtn);
    btnRow2->addWidget(saveBtn);
    formLayout->addLayout(btnRow2);

    mainLayout->addWidget(formWidget, 1);
}

QPixmap AccountDialog::loadAvatar(const UserInfo &user, int size) const
{
    if (!user.avatarPath.isEmpty() && QFile::exists(user.avatarPath)) {
        QPixmap pix(user.avatarPath);
        if (!pix.isNull()) {
            // Crop to circle
            QPixmap result(size, size);
            result.fill(Qt::transparent);
            QPainter p(&result);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath path;
            path.addEllipse(0, 0, size, size);
            p.setClipPath(path);
            p.drawPixmap(0, 0, size, size, pix.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
            return result;
        }
    }
    // Default gradient avatar
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0, 0, size, size);
    QString display = user.nickname.isEmpty() ? user.username : user.nickname;
    int hue = display.isEmpty() ? 0 : display.at(0).unicode() % 360;
    grad.setColorAt(0, QColor::fromHsl(hue, 120, 160));
    grad.setColorAt(1, QColor::fromHsl((hue + 40) % 360, 120, 140));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);
    p.setPen(QColor(255, 255, 255, 230));
    QFont font;
    font.setPointSize(size / 3);
    font.setBold(true);
    p.setFont(font);
    QString ch = display.isEmpty() ? "?" : QString(display.at(0).toUpper());
    p.drawText(pix.rect(), Qt::AlignCenter, ch);
    return pix;
}

// Copy an image into the user data directory, reporting a real reason instead of
// QFile::copy()'s bare false. The usual failure is the destination folder being
// blocked by security software, and the message has to say so to be actionable.
static bool copyImageFile(const QString &from, const QString &to, QString *why)
{
    QFile src(from);
    if (!src.open(QIODevice::ReadOnly)) {
        if (why) *why = QString("无法读取源文件：\n%1\n%2").arg(from, src.errorString());
        return false;
    }
    QFile dst(to);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (why) {
            *why = QString("无法写入目标文件：\n%1\n%2\n\n"
                           "该目录可能被安全软件保护（例如 Windows 安全中心的『受控文件夹访问』）。")
                       .arg(to, dst.errorString());
        }
        return false;
    }
    while (!src.atEnd()) {
        const QByteArray chunk = src.read(1 << 20);
        if (chunk.isEmpty()) break;
        if (dst.write(chunk) != chunk.size()) {
            if (why) *why = QString("写入中断：%1").arg(dst.errorString());
            return false;
        }
    }
    return true;
}

void AccountDialog::onChangeAvatar()
{
    QString filter = "图片文件 (*.jpg *.jpeg *.png *.bmp *.gif)";
    QString path = QFileDialog::getOpenFileName(this, "选择头像", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), filter);
    if (path.isEmpty()) return;

    QPixmap pix(path);
    if (pix.isNull()) {
        QMessageBox::warning(this, "错误", "无法读取该图片文件");
        return;
    }

    // Copy to user data directory (same root as the account store)
    QString dataDir = UserAuth::dataRoot() + "/avatars";
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::warning(this, "错误", QString("无法创建头像目录：\n%1").arg(dataDir));
        return;
    }
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty()) ext = "png";
    QString destName = QString("avatar_%1_%2.%3").arg(m_auth->currentUser().username).arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);
    QString destPath = dataDir + "/" + destName;
    QString why;
    if (copyImageFile(path, destPath, &why) || QFile::exists(destPath)) {
        m_newAvatarPath = destPath;
        UserInfo tmp = m_auth->currentUser();
        tmp.avatarPath = destPath;
        m_avatarLabel->setPixmap(loadAvatar(tmp, 72));
    } else {
        QMessageBox::warning(this, "错误", "保存头像失败\n\n" + why);
    }
}

void AccountDialog::onChangeBackground()
{
    QString filter = "图片文件 (*.jpg *.jpeg *.png *.bmp *.gif)";
    QString path = QFileDialog::getOpenFileName(this, "选择封面背景", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), filter);
    if (path.isEmpty()) return;

    QPixmap pix(path);
    if (pix.isNull()) {
        QMessageBox::warning(this, "错误", "无法读取该图片文件");
        return;
    }

    // Copy to user data directory (same root as the account store)
    QString dataDir = UserAuth::dataRoot() + "/backgrounds";
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::warning(this, "错误", QString("无法创建背景目录：\n%1").arg(dataDir));
        return;
    }
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty()) ext = "png";
    QString destName = QString("bg_%1_%2.%3").arg(m_auth->currentUser().username).arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);
    QString destPath = dataDir + "/" + destName;
    QString why;
    if (copyImageFile(path, destPath, &why) || QFile::exists(destPath)) {
        m_newBackgroundPath = destPath;
        // Update full-dialog background
        QPixmap coverPix = QPixmap(destPath).scaled(380, 600, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPalette p = palette();
        p.setBrush(QPalette::Window, QBrush(coverPix));
        setPalette(p);
    } else {
        QMessageBox::warning(this, "错误", "保存背景失败\n\n" + why);
    }
}

void AccountDialog::onChangeUserId()
{
    // ID is now edited via the input field and saved in onSave
    // This method is kept for compatibility but no longer used
}

void AccountDialog::onSave()
{
    UserInfo user = m_auth->currentUser();
    QString error;

    // Update nickname if changed
    if (m_nicknameEdit->text().trimmed() != user.nickname) {
        if (!m_auth->updateNickname(user.username, m_nicknameEdit->text().trimmed(), &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    }

    // Update username if changed
    if (m_usernameEdit->text().trimmed() != user.username) {
        if (!m_auth->updateUsername(user.username, m_usernameEdit->text().trimmed(), &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    }

    // Update user ID if changed
    if (m_userIdEdit->text().trimmed() != user.userId) {
        if (!m_auth->updateUserId(m_auth->currentUser().username, m_userIdEdit->text().trimmed(), &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
        if (m_coverIdLabel) m_coverIdLabel->setText("ID: " + m_auth->currentUser().userId);
    }

    // Update email if changed
    if (m_emailEdit->text().trimmed().toLower() != user.email) {
        if (!m_auth->bindEmail(m_auth->currentUser().username, m_emailEdit->text().trimmed(), &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    }

    // Update avatar
    if (!m_newAvatarPath.isEmpty()) {
        if (!m_auth->updateAvatar(m_auth->currentUser().username, m_newAvatarPath, &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    }

    // Update background
    if (!m_newBackgroundPath.isEmpty()) {
        if (!m_auth->updateBackground(m_auth->currentUser().username, m_newBackgroundPath, &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    }

    QMessageBox::information(this, "成功", "账号信息已更新！");
    accept();
}

void AccountDialog::onChangePassword()
{
    bool ok;
    QString oldPwd = QInputDialog::getText(this, "修改密码", "原密码：", QLineEdit::Password, "", &ok);
    if (!ok || oldPwd.isEmpty()) return;

    QString newPwd = QInputDialog::getText(this, "修改密码", "新密码（6-32位，含字母和数字）：", QLineEdit::Password, "", &ok);
    if (!ok || newPwd.isEmpty()) return;

    QString confirmPwd = QInputDialog::getText(this, "修改密码", "确认新密码：", QLineEdit::Password, "", &ok);
    if (!ok || confirmPwd.isEmpty()) return;

    if (newPwd != confirmPwd) {
        QMessageBox::warning(this, "错误", "两次密码不一致");
        return;
    }

    QString error;
    if (m_auth->changePassword(m_auth->currentUser().username, oldPwd, newPwd, &error)) {
        QMessageBox::information(this, "成功", "密码修改成功！");
    } else {
        QMessageBox::warning(this, "失败", error);
    }
}

void AccountDialog::onLogout()
{
    if (QMessageBox::question(this, "退出登录", "确定退出当前账号？") == QMessageBox::Yes) {
        m_auth->logout();
        // Clear auto login
        QSettings settings("PureRefCanvas", "里与由");
        settings.remove("login/autoLogin");
        settings.remove("login/password");
        settings.setValue("login/remember", false);
        reject();
    }
}
