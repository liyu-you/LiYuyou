#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>
#include "userauth.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QMovie;

class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AccountDialog(UserAuth *auth, QWidget *parent = nullptr);
    UserInfo updatedUser() const { return m_auth->currentUser(); }

private slots:
    void onChangeAvatar();
    void onChangeBackground();
    void onResetCover();
    void onChangeUserId();
    void onSave();
    void onChangePassword();
    void onLogout();

private:
    QPixmap loadAvatar(const UserInfo &user, int size) const;
    // 应用账户封面：静态图直接绘制，GIF/WebP 等动图用 QMovie 逐帧播放；
    // 无封面时对话框为纯白背景。只影响本对话框，不触碰启动页背景。
    void applyCover(const QString &path);
    void setCoverBrush(const QPixmap &pix);
    void onCoverFrame();
    void applyCoverStyle(bool hasCover);
    // 恢复封面按钮的可用性：仅当当前有封面时可点，否则置灰。
    void updateResetCoverUi(bool hasCover);

    UserAuth *m_auth;
    QLabel *m_avatarLabel;
    QPushButton *m_avatarBtn;
    QLabel *m_coverNameLabel;
    QLabel *m_coverIdLabel;
    QLabel *m_bgHintLabel;
    QPushButton *m_resetCoverBtn = nullptr;
    QMovie *m_coverMovie = nullptr;
    QLineEdit *m_nicknameEdit;
    QLineEdit *m_bioEdit;
    QLineEdit *m_userIdEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_emailEdit;
    QLabel *m_createdLabel;
    QLabel *m_lastLoginLabel;
    QString m_newAvatarPath;
    QString m_newCoverPath;
    // 用户点了“恢复默认背景”但尚未保存：保存时需清除账号里的封面路径。
    bool m_coverClearPending = false;
};

#endif
