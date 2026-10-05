#ifndef USERAUTH_H
#define USERAUTH_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QHash>
#include <QByteArray>
#include <expected>

// C++23: Result<T, QString> replaces the old "bool method(..., QString *error)"
// C-style out-param idiom. Success carries T (or nothing for Result<void>);
// failure carries the human-readable Chinese message the UI used to show.
// Callers use `if (result) {...} else { showError(result.error()); }` and can
// no longer accidentally drop the error by passing nullptr.
template <typename T>
using AuthResult = std::expected<T, QString>;
using AuthOk     = std::expected<void, QString>;

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
    QString backgroundPath;   // 启动页背景（splash）
    QString accountCoverPath; // 账户对话框封面（独立于启动页）
    QString bio;              // 个性签名（最多 30 字）
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

    // Pure predicates / crypto helpers — no error channel, kept as-is.
    static bool isValidEmail(const QString &email);
    static QString hashPassword(const QString &password);
    static bool isStrongPassword(const QString &password, QString *hint = nullptr);
    // Accepts both the current PBKDF2 format and the legacy unsalted SHA-256 one;
    // reports via needsRehash when a stored hash is still legacy so the caller can
    // transparently upgrade it after a successful check.
    static bool verifyHash(const QString &value, const QString &storedHash,
                           bool *needsRehash = nullptr);
    static bool isValidUserId(const QString &id);

    // ---- Fallible mutations (C++23 expected) ----
    AuthOk registerUser(const QString &username, const QString &email,
                        const QString &password, const QString &securityQuestion,
                        const QString &securityAnswer);
    AuthOk login(const QString &account, const QString &password);
    AuthOk resetPassword(const QString &email, const QString &securityAnswer,
                         const QString &newPassword);
    AuthOk bindEmail(const QString &username, const QString &newEmail);
    AuthOk changePassword(const QString &username, const QString &oldPassword,
                         const QString &newPassword);
    AuthOk updateAvatar(const QString &username, const QString &avatarPath);
    AuthOk updateUsername(const QString &oldUsername, const QString &newUsername);
    AuthOk updateNickname(const QString &username, const QString &newNickname);
    AuthOk updateBio(const QString &username, const QString &newBio);
    AuthOk updateBackground(const QString &username, const QString &bgPath);
    AuthOk updateAccountCover(const QString &username, const QString &coverPath);
    AuthOk clearAccountCover(const QString &username);
    AuthOk updateUserId(const QString &username, const QString &newId);

    // Pure verification, no side effects. No error channel.
    bool verifyLogin(const QString &account, const QString &password) const;
    // Pre-verified fast path (auto-login / loginAsUser): no password check, no
    // error string. Returns false when the account does not exist.
    bool loginAsUser(const QString &account);

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
    // save() plus a user-facing reason when the write could not happen.
    AuthOk commit();
    QString dataPath() const;

    // Failed-attempt throttling (in memory only: a restart clears it, which is
    // enough to stop scripted guessing without ever locking a real user out).
    struct AttemptState {
        int failures = 0;
        QDateTime lockedUntil;
    };
    // Returns a non-empty cooldown message when the key is locked out, else "".
    QString throttleMessage(const QString &key) const;
    void noteFailure(const QString &key);
    void clearFailures(const QString &key);

    QList<UserInfo> m_users;
    UserInfo m_currentUser;
    QHash<QString, AttemptState> m_attempts;
    bool m_storeHealthy = true;
    QString m_storeError;
};

#endif // USERAUTH_H
