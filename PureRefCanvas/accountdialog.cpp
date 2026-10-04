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
#include <QFileInfo>
#include <QMovie>
#include <QSettings>

// 账户对话框的两套配色：无封面时整窗纯白（空白底），有封面（图片或动图）时
// 表单区用半透明深色玻璃浮在图上、文字转白。仅作用于本对话框，与软件启动页
// 背景完全无关。字号/圆角/内边距两态共用，颜色按 hasCover 切换。
static QString coverStyleQss(bool hasCover)
{
    QString qss;
    if (hasCover) {
        qss += "QWidget#formArea { background: rgba(15,15,25,0.72); border-top: 1px solid rgba(255,255,255,0.12); }\n";
        qss += "QLabel#coverName { color: #ffffff; font-size: 18px; font-weight: 600; }\n";
        qss += "QLabel#coverId { color: rgba(255,255,255,0.75); font-size: 11px; }\n";
        qss += "QLabel#bgHint { color: rgba(255,255,255,200); font-size: 11px; background: rgba(0,0,0,0.25); border-radius: 12px; padding: 2px 8px; }\n";
        qss += "QLabel#fieldLabel { color: rgba(255,255,255,0.7); font-size: 12px; padding-left: 2px; }\n";
        qss += "QLabel#metaLabel { color: rgba(255,255,255,0.5); font-size: 11px; padding-left: 2px; }\n";
        qss += "QLabel#warnLabel { color: #ffb84d; font-size: 11px; padding-left: 2px; }\n";
        qss += "QLineEdit { background: rgba(255,255,255,0.15); color: #fff; border: 1px solid rgba(255,255,255,0.2); border-radius: 4px; padding: 10px 14px; font-size: 14px; }\n";
        qss += "QLineEdit:focus { border-color: #07c160; background: rgba(255,255,255,0.2); }\n";
        qss += "QLineEdit:disabled { background: rgba(255,255,255,0.08); color: rgba(255,255,255,0.5); border: 1px solid rgba(255,255,255,0.1); }\n";
        qss += "QPushButton#grayBtn { background: rgba(255,255,255,0.15); color: #fff; border: 1px solid rgba(255,255,255,0.2); border-radius: 4px; padding: 10px; font-size: 14px; }\n";
        qss += "QPushButton#grayBtn:hover { background: rgba(255,255,255,0.25); }\n";
        qss += "QPushButton#redBtn { background: rgba(255,255,255,0.15); color: #ff6b6b; border: 1px solid rgba(255,107,107,0.3); border-radius: 4px; padding: 10px; font-size: 14px; }\n";
        qss += "QPushButton#redBtn:hover { background: rgba(255,107,107,0.2); }\n";
        qss += "QPushButton#avatarBtn { background: transparent; border: 2px solid rgba(255,255,255,0.8); padding: 0; border-radius: 36px; }\n";
        qss += "QPushButton#avatarBtn:hover { border-color: #07c160; }\n";
    } else {
        qss += "QWidget#formArea { background: #ffffff; border-top: 1px solid #e6e6ea; }\n";
        qss += "QLabel#coverName { color: #1f1f24; font-size: 18px; font-weight: 600; }\n";
        qss += "QLabel#coverId { color: rgba(0,0,0,0.55); font-size: 11px; }\n";
        qss += "QLabel#bgHint { color: rgba(0,0,0,0.55); font-size: 11px; background: rgba(0,0,0,0.06); border-radius: 12px; padding: 2px 8px; }\n";
        qss += "QLabel#fieldLabel { color: rgba(0,0,0,0.6); font-size: 12px; padding-left: 2px; }\n";
        qss += "QLabel#metaLabel { color: rgba(0,0,0,0.4); font-size: 11px; padding-left: 2px; }\n";
        qss += "QLabel#warnLabel { color: #b8860b; font-size: 11px; padding-left: 2px; }\n";
        qss += "QLineEdit { background: #f4f4f7; color: #1f1f24; border: 1px solid #dcdce2; border-radius: 4px; padding: 10px 14px; font-size: 14px; }\n";
        qss += "QLineEdit:focus { border-color: #07c160; background: #ffffff; }\n";
        qss += "QLineEdit:disabled { background: #ececef; color: rgba(0,0,0,0.4); border: 1px solid #e0e0e4; }\n";
        qss += "QPushButton#grayBtn { background: #ececef; color: #333333; border: 1px solid #dcdce2; border-radius: 4px; padding: 10px; font-size: 14px; }\n";
        qss += "QPushButton#grayBtn:hover { background: #e0e0e6; }\n";
        qss += "QPushButton#redBtn { background: #fdecec; color: #e04b4b; border: 1px solid #f5c2c2; border-radius: 4px; padding: 10px; font-size: 14px; }\n";
        qss += "QPushButton#redBtn:hover { background: #fbdcdc; }\n";
        qss += "QPushButton#avatarBtn { background: transparent; border: 2px solid #cfcfd6; padding: 0; border-radius: 36px; }\n";
        qss += "QPushButton#avatarBtn:hover { border-color: #07c160; }\n";
    }
    // 两态共用
    qss += "QPushButton#greenBtn { background-color: #07c160; color: #fff; border: none; border-radius: 4px; padding: 10px; font-size: 14px; font-weight: 500; }\n";
    qss += "QPushButton#greenBtn:hover { background-color: #06ad56; }\n";
    qss += "QPushButton#bgBtn { background: transparent; border: none; padding: 0; }\n";
    return qss;
}

