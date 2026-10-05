#include "musiclibrarydb.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>

MusicLibraryDb::MusicLibraryDb(const QString &dbPath)
    : m_dbPath(dbPath)
{
}

MusicLibraryDb::~MusicLibraryDb()
{
    close();
}

DbOk MusicLibraryDb::open()
{
    if (m_open) return {};
    QFileInfo fi(m_dbPath);
    QDir().mkpath(fi.absolutePath());

    // Unique connection name so multiple instances don't collide.
    m_connectionSeq++;
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        const QString err = db.lastError().text();
        qWarning() << "MusicLibraryDb: cannot open" << m_dbPath << err;
        return std::unexpected(QStringLiteral("无法打开音乐库数据库：%1").arg(err));
    }
    {
        // Qt6: QSqlDatabase::exec(const QString&) is deprecated; use QSqlQuery(db).exec().
        QSqlQuery q(db);
        q.exec("PRAGMA journal_mode = WAL");
        q.exec("PRAGMA synchronous = NORMAL");
        q.exec("PRAGMA temp_store = MEMORY");
    }
    m_open = true;
    createSchema();
    return {};
}

void MusicLibraryDb::close()
{
    if (!m_open) return;
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(connName);
    m_open = false;
}

void MusicLibraryDb::createSchema()
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    q.exec(R"(CREATE TABLE IF NOT EXISTS songs (
        path TEXT PRIMARY KEY,
        fingerprint TEXT NOT NULL DEFAULT '',
        title TEXT NOT NULL DEFAULT '',
        artist TEXT NOT NULL DEFAULT '',
        album TEXT NOT NULL DEFAULT '',
        genre TEXT NOT NULL DEFAULT '',
        year TEXT NOT NULL DEFAULT '',
        track TEXT NOT NULL DEFAULT '',
        duration_ms INTEGER NOT NULL DEFAULT 0,
        sample_rate INTEGER NOT NULL DEFAULT 0,
        channels INTEGER NOT NULL DEFAULT 0,
        bitrate INTEGER NOT NULL DEFAULT 0,
        cover_mime TEXT NOT NULL DEFAULT '',
        cover BLOB,
        updated_at INTEGER NOT NULL DEFAULT 0
    ))");
    q.exec(R"(CREATE TABLE IF NOT EXISTS stats (
        path TEXT PRIMARY KEY,
        play_count INTEGER NOT NULL DEFAULT 0,
        listen_ms INTEGER NOT NULL DEFAULT 0,
        favorite INTEGER NOT NULL DEFAULT 0,
        favorite_at INTEGER NOT NULL DEFAULT 0,
        last_played_at INTEGER NOT NULL DEFAULT 0
    ))");
    q.exec("CREATE INDEX IF NOT EXISTS idx_songs_fingerprint ON songs(fingerprint)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_stats_last_played ON stats(last_played_at DESC)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_stats_favorite ON stats(favorite, last_played_at DESC)");
}

QString MusicLibraryDb::makeFingerprint(qint64 size, qint64 mtime)
{
    return QString::number(size) + "|" + QString::number(mtime);
}

bool MusicLibraryDb::isFresh(const QString &path, const QString &fingerprint) const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM songs WHERE path=? AND fingerprint=?");
    q.addBindValue(path);
    q.addBindValue(fingerprint);
    if (!q.exec() || !q.next()) return false;
    return true;
}

DbSong MusicLibraryDb::loadCached(const QString &path, const QString &fingerprint) const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    q.prepare("SELECT title, artist, album, genre, year, track, duration_ms, sample_rate, channels, bitrate, cover_mime, cover "
              "FROM songs WHERE path=? AND fingerprint=?");
    q.addBindValue(path);
    q.addBindValue(fingerprint);
    if (!q.exec() || !q.next())
        return std::unexpected(QStringLiteral("无缓存元数据"));
    SongInfo out;
    out.path = path;
    out.title = q.value(0).toString();
    out.artist = q.value(1).toString();
    out.album = q.value(2).toString();
    out.genre = q.value(3).toString();
    out.year = q.value(4).toString();
    out.trackNumber = q.value(5).toString();
    out.duration = q.value(6).toLongLong();
    out.sampleRate = q.value(7).toInt();
    out.channels = q.value(8).toInt();
    out.bitrate = q.value(9).toInt();
    out.embeddedCoverMime = q.value(10).toString();
    out.embeddedCover = q.value(11).toByteArray();
    return out;
}

