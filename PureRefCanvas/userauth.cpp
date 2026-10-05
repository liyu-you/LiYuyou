#include "userauth.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSaveFile>

namespace {
constexpr int kPbkdf2Iterations = 100000;
constexpr int kDerivedKeyBytes = 32;
// Fixed salt of the retired scheme, kept only so old hashes can still be verified
// and upgraded in place instead of forcing everyone to re-register.
constexpr char kLegacySalt[] = "LiYuYou_Salt_2026";
constexpr int kFailureLimit = 5;      // consecutive failures before a cooldown
constexpr int kLockoutSeconds = 30;   // how long guessing is refused afterwards

// True when a file can actually be created in this directory. Creating the
// directory is not enough: Windows security software can deny writes inside an
// existing, correctly-permissioned folder.
bool ensureWritableDir(const QString &dir)
{
    if (!QDir().mkpath(dir)) return false;
    QFile probe(dir + "/.liyiyou_write_test");
    if (!probe.open(QIODevice::WriteOnly)) return false;
    probe.write("x");
    probe.close();
    QFile::remove(probe.fileName());
    return true;
}

QDateTime storeMtime(const QString &dir)
{
    const QFileInfo fi(dir + "/users.json");
    return fi.exists() ? fi.lastModified() : QDateTime();
}
}

// PBKDF2-HMAC-SHA256, RFC 8018. Implemented here because this Qt build does not
// ship QPasswordDigestor; the test program checks it against the published
// vectors before it is trusted with real credentials.
QByteArray AuthCrypto::pbkdf2HmacSha256(const QByteArray &password, const QByteArray &salt,
                                        int iterations, int dkLen)
{
    constexpr int hLen = 32; // SHA-256 output
    if (iterations < 1 || dkLen < 1) return QByteArray();
    QByteArray derived;
    derived.reserve(dkLen);
    const int blocks = (dkLen + hLen - 1) / hLen;
    for (int block = 1; block <= blocks; ++block) {
        QByteArray index(4, '\0');
        index[0] = char((block >> 24) & 0xFF);
        index[1] = char((block >> 16) & 0xFF);
        index[2] = char((block >> 8) & 0xFF);
        index[3] = char(block & 0xFF);

        QByteArray u = QMessageAuthenticationCode::hash(salt + index, password,
                                                        QCryptographicHash::Sha256);
        QByteArray accumulator = u;
        for (int i = 1; i < iterations; ++i) {
            u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
            for (int j = 0; j < accumulator.size(); ++j)
                accumulator[j] = char(accumulator.at(j) ^ u.at(j));
        }
        derived += accumulator;
    }
    return derived.left(dkLen);
}

bool AuthCrypto::constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

UserAuth::UserAuth(QObject *parent) : QObject(parent)
{
    load();
    if (usingFallbackDataRoot() && m_storeError.isEmpty()) {
        // Say where the data actually lives, otherwise "my changes don't stick"
        // looks like a bug in the app rather than a blocked folder.
        m_storeError = QString("无法写入 Documents 目录（可能被 Windows 安全中心的『受控文件夹访问』拦截），"
                               "账号与背景数据已改用：%1").arg(dataRoot());
    }
}

QString UserAuth::throttleMessage(const QString &key) const
{
    const auto it = m_attempts.constFind(key);
    if (it == m_attempts.constEnd()) return {};
    if (!it->lockedUntil.isValid()) return {};
    const qint64 secs = QDateTime::currentDateTime().secsTo(it->lockedUntil);
    if (secs <= 0) return {}; // cooldown elapsed
    return QString("尝试过于频繁，请 %1 秒后再试").arg(secs);
}

void UserAuth::noteFailure(const QString &key)
{
    AttemptState &state = m_attempts[key];
    ++state.failures;
    if (state.failures >= kFailureLimit) {
        state.lockedUntil = QDateTime::currentDateTime().addSecs(kLockoutSeconds);
        state.failures = 0; // start counting again after the cooldown
    }
}

void UserAuth::clearFailures(const QString &key)
{
    m_attempts.remove(key);
}

