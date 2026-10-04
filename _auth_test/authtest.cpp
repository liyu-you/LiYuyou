// Credential-path test for the login rework.
//
// Covers the things that could lock a real user out of their own accounts:
//   * PBKDF2-HMAC-SHA256 correctness against the published test vectors
//   * legacy (unsalted SHA-256) hashes still verify, and are upgraded in place
//   * register/login round trip, wrong password rejected
//   * failed-attempt throttling
//   * a damaged users.json can never wipe the accounts (quarantine + refuse write)
//   * a damaged users.json is recovered from users.json.bak
//
// Runs against LIYIYOU_DATA_DIR, never the real Documents\PureRefCanvas store.

#include "userauth.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDebug>

static int g_failures = 0;

static void check(bool ok, const QString &what)
{
    qInfo().noquote() << (ok ? "  PASS  " : "  FAIL  ") << what;
    if (!ok) ++g_failures;
}

static QString readStore(const QString &dir)
{
    QFile f(dir + "/users.json");
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromUtf8(f.readAll());
}

static QString firstPasswordHash(const QString &dir)
{
    const QJsonDocument doc = QJsonDocument::fromJson(readStore(dir).toUtf8());
    if (!doc.isArray() || doc.array().isEmpty()) return QString();
    return doc.array().first().toObject()["passwordHash"].toString();
}

// Write a raw file (used for users.json and its .bak); reports failures instead
// of silently doing nothing.
static void writeRawFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "cannot write" << path << ":" << f.errorString();
        return;
    }
    f.write(content);
    f.close();
}

