#ifndef MUSICLIBRARYDB_H
#define MUSICLIBRARYDB_H

// SQLite-backed local music library index, ported from Mineradio's
// local-library-store.js. Design goals:
//   * Fingerprint = "path|size|mtime". When unchanged, skip re-parsing tags.
//   * Cover art stored as BLOB (no base64 text, ~33% smaller).
//   * Play stats (count / listened ms / favorite / last-played) live in a
//     separate table so they survive rescan and tag re-reads.
//
// All methods are synchronous; the DB is small and we call this on import/scan,
// not on the audio thread.

#include "songinfo.h"

#include <QString>
#include <QHash>
#include <QSet>
#include <expected>

class QSqlDatabase;

struct SongStats {
    qint64 playCount = 0;
    qint64 listenMs = 0;
    bool favorite = false;
    qint64 favoriteAt = 0;
    qint64 lastPlayedAt = 0;
};

// C++23: fallible operations return std::expected instead of bool + out-param.
using DbOk   = std::expected<void, QString>;
using DbSong = std::expected<SongInfo, QString>;

class MusicLibraryDb {
public:
    explicit MusicLibraryDb(const QString &dbPath);
    ~MusicLibraryDb();

    DbOk open();
    void close();
    bool isOpen() const { return m_open; }

    // Fingerprint: size|mtime (path is the PK).
    static QString makeFingerprint(qint64 size, qint64 mtime);

    // Returns true when the cached row for `path` matches the given fingerprint
    // (i.e. the file has not changed since last scan).
    bool isFresh(const QString &path, const QString &fingerprint) const;

    // Load cached song metadata by path. On success returns the SongInfo; on
    // failure (no fresh row) returns an error string.
    DbSong loadCached(const QString &path, const QString &fingerprint) const;

    // Upsert parsed metadata + cover blob.
    void upsertSong(const SongInfo &song, qint64 size, qint64 mtime);

    // Remove rows whose path is not in `validPaths` (called after a full rescan).
    int pruneMissing(const QSet<QString> &validPaths);

    // ---- Play stats ----
    void bumpPlay(const QString &path, qint64 listenMs);
    void setFavorite(const QString &path, bool favorite);
    SongStats statsFor(const QString &path) const;
    QHash<QString, SongStats> allStats() const;

    // Ordered path lists for sidebar views.
    QStringList recentPaths(int limit = 50) const;
    QStringList topPlayedPaths(int limit = 50) const;

private:
    QString m_dbPath;
    bool m_open = false;
    int m_connectionSeq = 0;

    void createSchema();
};

#endif // MUSICLIBRARYDB_H