void MusicLibraryDb::upsertSong(const SongInfo &song, qint64 size, qint64 mtime)
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    const QString fp = makeFingerprint(size, mtime);
    q.prepare(R"(INSERT INTO songs (path, fingerprint, title, artist, album, genre, year, track,
                duration_ms, sample_rate, channels, bitrate, cover_mime, cover, updated_at)
                VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
                ON CONFLICT(path) DO UPDATE SET
                  fingerprint=excluded.fingerprint, title=excluded.title, artist=excluded.artist,
                  album=excluded.album, genre=excluded.genre, year=excluded.year, track=excluded.track,
                  duration_ms=excluded.duration_ms, sample_rate=excluded.sample_rate,
                  channels=excluded.channels, bitrate=excluded.bitrate,
                  cover_mime=excluded.cover_mime, cover=excluded.cover, updated_at=excluded.updated_at)");
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    q.addBindValue(song.path);
    q.addBindValue(fp);
    q.addBindValue(song.title);
    q.addBindValue(song.artist);
    q.addBindValue(song.album);
    q.addBindValue(song.genre);
    q.addBindValue(song.year);
    q.addBindValue(song.trackNumber);
    q.addBindValue(song.duration);
    q.addBindValue(song.sampleRate);
    q.addBindValue(song.channels);
    q.addBindValue(song.bitrate);
    q.addBindValue(song.embeddedCoverMime);
    q.addBindValue(song.embeddedCover);
    q.addBindValue(now);
    if (!q.exec()) {
        qWarning() << "upsertSong failed:" << q.lastError().text();
    }
}

int MusicLibraryDb::pruneMissing(const QSet<QString> &validPaths)
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    db.transaction();
    QSqlQuery q(db);
    // Build a temp table of valid paths for set-based pruning.
    q.exec("CREATE TEMP TABLE _valid(path TEXT PRIMARY KEY)");
    QSqlQuery ins(db);
    ins.prepare("INSERT OR IGNORE INTO _valid(path) VALUES (?)");
    for (const QString &p : validPaths) ins.addBindValue(p);
    ins.execBatch();
    q.exec("DELETE FROM songs WHERE path NOT IN (SELECT path FROM _valid)");
    const int removed = q.numRowsAffected();
    q.exec("DELETE FROM stats WHERE path NOT IN (SELECT path FROM _valid)");
    db.commit();
    return removed;
}

void MusicLibraryDb::bumpPlay(const QString &path, qint64 listenMs)
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    q.prepare(R"(INSERT INTO stats (path, play_count, listen_ms, last_played_at)
                VALUES (?, 1, ?, ?)
                ON CONFLICT(path) DO UPDATE SET
                  play_count=play_count+1,
                  listen_ms=listen_ms+excluded.listen_ms,
                  last_played_at=excluded.last_played_at)");
    q.addBindValue(path);
    q.addBindValue(listenMs);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
}

void MusicLibraryDb::setFavorite(const QString &path, bool favorite)
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    const qint64 now = favorite ? QDateTime::currentMSecsSinceEpoch() : 0;
    q.prepare(R"(INSERT INTO stats (path, favorite, favorite_at) VALUES (?, ?, ?)
                ON CONFLICT(path) DO UPDATE SET favorite=excluded.favorite, favorite_at=excluded.favorite_at)");
    q.addBindValue(path);
    q.addBindValue(favorite ? 1 : 0);
    q.addBindValue(now);
    q.exec();
}

SongStats MusicLibraryDb::statsFor(const QString &path) const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    q.prepare("SELECT play_count, listen_ms, favorite, favorite_at, last_played_at FROM stats WHERE path=?");
    q.addBindValue(path);
    SongStats s;
    if (q.exec() && q.next()) {
        s.playCount = q.value(0).toLongLong();
        s.listenMs = q.value(1).toLongLong();
        s.favorite = q.value(2).toInt() != 0;
        s.favoriteAt = q.value(3).toLongLong();
        s.lastPlayedAt = q.value(4).toLongLong();
    }
    return s;
}

QHash<QString, SongStats> MusicLibraryDb::allStats() const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    QHash<QString, SongStats> out;
    if (!q.exec("SELECT path, play_count, listen_ms, favorite, favorite_at, last_played_at FROM stats"))
        return out;
    while (q.next()) {
        SongStats s;
        s.playCount = q.value(1).toLongLong();
        s.listenMs = q.value(2).toLongLong();
        s.favorite = q.value(3).toInt() != 0;
        s.favoriteAt = q.value(4).toLongLong();
        s.lastPlayedAt = q.value(5).toLongLong();
        out.insert(q.value(0).toString(), s);
    }
    return out;
}

QStringList MusicLibraryDb::recentPaths(int limit) const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    QStringList out;
    q.prepare("SELECT path FROM stats WHERE last_played_at > 0 ORDER BY last_played_at DESC LIMIT ?");
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) out << q.value(0).toString();
    return out;
}

QStringList MusicLibraryDb::topPlayedPaths(int limit) const
{
    const QString connName = QStringLiteral("musicdb_%1").arg(m_connectionSeq);
    QSqlDatabase db = QSqlDatabase::database(connName);
    QSqlQuery q(db);
    QStringList out;
    q.prepare("SELECT path FROM stats WHERE play_count > 0 ORDER BY play_count DESC, listen_ms DESC LIMIT ?");
    q.addBindValue(limit);
    if (!q.exec()) return out;
    while (q.next()) out << q.value(0).toString();
    return out;
}