bool UserAuth::isValidEmail(const QString &email)
{
    static const QRegularExpression re(
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    return re.match(email).hasMatch();
}

QString UserAuth::hashPassword(const QString &password)
{
    // PBKDF2-HMAC-SHA256 with a per-user random salt. The legacy scheme was a
    // single unsalted SHA-256 round, which made identical passwords hash
    // identically and was cheap to brute force; 100k iterations puts a real cost
    // on guessing while staying well under a perceptible login delay.
    QByteArray salt(16, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32 *>(salt.data()),
                                          salt.size() / sizeof(quint32));
    const QByteArray key = AuthCrypto::pbkdf2HmacSha256(password.toUtf8(), salt,
                                                        kPbkdf2Iterations, kDerivedKeyBytes);
    return QStringLiteral("pbkdf2-sha256$%1$%2$%3")
        .arg(kPbkdf2Iterations)
        .arg(QString::fromLatin1(salt.toBase64()))
        .arg(QString::fromLatin1(key.toBase64()));
}

bool UserAuth::verifyHash(const QString &value, const QString &storedHash, bool *needsRehash)
{
    if (needsRehash) *needsRehash = false;
    if (storedHash.isEmpty()) return false;

    // Legacy format: 64 hex chars of sha256(value + fixed salt).
    if (!storedHash.contains(QLatin1Char('$'))) {
        const QByteArray legacy = QCryptographicHash::hash(
            value.toUtf8() + QByteArray(kLegacySalt), QCryptographicHash::Sha256).toHex();
        const bool ok = (QString::fromLatin1(legacy) == storedHash);
        if (ok && needsRehash) *needsRehash = true; // upgrade on next successful check
        return ok;
    }

    const QStringList parts = storedHash.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != QLatin1String("pbkdf2-sha256")) {
        return false; // unknown scheme: fail closed rather than guess
    }
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations < 1000) return false;
    const QByteArray salt = QByteArray::fromBase64(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromBase64(parts.at(3).toLatin1());
    if (salt.isEmpty() || expected.isEmpty()) return false;
    const QByteArray actual = AuthCrypto::pbkdf2HmacSha256(value.toUtf8(), salt,
                                                           iterations, expected.size());
    return AuthCrypto::constantTimeEquals(actual, expected);
}

bool UserAuth::isStrongPassword(const QString &password, QString *hint)
{
    if (password.length() < 6) {
        if (hint) *hint = "密码至少6位";
        return false;
    }
    if (password.length() > 32) {
        if (hint) *hint = "密码最多32位";
        return false;
    }
    bool hasLetter = password.contains(QRegularExpression("[A-Za-z]"));
    bool hasDigit = password.contains(QRegularExpression("[0-9]"));
    if (!hasLetter || !hasDigit) {
        if (hint) *hint = "密码需包含字母和数字";
        return false;
    }
    return true;
}

AuthOk UserAuth::registerUser(const QString &username, const QString &email,
                              const QString &password, const QString &securityQuestion,
                              const QString &securityAnswer)
{
    if (username.trimmed().isEmpty())
        return std::unexpected("用户名不能为空");
    if (username.length() < 2 || username.length() > 20)
        return std::unexpected("用户名长度2-20位");
    if (!isValidEmail(email))
        return std::unexpected("邮箱格式不正确");
    QString hint;
    if (!isStrongPassword(password, &hint))
        return std::unexpected(hint);
    if (securityQuestion.isEmpty() || securityAnswer.isEmpty())
        return std::unexpected("请设置安全问题和答案");
    if (userExists(username))
        return std::unexpected("用户名已存在");
    if (emailExists(email))
        return std::unexpected("邮箱已被注册");

    UserInfo user;
    user.username = username.trimmed();
    user.nickname = username.trimmed(); // 默认昵称=用户名
    user.email = email.trimmed().toLower();
    user.passwordHash = hashPassword(password);
    user.securityQuestion = securityQuestion;
    user.securityAnswerHash = hashPassword(securityAnswer.toLower());
    user.createdAt = QDateTime::currentDateTime();
    user.lastLogin = QDateTime::currentDateTime();
    user.emailVerified = false;

    // 生成11位唯一数字ID
    QString newId;
    do {
        newId = QString::number(QDateTime::currentMSecsSinceEpoch() % 100000000000LL).rightJustified(11, '0');
        // 确保11位
        while (newId.length() < 11) {
            newId = QString("%1%2").arg(QRandomGenerator::global()->bounded(1, 10)).arg(newId).left(11);
        }
    } while (userIdExists(newId));
    user.userId = newId;

    m_users.append(user);
    if (!save()) {
        m_users.removeLast();
        return std::unexpected(m_storeHealthy
            ? QStringLiteral("账号数据保存失败，请检查磁盘空间或权限")
            : QStringLiteral("账号数据文件异常，已阻止写入以免覆盖现有账号"));
    }
    return {};
}

