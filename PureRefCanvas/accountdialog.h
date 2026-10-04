#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>
#include "userauth.h"

class QLabel;
class QLineEdit;
class QPushButton;

class AccountDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AccountDialog(UserAuth *auth, QWidget *parent = nullptr);
    UserInfo updatedUser() const { return m_auth->currentUser(); }

private slots:
    void onChangeAvatar();
    void onChangeBackground();
    void onChangeUserId();
    void onSave();
    void onChangePassword();
    void onLogout();

private:
    QPixmap loadAvatar(const UserInfo &user, int size) const;

    UserAuth *m_auth;
    QLabel *m_avatarLabel;
    QLabel *m_backgroundLabel;
    QLabel *m_coverIdLabel;
    QLineEdit *m_nicknameEdit;
    QLineEdit *m_userIdEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_emailEdit;
    QLabel *m_createdLabel;
    QLabel *m_lastLoginLabel;
    QString m_newAvatarPath;
    QString m_newBackgroundPath;
};

#endif
