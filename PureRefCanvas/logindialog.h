#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "userauth.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QComboBox;
class QCheckBox;
class QProgressBar;
class QTimer;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(UserAuth *auth, QWidget *parent = nullptr);
    UserInfo loggedInUser() const { return m_loggedInUser; }

private slots:
    void doLogin();
    void doRegister();
    void doResetPassword();
    void doQuickLogin();
    void showError(const QString &msg);
    void clearError();
    void onConnectTick();

private:
    void setupConnectPage();
    void setupQuickPage();
    void setupLoginPage();
    void setupRegisterPage();
    void setupForgotPage();
    QPixmap makeAvatar(const QString &name, int size = 80) const;
    QPixmap loadUserAvatar(const QString &username, int size = 80) const;

    UserAuth *m_auth;
    UserInfo m_loggedInUser;
    QString m_lastUser;

    QStackedWidget *m_stack;
    QLabel *m_errorLabel;

    // Quick login (WeChat style)
    QLabel *m_avatarLabel;
    QLabel *m_quickNameLabel;
    QLabel *m_connectingLabel;
    QProgressBar *m_connectProgress;
    QTimer *m_connectTimer;
    int m_connectValue;
    QString m_savedAutoAccount;
    QString m_savedAutoPassword;
    bool m_autoLoginPreValidated;
    bool m_autoLoginWillSucceed;

    // Login
    QLineEdit *m_loginAccount;
    QLineEdit *m_loginPassword;
    QCheckBox *m_rememberPwd;
    QCheckBox *m_autoLogin;

    // Register
    QLineEdit *m_regUsername;
    QLineEdit *m_regEmail;
    QLineEdit *m_regPassword;
    QLineEdit *m_regConfirmPassword;
    QComboBox *m_regQuestion;
    QLineEdit *m_regAnswer;
    QCheckBox *m_agreeTerms;

    // Forgot
    QLineEdit *m_forgotEmail;
    QLineEdit *m_forgotAnswer;
    QLineEdit *m_forgotNewPassword;
    QLineEdit *m_forgotConfirmPassword;
};

#endif