AuthOk UserAuth::login(const QString &account, const QString &password)
{
    const QString acc = account.trimmed();
    const QString throttleKey = "login:" + acc.toLower();
    if (QString cooldown = throttleMessage(throttleKey); !cooldown.isEmpty())
        return std::unexpected(cooldown);

    for (UserInfo &u : m_users) {
        if (u.username == acc || u.email == acc.toLower()) {
            bool needsRehash = false;
            if (!verifyHash(password, u.passwordHash, &needsRehash)) {
                noteFailure(throttleKey);
                // Surface the cooldown, otherwise it just looks like another
                // wrong-password failure and the user keeps hammering the button.
                if (QString cooldown = throttleMessage(throttleKey); !cooldown.isEmpty())
                    return std::unexpected(cooldown);
                return std::unexpected("密码错误");
            }
            // Legacy hash: upgrade it now that the password is known to be right.
            if (needsRehash) u.passwordHash = hashPassword(password);
            u.lastLogin = QDateTime::currentDateTime();
            m_currentUser = u;
            clearFailures(throttleKey);
            save();
            emit loginSuccess(m_currentUser);
            return {};
        }
    }
    return std::unexpected("用户不存在");
}

bool UserAuth::verifyLogin(const QString &account, const QString &password) const
{
    QString acc = account.trimmed();
    for (const UserInfo &u : m_users) {
        if (u.username == acc || u.email == acc.toLower()) {
            return verifyHash(password, u.passwordHash);
        }
    }
    return false;
}

bool UserAuth::loginAsUser(const QString &account)
{
    QString acc = account.trimmed();
    for (const UserInfo &u : m_users) {
        if (u.username == acc || u.email == acc.toLower()) {
            m_currentUser = u;
            // Update last login
            for (UserInfo &uu : m_users) {
                if (uu.username == u.username) {
                    uu.lastLogin = QDateTime::currentDateTime();
                    break;
                }
            }
            save();
            emit loginSuccess(m_currentUser);
            return true;
        }
    }
    return false;
}

AuthOk UserAuth::resetPassword(const QString &email, const QString &securityAnswer,
                               const QString &newPassword)
{
    if (!isValidEmail(email))
        return std::unexpected("邮箱格式不正确");
    QString hint;
    if (!isStrongPassword(newPassword, &hint))
        return std::unexpected(hint);

    const QString mail = email.trimmed().toLower();
    const QString throttleKey = "reset:" + mail;
    if (QString cooldown = throttleMessage(throttleKey); !cooldown.isEmpty())
        return std::unexpected(cooldown);

    for (UserInfo &u : m_users) {
        if (u.email == mail) {
            bool needsRehash = false;
            if (!verifyHash(securityAnswer.toLower(), u.securityAnswerHash, &needsRehash)) {
                noteFailure(throttleKey);
                if (QString cd = throttleMessage(throttleKey); !cd.isEmpty())
                    return std::unexpected(cd);
                return std::unexpected("安全答案错误");
            }
            // The security answer is a credential too: upgrade it the same way.
            if (needsRehash) u.securityAnswerHash = hashPassword(securityAnswer.toLower());
            u.passwordHash = hashPassword(newPassword);
            clearFailures(throttleKey);
            return commit();
        }
    }
    return std::unexpected("该邮箱未注册");
}

