#ifndef USERAUTH_H
#define USERAUTH_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QByteArray>

// Credential hashing primitives. Exposed (rather than file-static) so the
// credential path can be checked against the published PBKDF2 test vectors.
namespace AuthCrypto {
QByteArray pbkdf2HmacSha256(const QByteArray &password, const QByteArray &salt,
                            int iterations, int dkLen);
// Length-independent, branch-free comparison of two derived keys.
bool constantTimeEquals(const QByteArray &a, const QByteArray &b);
}

struct UserInfo {
    QString userId;       // 11位数字账户ID，系统自动生成
    QString username;     // 登录账号
    QString nickname;     // 用户昵称（显示用）
    QString email;
    QString passwordHash;
    QString securityQuestion;
    QString securityAnswerHash;
    QString avatarPath;
    QString backgroundPath;
    QDateTime createdAt;
    QDateTime lastLogin;
    QDateTime lastIdChangeTime;
    bool emailVerified = false;
};

class UserAuth : public QObject
{
    Q_OBJECT
public:
    explicit UserAuth(QObject *parent = nullptr);

    static bool isValidEmail(const QString &email);
    static QString hashPassword(const QString &password);
    static bool isStrongPassword(const QString &password, QString *hint = nullptr);
    // Accepts both the current PBKDF2 format and the legacy unsalted SHA-256 one;
    // reports via needsRehash when a stored hash is still legacy so the caller can
    // transparently upgrade it after a successful check.
    static bool verifyHash(const QString &value, const QString &storedHash,
                           bool *needsRehash = nullptr);

    bool registerUser(const QString &username, const QString &email,
                      const QString &password, const QString &securityQuestion,
                      const QString &securityAnswer, QString *error = nullptr);
    bool login(const QString &account, const QString &password, QString *error = nullptr);
    bool verifyLogin(const QString &account, const QString &password) const; // 纯验证，无副作用
    bool loginAsUser(const QString &account); // 预验证成功后快速登录（跳过密码验证）
    bool resetPassword(const QString &email, const QString &securityAnswer,
                       const QString &newPassword, QString *error = nullptr);
    bool bindEmail(const QString &username, const QString &newEmail, QString *error = nullptr);
    bool changePassword(const QString &username, const QString &oldPassword,
                        const QString &newPassword, QString *error = nullptr);
    bool updateAvatar(const QString &username, const QString &avatarPath, QString *error = nullptr);
    bool updateUsername(const QString &oldUsername, const QString &newUsername, QString *error = nullptr);
    bool updateNickname(const QString &username, const QString &newNickname, QString *error = nullptr);
    bool updateBackground(const QString &username, const QString &bgPath, QString *error = nullptr);
    bool updateUserId(const QString &username, const QString &newId, QString *error = nullptr);
    static bool isValidUserId(const QString &id);

    UserInfo currentUser() const { return m_currentUser; }
    bool isLoggedIn() const { return !m_currentUser.username.isEmpty(); }
    void logout();

    // Directory holding users.json plus the per-user assets referenced by it
    // (avatars, cover backgrounds). Normally the Documents location; it falls back
    // to the application data directory when Documents cannot be written on this
    // machine (e.g. Defender's Controlled Folder Access blocking an unrecognised
    // program), instead of failing every save with a bare "保存失败".
    static QString dataRoot();
    static bool usingFallbackDataRoot();

    // False when users.json could not be read/parsed. While that is the case the
    // store refuses to write, because overwriting a file we failed to read is how
    // every account would be silently wiped.
    bool storeHealthy() const { return m_storeHealthy; }
    QString storeError() const { return m_storeError; }

    bool userExists(const QString &username) const;
    bool emailExists(const QString &email) const;
    bool userIdExists(const QString &userId) const;
    QString displayName(const QString &username) const;
    QString avatarPathForUser(const QString &username) const;

signals:
    void loginSuccess(const UserInfo &user);
    void logoutSuccess();

private:
    bool load();
    bool save();
    // save() plus a user-facing reason when the write could not happen
    bool commit(QString *error);
    QString dataPath() const;

    // Failed-attempt throttling (in memory only: a restart clears it, which is
    // enough to stop scripted guessing without ever locking a real user out).
    struct AttemptState {
        int failures = 0;
        QDateTime lockedUntil;
    };
    bool throttled(const QString &key, QString *error) const;
    void noteFailure(const QString &key);
    void clearFailures(const QString &key);

    QList<UserInfo> m_users;
    UserInfo m_currentUser;
    QHash<QString, AttemptState> m_attempts;
    bool m_storeHealthy = true;
    QString m_storeError;
};

#endif