static void writeStore(const QString &dir, const QByteArray &content)
{
    writeRawFile(dir + "/users.json", content);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // Fresh-process inspection of where the app would put its data. Writes no
    // store data, so it is safe to run against the real environment.
    if (argc > 1 && QString::fromLatin1(argv[1]) == QLatin1String("--root-only")) {
        const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                             + "/PureRefCanvas";
        QDir().mkpath(docs);
        QFile probe(docs + "/.test_writable");
        const bool docsWritable = probe.open(QIODevice::WriteOnly);
        if (docsWritable) { probe.close(); QFile::remove(probe.fileName()); }
        const QString root = UserAuth::dataRoot();
        qInfo().noquote() << "Documents writable :" << docsWritable
                          << "\nresolved dataRoot  :" << root
                          << "\nusingFallback      :" << UserAuth::usingFallbackDataRoot();
        return 0;
    }

    // Scratch dir: overridable, because a sandboxed runner may not be allowed to
    // write to the system temp directory.
    const QString dir = qEnvironmentVariable("LIYIYOU_TEST_DIR",
                                             QDir::tempPath() + "/liyiyou_auth_test");
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    qputenv("LIYIYOU_DATA_DIR", dir.toLocal8Bit());
    qInfo().noquote() << "scratch store:" << dir;

    // ---- 1. PBKDF2-HMAC-SHA256 against the published vectors ---------------
    qInfo().noquote() << "\n[1] PBKDF2-HMAC-SHA256 test vectors";
    struct Vector { const char *pwd; const char *salt; int iter; int dkLen; const char *hex; };
    const Vector vectors[] = {
        {"password", "salt", 1, 32,
         "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
        {"password", "salt", 2, 32,
         "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
        {"password", "salt", 4096, 32,
         "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"},
        {"passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40,
         "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9"},
    };
    for (const Vector &v : vectors) {
        const QByteArray got = AuthCrypto::pbkdf2HmacSha256(QByteArray(v.pwd), QByteArray(v.salt),
                                                           v.iter, v.dkLen);
        check(QString::fromLatin1(got.toHex()) == QLatin1String(v.hex),
              QString("PBKDF2 c=%1 dkLen=%2").arg(v.iter).arg(v.dkLen));
    }

    // ---- 2. register / login round trip ------------------------------------
    qInfo().noquote() << "\n[2] register + login";
    {
        UserAuth auth;
        QString error;
        const bool registered = auth.registerUser("alice", "alice@example.com", "Passw0rd1",
                                                 "你的出生地？", "Shanghai", &error);
        check(registered, "register alice (" + error + ")");
        const QString hash = firstPasswordHash(dir);
        check(hash.startsWith("pbkdf2-sha256$100000$"), "stored hash uses PBKDF2 with a salt");

        // Same password, second account: the per-user salt must make the stored
        // hashes differ (that is the whole point of salting).
        QString error2;
        auth.registerUser("alice2", "alice2@example.com", "Passw0rd1", "你的出生地？", "Shanghai", &error2);
        const QJsonArray arr = QJsonDocument::fromJson(readStore(dir).toUtf8()).array();
        check(arr.size() == 2 && arr.at(0).toObject()["passwordHash"].toString()
                                    != arr.at(1).toObject()["passwordHash"].toString(),
              "identical passwords produce different hashes (per-user salt)");

        check(auth.login("alice", "Passw0rd1", &error), "login with correct password");
        check(!auth.login("alice", "wrongpass", &error), "login with wrong password rejected");
        check(auth.login("alice@example.com", "Passw0rd1", &error), "login by email works");
    }

    // ---- 3. legacy hash verifies and is upgraded in place -------------------
    qInfo().noquote() << "\n[3] legacy hash migration";
    {
        const QByteArray legacy = QCryptographicHash::hash(
            QByteArray("OldPass123") + QByteArray("LiYuYou_Salt_2026"),
            QCryptographicHash::Sha256).toHex();
        QJsonObject user;
        user["username"] = "legacy";
        user["nickname"] = "legacy";
        user["email"] = "legacy@example.com";
        user["passwordHash"] = QString::fromLatin1(legacy);
        user["securityAnswerHash"] = QString::fromLatin1(legacy);
        user["userId"] = "00000000001";
        QJsonArray arr;
        arr.append(user);
        writeStore(dir, QJsonDocument(arr).toJson());

        UserAuth auth;
        QString error;
        check(auth.storeHealthy(), "legacy store loads cleanly");
        check(!auth.login("legacy", "WrongOne1", &error), "legacy user rejects a wrong password");
        check(auth.login("legacy", "OldPass123", &error), "legacy password still logs in");
        check(firstPasswordHash(dir).startsWith("pbkdf2-sha256$"),
              "legacy hash was upgraded to PBKDF2 after a successful login");
    }

    // ---- 4. failed-attempt throttling ---------------------------------------
    qInfo().noquote() << "\n[4] throttling";
    {
        // Fresh store: step 3 replaced the file with a single legacy account.
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        UserAuth auth;
        QString error;
        auth.registerUser("carol", "carol@example.com", "Passw0rd1", "q", "a", &error);
        for (int i = 0; i < 5; ++i)
            auth.login("carol", "nope", &error);
        const bool refused = !auth.login("carol", "Passw0rd1", &error);
        check(refused, "after 5 failures even the right password is refused");
        check(error.contains(QString::fromUtf8("频繁")),
              QString::fromUtf8("the cooldown is explained to the user"));
    }

    // ---- 5. damaged store: quarantine, never wipe ---------------------------
    qInfo().noquote() << "\n[5] damaged store safety";
    {
        // Damage the backup too, otherwise recovery-from-backup (test 6) kicks in
        // and the store stays healthy - which is correct, but not what this step
        // is about.
        const QByteArray garbage = "{ this is not json";
        writeStore(dir, garbage);
        writeRawFile(dir + "/users.json.bak", garbage);
        UserAuth auth;
        QString error;
        check(!auth.storeHealthy(), "damaged store reported unhealthy");
        const bool refusedRegister = auth.registerUser("bob", "bob@example.com", "Passw0rd1",
                                                      "q", "a", &error);
        check(!refusedRegister, "registering is refused (" + error + ")");
        const QStringList quarantined = QDir(dir).entryList(QStringList() << "users.json.corrupt-*",
                                                           QDir::Files);
        check(!quarantined.isEmpty(), "damaged file kept as a quarantine copy");
        bool preserved = false;
        if (!quarantined.isEmpty()) {
            QFile q(dir + "/" + quarantined.first());
            if (q.open(QIODevice::ReadOnly)) preserved = (q.readAll() == garbage);
        }
        check(preserved, "quarantined copy still holds the original bytes (nothing destroyed)");
    }

    // ---- 6. recovery from backup -------------------------------------------
    qInfo().noquote() << "\n[6] recovery from users.json.bak";
    {
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        writeStore(dir, "{ broken again");
        // A good backup holding one account.
        QJsonObject user;
        user["username"] = "restored";
        user["nickname"] = "restored";
        user["email"] = "restored@example.com";
        user["passwordHash"] = UserAuth::hashPassword("Passw0rd1");
        user["userId"] = "00000000002";
        QJsonArray arr;
        arr.append(user);
        QFile bak(dir + "/users.json.bak");
        bak.open(QIODevice::WriteOnly | QIODevice::Truncate);
        bak.write(QJsonDocument(arr).toJson());
        bak.close();

        UserAuth auth;
        QString error;
        check(auth.storeHealthy(), "store healthy after recovering from the backup");
        check(auth.storeError().contains("备份"), "recovery is reported: " + auth.storeError());
        check(auth.login("restored", "Passw0rd1", &error), "account from the backup can log in");
    }

    // ---- 8. account cover is independent from the splash background ---------
    qInfo().noquote() << "\n[8] account cover vs startup background";
    {
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);
        UserAuth auth;
        QString error;
        check(auth.registerUser("dave", "dave@example.com", "Passw0rd1", "q", "a", &error),
              "register dave (" + error + ")");
        check(auth.login("dave", "Passw0rd1", &error), "login dave");

        // Setting the account cover must not touch the startup background.
        check(auth.updateAccountCover("dave", "C:/fake/covers/cover.gif", &error),
              "set account cover (" + error + ")");
        check(auth.currentUser().accountCoverPath == "C:/fake/covers/cover.gif",
              "cover stored on the current user");
        check(auth.currentUser().backgroundPath.isEmpty(),
              "startup background untouched by the cover");

        // Setting the startup background must not touch the account cover.
        check(auth.updateBackground("dave", "C:/fake/splash.png", &error),
              "set startup background (" + error + ")");
        check(auth.currentUser().backgroundPath == "C:/fake/splash.png",
              "startup background stored");
        check(auth.currentUser().accountCoverPath == "C:/fake/covers/cover.gif",
              "cover untouched by the startup background");

        // Both fields must survive a reload from disk.
        const QString store = readStore(dir);
        check(store.contains("cover.gif") && store.contains("splash.png"),
              "both paths persisted into users.json");
        {
            UserAuth auth2;
            check(auth2.login("dave", "Passw0rd1", &error), "re-login after reload");
            check(auth2.currentUser().accountCoverPath == "C:/fake/covers/cover.gif",
                  "cover path survived a fresh load");
            check(auth2.currentUser().backgroundPath == "C:/fake/splash.png",
                  "startup background survived a fresh load");

            // Clearing the cover leaves the startup background alone.
            check(auth2.clearAccountCover("dave", &error), "clear cover (" + error + ")");
            check(auth2.currentUser().accountCoverPath.isEmpty(), "cover cleared");
            check(auth2.currentUser().backgroundPath == "C:/fake/splash.png",
                  "startup background kept after clearing the cover");
        }

        // Unknown users are refused.
        check(!auth.updateAccountCover("ghost", "C:/x.gif", &error),
              "cover update refused for an unknown user");
    }

    // ---- 7. data-root selection ------------------------------------------
    // dataRoot() caches its decision for the process, so this has to run in a
    // fresh process; --root-only does exactly that and writes no store data.
    qInfo().noquote() << "\n[7] data root selection (run authtest --root-only for the"
                         " fresh-process check)";

    QDir(dir).removeRecursively();
    qInfo().noquote() << (g_failures == 0 ? "\nALL CHECKS PASSED"
                                          : QString("\n%1 CHECK(S) FAILED").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}