AuthOk UserAuth::bindEmail(const QString &username, const QString &newEmail)
{
    if (!isValidEmail(newEmail))
        return std::unexpected("邮箱格式不正确");
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            if (u.email == newEmail.trimmed().toLower())
                return std::unexpected("新邮箱与当前邮箱相同");
            if (emailExists(newEmail) && u.email != newEmail.trimmed().toLower())
                return std::unexpected("邮箱已被其他账号绑定");
            u.email = newEmail.trimmed().toLower();
            u.emailVerified = true;
            m_currentUser = u;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::changePassword(const QString &username, const QString &oldPassword,
                                const QString &newPassword)
{
    QString hint;
    if (!isStrongPassword(newPassword, &hint))
        return std::unexpected(hint);
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            if (!verifyHash(oldPassword, u.passwordHash))
                return std::unexpected("原密码错误");
            u.passwordHash = hashPassword(newPassword);
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateAvatar(const QString &username, const QString &avatarPath)
{
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.avatarPath = avatarPath;
            if (m_currentUser.username == username)
                m_currentUser.avatarPath = avatarPath;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateUsername(const QString &oldUsername, const QString &newUsername)
{
    QString name = newUsername.trimmed();
    if (name.length() < 2 || name.length() > 20)
        return std::unexpected("用户名长度需在2-20位之间");
    if (userExists(name))
        return std::unexpected("该用户名已被使用");
    for (UserInfo &u : m_users) {
        if (u.username == oldUsername) {
            u.username = name;
            if (m_currentUser.username == oldUsername)
                m_currentUser.username = name;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateNickname(const QString &username, const QString &newNickname)
{
    QString nick = newNickname.trimmed();
    if (nick.isEmpty())
        return std::unexpected("昵称不能为空");
    if (nick.length() > 20)
        return std::unexpected("昵称长度不能超过20位");
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.nickname = nick;
            if (m_currentUser.username == username)
                m_currentUser.nickname = nick;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateBio(const QString &username, const QString &newBio)
{
    QString bio = newBio.trimmed().left(30);
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.bio = bio;
            if (m_currentUser.username == username)
                m_currentUser.bio = bio;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateBackground(const QString &username, const QString &bgPath)
{
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.backgroundPath = bgPath;
            if (m_currentUser.username == username)
                m_currentUser.backgroundPath = bgPath;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::updateAccountCover(const QString &username, const QString &coverPath)
{
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.accountCoverPath = coverPath;
            if (m_currentUser.username == username)
                m_currentUser.accountCoverPath = coverPath;
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

AuthOk UserAuth::clearAccountCover(const QString &username)
{
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            u.accountCoverPath.clear();
            if (m_currentUser.username == username)
                m_currentUser.accountCoverPath.clear();
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

bool UserAuth::isValidUserId(const QString &id)
{
    // 必须以字母开头，只能包含字母和数字，长度6-20
    static QRegularExpression re("^[A-Za-z][A-Za-z0-9]{5,19}$");
    return re.match(id).hasMatch();
}

AuthOk UserAuth::updateUserId(const QString &username, const QString &newId)
{
    QString id = newId.trimmed();
    if (!isValidUserId(id))
        return std::unexpected("ID需以字母开头，仅含字母和数字，长度6-20位");
    if (userIdExists(id))
        return std::unexpected("该ID已被使用");
    for (UserInfo &u : m_users) {
        if (u.username == username) {
            // Check cooldown: 30 days
            if (u.lastIdChangeTime.isValid()) {
                qint64 daysLeft = 30 - u.lastIdChangeTime.daysTo(QDateTime::currentDateTime());
                if (daysLeft > 0) {
                    return std::unexpected(QString("ID修改过于频繁，请%1天后再试（每30天可修改一次）").arg(daysLeft));
                }
            }
            u.userId = id;
            u.lastIdChangeTime = QDateTime::currentDateTime();
            if (m_currentUser.username == username) {
                m_currentUser.userId = id;
                m_currentUser.lastIdChangeTime = u.lastIdChangeTime;
            }
            return commit();
        }
    }
    return std::unexpected("用户不存在");
}

void UserAuth::logout()
{
    m_currentUser = UserInfo();
    emit logoutSuccess();
}

bool UserAuth::userExists(const QString &username) const
{
    for (const UserInfo &u : m_users) {
        if (u.username == username.trimmed()) return true;
    }
    return false;
}

bool UserAuth::emailExists(const QString &email) const
{
    QString e = email.trimmed().toLower();
    for (const UserInfo &u : m_users) {
        if (u.email == e) return true;
    }
    return false;
}

bool UserAuth::userIdExists(const QString &userId) const
{
    for (const UserInfo &u : m_users) {
        if (u.userId == userId) return true;
    }
    return false;
}

QString UserAuth::displayName(const QString &username) const
{
    for (const UserInfo &u : m_users) {
        if (u.username == username) {
            return u.nickname.isEmpty() ? u.username : u.nickname;
        }
    }
    return username;
}

QString UserAuth::avatarPathForUser(const QString &username) const
{
    for (const UserInfo &u : m_users) {
        if (u.username == username) {
            return u.avatarPath;
        }
    }
    return QString();
}

AuthOk UserAuth::commit()
{
    if (save()) return {};
    return std::unexpected(m_storeHealthy
        ? QStringLiteral("保存失败，请检查磁盘空间或权限")
        : QStringLiteral("账号数据文件异常，已阻止写入以免覆盖现有账号"));
}

bool UserAuth::load()
{
    const QString path = dataPath();
    m_storeHealthy = true;
    m_storeError.clear();
    m_users.clear();

    if (!QFile::exists(path))
        return true; // first run: nothing to load, and that is a healthy state

    QByteArray raw;
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            m_storeHealthy = false;
            m_storeError = QString("无法读取账号数据文件：%1").arg(path);
            return false;
        }
        raw = file.readAll();
    }

    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        // Before declaring the store damaged, try the last good copy.
        const QString backup = path + ".bak";
        QByteArray fallback;
        if (QFile::exists(backup)) {
            QFile bf(backup);
            if (bf.open(QIODevice::ReadOnly)) fallback = bf.readAll();
        }
        const QJsonDocument fallbackDoc = QJsonDocument::fromJson(fallback);
        if (!fallback.isEmpty() && fallbackDoc.isArray()) {
            doc = fallbackDoc;
            m_storeError = "主账号文件损坏，已从备份恢复";
            // Repair in place so the main file and the backup agree again.
            QSaveFile repair(path);
            if (repair.open(QIODevice::WriteOnly)) {
                repair.write(fallback);
                repair.commit();
            }
        } else {
            // Quarantine rather than delete: never destroy what we cannot read,
            // and refuse to write later on so accounts cannot be wiped.
            const QString quarantine = path + ".corrupt-"
                + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
            const bool moved = QFile::rename(path, quarantine);
            m_storeHealthy = false;
            m_storeError = moved
                ? QString("账号数据文件损坏，已隔离为 %1；本次不写入以免覆盖").arg(QFileInfo(quarantine).fileName())
                : QString("账号数据文件损坏且无法隔离（%1）；本次不写入以免覆盖").arg(path);
            return false;
        }
    }

    bool migrated = false;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject obj = v.toObject();
        UserInfo u;
        u.username = obj["username"].toString();
        u.nickname = obj["nickname"].toString(u.username);
        u.userId = obj["userId"].toString();
        u.email = obj["email"].toString();
        u.passwordHash = obj["passwordHash"].toString();
        u.securityQuestion = obj["securityQuestion"].toString();
        u.securityAnswerHash = obj["securityAnswerHash"].toString();
        u.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        u.lastLogin = QDateTime::fromString(obj["lastLogin"].toString(), Qt::ISODate);
        u.lastIdChangeTime = QDateTime::fromString(obj["lastIdChangeTime"].toString(), Qt::ISODate);
        u.emailVerified = obj["emailVerified"].toBool(false);
        u.avatarPath = obj["avatarPath"].toString();
        u.backgroundPath = obj["backgroundPath"].toString();
        u.accountCoverPath = obj["accountCoverPath"].toString();
        u.bio = obj["bio"].toString();
        // 迁移：旧用户没有userId，自动生成
        if (u.userId.isEmpty()) {
            QString newId;
            do {
                newId = QString::number(QDateTime::currentMSecsSinceEpoch() % 100000000000LL).rightJustified(11, '0');
                while (newId.length() < 11) {
                    newId = QString("%1%2").arg(QRandomGenerator::global()->bounded(1, 10)).arg(newId).left(11);
                }
            } while (userIdExists(newId));
            u.userId = newId;
            migrated = true;
        }
        // 迁移：旧用户没有nickname，默认等于username
        if (u.nickname.isEmpty()) {
            u.nickname = u.username;
            migrated = true;
        }
        m_users.append(u);
    }
    if (migrated) save();
    return true;
}

bool UserAuth::save()
{
    // Refuse to write when the existing file could not be read: overwriting it is
    // how every account would be silently replaced by whatever is in memory now.
    if (!m_storeHealthy) return false;

    QJsonArray arr;
    for (const UserInfo &u : m_users) {
        QJsonObject obj;
        obj["username"] = u.username;
        obj["nickname"] = u.nickname;
        obj["userId"] = u.userId;
        obj["email"] = u.email;
        obj["passwordHash"] = u.passwordHash;
        obj["securityQuestion"] = u.securityQuestion;
        obj["securityAnswerHash"] = u.securityAnswerHash;
        obj["createdAt"] = u.createdAt.toString(Qt::ISODate);
        obj["lastLogin"] = u.lastLogin.toString(Qt::ISODate);
        obj["lastIdChangeTime"] = u.lastIdChangeTime.toString(Qt::ISODate);
        obj["emailVerified"] = u.emailVerified;
        obj["avatarPath"] = u.avatarPath;
        obj["backgroundPath"] = u.backgroundPath;
        obj["accountCoverPath"] = u.accountCoverPath;
        obj["bio"] = u.bio;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    const QString path = dataPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Keep the last good copy so a damaged file can still be recovered.
    if (QFile::exists(path)) {
        const QString backup = path + ".bak";
        QFile::remove(backup);
        QFile::copy(path, backup);
    }

    // QSaveFile writes a temporary file and renames it into place, so an
    // interrupted or crashing write can no longer leave a half-written
    // users.json behind (which previously meant losing every account).
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

QString UserAuth::dataRoot()
{
    static QString resolved;
    if (!resolved.isEmpty()) return resolved;

    const QByteArray overrideDir = qgetenv("LIYIYOU_DATA_DIR");
    if (!overrideDir.isEmpty()) {
        resolved = QString::fromLocal8Bit(overrideDir);
        return resolved;
    }

    const QString primary = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + "/PureRefCanvas";
    QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (fallback.isEmpty())
        fallback = QDir::tempPath() + "/PureRefCanvas";

    const bool primaryWritable = ensureWritableDir(primary);
    const QDateTime primaryStore = storeMtime(primary);
    const QDateTime fallbackStore = storeMtime(fallback);

    // Prefer Documents, where the data has always lived. Only move to the fallback
    // when Documents cannot be written, or when Documents holds no store while the
    // fallback does - so accounts created during a fallback are never lost once the
    // original location becomes writable again.
    if (primaryWritable && (primaryStore.isValid() || !fallbackStore.isValid()))
        resolved = primary;
    else
        resolved = fallback;
    return resolved;
}

bool UserAuth::usingFallbackDataRoot()
{
    if (!qgetenv("LIYIYOU_DATA_DIR").isEmpty()) return false;
    return dataRoot() != QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + "/PureRefCanvas";
}

QString UserAuth::dataPath() const
{
    return dataRoot() + "/users.json";
}