AccountDialog::AccountDialog(UserAuth *auth, QWidget *parent)
    : QDialog(parent), m_auth(auth)
{
    setWindowTitle("账号信息");
    setFixedSize(380, 600);

    UserInfo user = m_auth->currentUser();
    QString displayName = user.nickname.isEmpty() ? user.username : user.nickname;

    // 默认空白（纯白）背景。账户封面（静态图或动图）由 applyCover 单独绘制，
    // 与软件启动页背景（backgroundPath）互不影响。
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setBrush(QPalette::Window, QBrush(Qt::white));
    setPalette(pal);

    applyCoverStyle(false);

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
    m_bgHintLabel = new QLabel("点击更换封面", bgBtn);
    m_bgHintLabel->setObjectName("bgHint");
    m_bgHintLabel->setGeometry(280, 8, 90, 24);
    m_bgHintLabel->setAlignment(Qt::AlignCenter);
    m_bgHintLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // 恢复默认背景：换过封面后一键回到纯白空白底（换封面本身仍可点击封面区）。
    // 无封面时置灰禁用，避免把“清除一个不存在的封面”做成可点的空操作。
    m_resetCoverBtn = new QPushButton("恢复默认背景", bgBtn);
    m_resetCoverBtn->setObjectName("resetCoverBtn");
    m_resetCoverBtn->setGeometry(248, 108, 124, 26);
    m_resetCoverBtn->setCursor(Qt::PointingHandCursor);
    m_resetCoverBtn->setToolTip("清除当前封面，回到默认纯白背景");
    m_resetCoverBtn->setEnabled(false);
    connect(m_resetCoverBtn, &QPushButton::clicked, this, &AccountDialog::onResetCover);

    // Avatar overlaid on cover
    m_avatarLabel = new QLabel;
    m_avatarLabel->setFixedSize(72, 72);
    m_avatarLabel->setScaledContents(true);
    m_avatarLabel->setStyleSheet("border-radius: 36px;");
    m_avatarLabel->setPixmap(loadAvatar(user, 72));

    m_avatarBtn = new QPushButton;
    m_avatarBtn->setObjectName("avatarBtn");
    m_avatarBtn->setFixedSize(72, 72);
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    m_avatarBtn->setToolTip("点击更换头像");
    m_avatarBtn->setParent(bgBtn);
    m_avatarBtn->move(24, 34); // 垂直居中: (140-72)/2 = 34
    QVBoxLayout *avatarBtnLayout = new QVBoxLayout(m_avatarBtn);
    avatarBtnLayout->setContentsMargins(0, 0, 0, 0);
    avatarBtnLayout->addWidget(m_avatarLabel);
    connect(m_avatarBtn, &QPushButton::clicked, this, &AccountDialog::onChangeAvatar);

    // Nickname on cover (vertical center with avatar)
    m_coverNameLabel = new QLabel(displayName, bgBtn);
    m_coverNameLabel->setObjectName("coverName");
    m_coverNameLabel->setGeometry(108, 44, 200, 28);
    m_coverNameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // User ID on cover
    m_coverIdLabel = new QLabel("ID: " + user.userId, bgBtn);
    m_coverIdLabel->setObjectName("coverId");
    m_coverIdLabel->setGeometry(108, 74, 200, 16);
    m_coverIdLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    mainLayout->addWidget(bgBtn);

    // ===== Form area =====
    QWidget *formWidget = new QWidget;
    formWidget->setObjectName("formArea");
    QVBoxLayout *formLayout = new QVBoxLayout(formWidget);
    formLayout->setContentsMargins(24, 20, 24, 20);
    formLayout->setSpacing(10);

    // Nickname
    QLabel *nickLabel = new QLabel("昵称");
    nickLabel->setObjectName("fieldLabel");
    formLayout->addWidget(nickLabel);
    m_nicknameEdit = new QLineEdit(displayName);
    m_nicknameEdit->setPlaceholderText("显示的昵称");
    formLayout->addWidget(m_nicknameEdit);

    // Username
    QLabel *userLabel = new QLabel("用户名（登录账号）");
    userLabel->setObjectName("fieldLabel");
    formLayout->addWidget(userLabel);
    m_usernameEdit = new QLineEdit(user.username);
    formLayout->addWidget(m_usernameEdit);

    // Email
    QLabel *emailLabel = new QLabel("邮箱");
    emailLabel->setObjectName("fieldLabel");
    formLayout->addWidget(emailLabel);
    m_emailEdit = new QLineEdit(user.email);
    m_emailEdit->setPlaceholderText("绑定邮箱用于找回密码");
    formLayout->addWidget(m_emailEdit);

    // User ID (editable - custom ID, 30-day cooldown)
    QLabel *idLabel = new QLabel("账户ID（字母开头，字母+数字，6-20位，每30天可改一次）");
    idLabel->setObjectName("fieldLabel");
    formLayout->addWidget(idLabel);
    m_userIdEdit = new QLineEdit(user.userId);
    m_userIdEdit->setPlaceholderText("自定义ID，如 liyu2024");
    formLayout->addWidget(m_userIdEdit);
    // Cooldown hint
    if (user.lastIdChangeTime.isValid()) {
        qint64 daysLeft = 30 - user.lastIdChangeTime.daysTo(QDateTime::currentDateTime());
        if (daysLeft > 0) {
            QLabel *cdHint = new QLabel(QString("距离下次可修改还有 %1 天").arg(daysLeft));
            cdHint->setObjectName("warnLabel");
            formLayout->addWidget(cdHint);
            m_userIdEdit->setEnabled(false);
        }
    }

    m_createdLabel = new QLabel("注册时间：" + user.createdAt.toString("yyyy-MM-dd hh:mm"));
    m_createdLabel->setObjectName("metaLabel");
    formLayout->addWidget(m_createdLabel);

    m_lastLoginLabel = new QLabel("上次登录：" + user.lastLogin.toString("yyyy-MM-dd hh:mm"));
    m_lastLoginLabel->setObjectName("metaLabel");
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

    // 打开时若已保存过账户封面，则按原样恢复（含动图重新播放）
    applyCover(user.accountCoverPath);
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

void AccountDialog::applyCoverStyle(bool hasCover)
{
    setStyleSheet(coverStyleQss(hasCover));
}

// 恢复入口只在有封面时可点；点击封面区换图与它互不冲突。
void AccountDialog::updateResetCoverUi(bool hasCover)
{
    if (m_resetCoverBtn) m_resetCoverBtn->setEnabled(hasCover);
}

void AccountDialog::setCoverBrush(const QPixmap &pix)
{
    QPalette p = palette();
    p.setBrush(QPalette::Window, QBrush(pix));
    setPalette(p);
}

// 应用账户封面。path 为空或文件缺失时回到纯白空白底；动图（GIF/WebP/MNG）用
// QMovie 逐帧播放，静态图直接绘制。只影响本对话框，不读取也不改动启动页背景。
void AccountDialog::applyCover(const QString &path)
{
    if (m_coverMovie) {
        m_coverMovie->stop();
        m_coverMovie->deleteLater();
        m_coverMovie = nullptr;
    }

    if (path.isEmpty() || !QFile::exists(path)) {
        applyCoverStyle(false);
        updateResetCoverUi(false);
        QPalette p = palette();
        p.setBrush(QPalette::Window, QBrush(Qt::white));
        setPalette(p);
        return;
    }

    // 先尝试按动图加载；QMovie 能识别则逐帧播放，否则回退静态图。
    QMovie *movie = new QMovie(path, QByteArray(), this);
    if (movie->isValid()) {
        m_coverMovie = movie;
        connect(movie, &QMovie::frameChanged, this, &AccountDialog::onCoverFrame);
        applyCoverStyle(true);
        updateResetCoverUi(true);
        movie->jumpToFrame(0);
        QPixmap first = movie->currentPixmap();
        if (!first.isNull())
            setCoverBrush(first.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        movie->start();
        return;
    }
    movie->deleteLater();

    QPixmap pix(path);
    if (!pix.isNull()) {
        applyCoverStyle(true);
        updateResetCoverUi(true);
        setCoverBrush(pix.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        return;
    }

    // 读取失败：保持空白底
    applyCoverStyle(false);
    updateResetCoverUi(false);
}

void AccountDialog::onCoverFrame()
{
    if (!m_coverMovie) return;
    QPixmap frame = m_coverMovie->currentPixmap();
    if (frame.isNull()) return;
    setCoverBrush(frame.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
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
    QString filter = "图片与动图 (*.jpg *.jpeg *.png *.bmp *.gif *.webp *.mng *.apng);;所有文件 (*.*)";
    QString path = QFileDialog::getOpenFileName(this, "选择账户封面背景", QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), filter);
    if (path.isEmpty()) return;

    // 动图用 QMovie 探测，静态图用 QPixmap 探测，任一可读取即接受。
    QMovie probe(path);
    const bool animOk = probe.isValid();
    QPixmap still(path);
    if (!animOk && still.isNull()) {
        QMessageBox::warning(this, "错误", "无法读取该图片文件");
        return;
    }

    // Copy to user data directory (same root as the account store)
    QString dataDir = UserAuth::dataRoot() + "/covers";
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::warning(this, "错误", QString("无法创建封面目录：\n%1").arg(dataDir));
        return;
    }
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext.isEmpty()) ext = "png";
    QString destName = QString("cover_%1_%2.%3").arg(m_auth->currentUser().username).arg(QDateTime::currentMSecsSinceEpoch()).arg(ext);
    QString destPath = dataDir + "/" + destName;
    QString why;
    if (copyImageFile(path, destPath, &why) || QFile::exists(destPath)) {
        m_newCoverPath = destPath;
        applyCover(destPath);
    } else {
        QMessageBox::warning(this, "错误", "保存封面失败\n\n" + why);
    }
}

void AccountDialog::onResetCover()
{
    // 立即回到纯白空白底（applyCover("") 会停掉动图并禁用本按钮），
    // 并记下“待清除”，等保存时真正写回账号数据。
    m_coverClearPending = true;
    m_newCoverPath.clear();
    applyCover(QString());
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

    // Update account cover (independent from the splash/startup background)
    if (!m_newCoverPath.isEmpty()) {
        if (!m_auth->updateAccountCover(m_auth->currentUser().username, m_newCoverPath, &error)) {
            QMessageBox::warning(this, "保存失败", error);
            return;
        }
    } else if (m_coverClearPending) {
        // 用户点了“恢复默认背景”：把账号里的封面路径清空（启动页背景不受影响）。
        if (!m_auth->clearAccountCover(m_auth->currentUser().username, &error)) {
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
