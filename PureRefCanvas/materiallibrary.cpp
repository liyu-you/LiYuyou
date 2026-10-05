#include "materiallibrary.h"
#include "imageformats.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImage>
#include <QImageReader>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <algorithm>

QString MaterialItem::fullPath(const QString &libraryRoot) const
{
    return libraryRoot + "/assets/" + filePath;
}

QString MaterialItem::folderPath() const
{
    int idx = filePath.lastIndexOf('/');
    return idx >= 0 ? filePath.left(idx) : QString();
}

QString MaterialItem::baseName() const
{
    QFileInfo fi(fileName);
    return fi.completeBaseName();
}

MaterialLibrary::MaterialLibrary(const QString &rootPath, QObject *parent)
    : QObject(parent), m_rootPath(rootPath)
{
    QDir().mkpath(m_rootPath + "/assets");
    openDatabase();   // opens (or creates) library.db, applies schema, migrates legacy json
    load();           // SELECT * into m_items
    setupWatcher();
}

MaterialLibrary::~MaterialLibrary()
{
    closeDatabase();
}

QString MaterialLibrary::assetsPath() const
{
    return m_rootPath + "/assets";
}

void MaterialLibrary::setupWatcher()
{
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        m_scanTimer = new QTimer(this);
        m_scanTimer->setSingleShot(true);
        m_scanTimer->setInterval(400); // debounce bursts of file events
        connect(m_scanTimer, &QTimer::timeout, this, &MaterialLibrary::scanAndImportNewFiles);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &MaterialLibrary::onDirChanged);
    } else {
        m_watcher->removePaths(m_watcher->directories());
    }

    // Watch the assets tree (existing dirs; new subdirs are added on change)
    m_watcher->addPath(assetsPath());
    QSet<QString> dirs;
    collectSubFolders(QString(), dirs);
    for (const QString &d : dirs) {
        if (d.isEmpty()) continue;
        m_watcher->addPath(assetsPath() + "/" + d);
    }
}

void MaterialLibrary::onDirChanged(const QString &path)
{
    Q_UNUSED(path);
    // Watch any newly created subdirectories too
    QSet<QString> dirs;
    collectSubFolders(QString(), dirs);
    for (const QString &d : dirs) {
        if (d.isEmpty()) continue;
        m_watcher->addPath(assetsPath() + "/" + d);
    }
    // Debounce then rescan
    m_scanTimer->start();
}

// Register any files dropped directly into the assets folder (non-destructive:
// only adds new items, never removes existing metadata).
void MaterialLibrary::scanAndImportNewFiles()
{
    QSet<QString> known;
    for (const MaterialItem &item : m_items)
        known.insert(item.filePath);

    bool any = false;
    QDirIterator it(assetsPath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString absPath = it.filePath();
        QFileInfo fi(absPath);
        if (!fi.exists() || !fi.isFile()) continue;
        // Relative path from assets root, forward slashes
        QString rel = QDir(assetsPath()).relativeFilePath(absPath).replace('\\', '/');
        if (known.contains(rel)) continue;

        MaterialItem item;
        item.id = generateId();
        item.fileName = fi.fileName();
        item.filePath = rel;
        item.type = detectType(fi.fileName());
        item.fileSize = fi.size();
        item.importedAt = QDateTime::currentDateTime();
        if (item.type == AssetImage) {
            QImageReader reader(absPath);
            const QSize sz = reader.size(); // header-only read, cheap
            if (sz.isValid()) { item.width = sz.width(); item.height = sz.height(); }
        }
        m_items.append(item);
        upsertItem(item);
        known.insert(rel);
        any = true;
    }
    if (any) {
        emit libraryChanged();
        setupWatcher();
    }
}

QString MaterialLibrary::metadataPath() const
{
    return m_rootPath + "/library.json";
}

QString MaterialLibrary::dbPath() const
{
    return m_rootPath + "/library.db";
}

// ------------------------------------------------------------------
// Storage layer: SQLite (WAL).
//
// Why WAL:
//   - Concurrent readers don't block the writer, and vice versa.
//   - Crash consistency is automatic; we don't have to journal the JSON
//     ourselves.
//   - save() no longer rewrites the whole file on every tag change.
// ------------------------------------------------------------------
bool MaterialLibrary::openDatabase()
{
    m_dbConnName = "liyoud_lib_" + QString::number(reinterpret_cast<quintptr>(this));
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnName);
    db.setDatabaseName(dbPath());
    if (!db.open()) {
        qWarning("MaterialLibrary: cannot open %s: %s",
                 qPrintable(dbPath()), qPrintable(db.lastError().text()));
        return false;
    }
    // WAL: better concurrent read/write, crash-safe.
    QSqlQuery q(db);
    q.exec("PRAGMA journal_mode = WAL");
    q.exec("PRAGMA synchronous = NORMAL");   // safe under WAL, faster than FULL
    q.exec("PRAGMA foreign_keys = ON");

    applySchema();

    // One-shot migration from the legacy JSON file. Trigger condition:
    // the items table is empty AND a library.json is sitting next to the db.
    // (Earlier versions keyed off the presence of a -wal file, but PRAGMA
    // journal_mode=WAL creates that file immediately, so migration was
    // silently skipped and the old library never showed up.)
    QSqlQuery countQ(db);
    countQ.exec("SELECT COUNT(*) FROM items");
    const bool hasItems = countQ.next() && countQ.value(0).toInt() > 0;
    if (!hasItems && QFileInfo::exists(metadataPath())) {
        migrateLegacyJson();
    }
    return true;
}

void MaterialLibrary::closeDatabase()
{
    if (m_dbConnName.isEmpty()) return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (db.isOpen()) db.close();
    }
    QSqlDatabase::removeDatabase(m_dbConnName);
    m_dbConnName.clear();
}

void MaterialLibrary::applySchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    QSqlQuery q(db);
    q.exec(R"(CREATE TABLE IF NOT EXISTS items (
        id            TEXT PRIMARY KEY,
        fileName      TEXT NOT NULL,
        filePath      TEXT NOT NULL UNIQUE,
        type          INTEGER NOT NULL DEFAULT 0,
        tags_json     TEXT NOT NULL DEFAULT '[]',
        note          TEXT NOT NULL DEFAULT '',
        rating        INTEGER NOT NULL DEFAULT 0,
        source_url    TEXT NOT NULL DEFAULT '',
        referrer      TEXT NOT NULL DEFAULT '',
        width         INTEGER NOT NULL DEFAULT 0,
        height        INTEGER NOT NULL DEFAULT 0,
        file_size     INTEGER NOT NULL DEFAULT 0,
        imported_at   TEXT NOT NULL
    ))");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_folder ON items(filePath)");
    q.exec("CREATE INDEX IF NOT EXISTS idx_items_rating ON items(rating)");

    q.exec(R"(CREATE TABLE IF NOT EXISTS history (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        item_id    TEXT NOT NULL,
        field      TEXT NOT NULL,
        old_value  TEXT,
        new_value  TEXT,
        ts         TEXT NOT NULL
    ))");
    q.exec("CREATE INDEX IF NOT EXISTS idx_history_item ON history(item_id, ts)");
}

void MaterialLibrary::migrateLegacyJson()
{
    QFile file(metadataPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) return;

    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    db.transaction();
    for (const QJsonValue &val : doc.array()) {
        const QJsonObject obj = val.toObject();
        MaterialItem item;
        item.id = obj["id"].toString();
        item.fileName = obj["fileName"].toString();
        item.filePath = obj["filePath"].toString();
        item.type = (AssetType)obj["type"].toInt(0);
        item.tags = obj["tags"].toVariant().toStringList();
        item.note = obj["note"].toString();
        item.rating = obj["rating"].toInt(0);
        item.sourceUrl = obj["sourceUrl"].toString();
        item.sourceReferrer = obj["sourceReferrer"].toString();
        item.width = obj["width"].toInt();
        item.height = obj["height"].toInt();
        item.fileSize = obj["fileSize"].toVariant().toLongLong();
        item.importedAt = QDateTime::fromString(obj["importedAt"].toString(), Qt::ISODate);
        upsertItem(item);
    }
    db.commit();

    // Rename the legacy file so we don't migrate twice.
    QFile::rename(metadataPath(), metadataPath() + ".bak");
}

void MaterialLibrary::upsertItem(const MaterialItem &item)
{
    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    QSqlQuery q(db);
    q.prepare(R"(INSERT OR REPLACE INTO items
        (id, fileName, filePath, type, tags_json, note, rating,
         source_url, referrer, width, height, file_size, imported_at)
        VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?))");
    q.addBindValue(item.id);
    q.addBindValue(item.fileName);
    q.addBindValue(item.filePath);
    q.addBindValue((int)item.type);
    q.addBindValue(QJsonDocument(QJsonArray::fromStringList(item.tags)).toJson(QJsonDocument::Compact));
    q.addBindValue(item.note);
    q.addBindValue(item.rating);
    q.addBindValue(item.sourceUrl);
    q.addBindValue(item.sourceReferrer);
    q.addBindValue(item.width);
    q.addBindValue(item.height);
    q.addBindValue((qlonglong)item.fileSize);
    q.addBindValue(item.importedAt.toString(Qt::ISODate));
    if (!q.exec()) {
        qWarning("upsertItem: %s", qPrintable(q.lastError().text()));
    }
}

void MaterialLibrary::deleteItemRow(const QString &id)
{
    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    QSqlQuery q(db);
    q.prepare("DELETE FROM items WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

void MaterialLibrary::recordHistory(const QString &itemId, const QString &field,
                                    const QString &oldValue, const QString &newValue)
{
    if (oldValue == newValue) return; // no-op, don't pollute the log
    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    QSqlQuery q(db);
    q.prepare(R"(INSERT INTO history (item_id, field, old_value, new_value, ts)
                 VALUES (?,?,?,?,?))");
    q.addBindValue(itemId);
    q.addBindValue(field);
    q.addBindValue(oldValue);
    q.addBindValue(newValue);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.exec();
}

QString MaterialLibrary::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

AssetType MaterialLibrary::detectType(const QString &fileName)
{
    QString ext = QFileInfo(fileName).suffix().toLower();
    // Image formats are probed at runtime instead of hard-coded: a fixed list
    // would classify files as images that this Qt build cannot decode (WebP or
    // TIFF without the matching image-format plugin), which leaves a permanent
    // placeholder thumbnail instead of a correctly typed "other" entry.
    static const QStringList videoExts = {"mp4","avi","mkv","mov","wmv","flv","webm","m4v","mpg","mpeg"};
    static const QStringList textExts = {"txt","md","markdown","rtf","json","xml","html","htm","csv","log","ini","cfg","cpp","h","py","js","css","java","c","cs","go","rs","sh","bat","ps1"};
    if (ImageFormats::isReadableExtension(ext)) return AssetImage;
    if (videoExts.contains(ext)) return AssetVideo;
    if (textExts.contains(ext)) return AssetText;
    return AssetOther;
}

bool MaterialLibrary::load()
{
    m_items.clear();
    QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
    if (!db.isOpen()) return false;
    QSqlQuery q("SELECT id, fileName, filePath, type, tags_json, note, rating, "
                "source_url, referrer, width, height, file_size, imported_at FROM items", db);
    while (q.next()) {
        MaterialItem item;
        item.id        = q.value(0).toString();
        item.fileName  = q.value(1).toString();
        item.filePath  = q.value(2).toString();
        item.type      = (AssetType)q.value(3).toInt();
        {
            const QJsonArray tagArr = QJsonDocument::fromJson(q.value(4).toByteArray()).array();
            for (const QJsonValue &v : tagArr) item.tags << v.toString();
        }
        item.note      = q.value(5).toString();
        item.rating    = q.value(6).toInt();
        item.sourceUrl = q.value(7).toString();
        item.sourceReferrer = q.value(8).toString();
        item.width     = q.value(9).toInt();
        item.height    = q.value(10).toInt();
        item.fileSize  = q.value(11).toLongLong();
        item.importedAt = QDateTime::fromString(q.value(12).toString(), Qt::ISODate);
        m_items.append(item);
    }
    return true;
}

// save() is now a no-op on the storage side: every mutator writes straight
// through to SQLite. We keep the method (and the libraryChanged emission) so
// call sites that still call save() after batch edits don't silently break.
bool MaterialLibrary::save()
{
    emit libraryChanged();
    return true;
}

QList<MaterialItem> MaterialLibrary::itemsInFolder(const QString &folderPath) const
{
    QList<MaterialItem> result;
    for (const MaterialItem &item : m_items) {
        if (item.folderPath() == folderPath)
            result.append(item);
    }
    return result;
}

// Eagle-style structured search. Tokens are AND-ed. Recognised prefixes:
//   tag:<word>        must carry tag <word> (case-insensitive)
//   rating:N          rating == N
//   rating:>=N / >N / <=N / <N   rating comparison
//   ext:<ext>         file extension (without dot, case-insensitive)
//   name:<word>       base name contains <word>
//   note:<word>       note contains <word>
// A bare token matches against fileName, filePath, tags, note and sourceUrl.
QList<MaterialItem> MaterialLibrary::searchItems(const QString &rawText) const
{
    QString text = rawText.trimmed();
    if (text.isEmpty()) return m_items;

    // Tokenise on whitespace but preserve quoted phrases ("two words").
    QStringList tokens;
    {
        QString cur;
        bool inQuote = false;
        for (const QChar ch : text) {
            if (ch == '"') { inQuote = !inQuote; continue; }
            if (ch.isSpace() && !inQuote) {
                if (!cur.isEmpty()) { tokens << cur; cur.clear(); }
            } else {
                cur += ch;
            }
        }
        if (!cur.isEmpty()) tokens << cur;
    }

    // Precompile token predicates.
    struct Pred {
        enum Kind { Tag, Rating, Ext, Name, Note, Free } kind;
        QString value;          // for Tag/Ext/Name/Note/Free
        int ratingOp = 0;       // 0=eq, 1=>=, 2=>, 3=<=, 4=<
        int ratingVal = 0;
    };
    QList<Pred> preds;
    for (const QString &tok : tokens) {
        QString lower = tok.toLower();
        Pred p;
        if (lower.startsWith("tag:")) {
            p.kind = Pred::Tag; p.value = tok.mid(4).toLower();
        } else if (lower.startsWith("rating:")) {
            p.kind = Pred::Rating;
            QString body = tok.mid(7);
            if (body.startsWith(">=")) { p.ratingOp = 1; p.ratingVal = body.mid(2).toInt(); }
            else if (body.startsWith(">"))  { p.ratingOp = 2; p.ratingVal = body.mid(1).toInt(); }
            else if (body.startsWith("<=")){ p.ratingOp = 3; p.ratingVal = body.mid(2).toInt(); }
            else if (body.startsWith("<")) { p.ratingOp = 4; p.ratingVal = body.mid(1).toInt(); }
            else                            { p.ratingOp = 0; p.ratingVal = body.toInt(); }
        } else if (lower.startsWith("ext:")) {
            p.kind = Pred::Ext; p.value = tok.mid(4).toLower();
        } else if (lower.startsWith("name:")) {
            p.kind = Pred::Name; p.value = tok.mid(5).toLower();
        } else if (lower.startsWith("note:")) {
            p.kind = Pred::Note; p.value = tok.mid(5).toLower();
        } else {
            p.kind = Pred::Free; p.value = lower;
        }
        preds << p;
    }

    QList<MaterialItem> result;
    for (const MaterialItem &item : m_items) {
        bool ok = true;
        for (const Pred &p : preds) {
            switch (p.kind) {
            case Pred::Tag: {
                bool hit = false;
                for (const QString &t : item.tags)
                    if (t.toLower().contains(p.value)) { hit = true; break; }
                if (!hit) ok = false;
                break;
            }
            case Pred::Rating: {
                const int r = item.rating;
                switch (p.ratingOp) {
                    case 0: ok = (r == p.ratingVal); break;
                    case 1: ok = (r >= p.ratingVal); break;
                    case 2: ok = (r >  p.ratingVal); break;
                    case 3: ok = (r <= p.ratingVal); break;
                    case 4: ok = (r <  p.ratingVal); break;
                }
                break;
            }
            case Pred::Ext:
                ok = QFileInfo(item.fileName).suffix().toLower() == p.value;
                break;
            case Pred::Name:
                ok = item.baseName().toLower().contains(p.value);
                break;
            case Pred::Note:
                ok = item.note.toLower().contains(p.value);
                break;
            case Pred::Free: {
                bool hit = item.fileName.toLower().contains(p.value)
                        || item.filePath.toLower().contains(p.value)
                        || item.note.toLower().contains(p.value)
                        || item.sourceUrl.toLower().contains(p.value);
                if (!hit) {
                    for (const QString &t : item.tags)
                        if (t.toLower().contains(p.value)) { hit = true; break; }
                }
                if (!hit) ok = false;
                break;
            }
            }
            if (!ok) break;
        }
        if (ok) result.append(item);
    }
    return result;
}

QStringList MaterialLibrary::subFolders(const QString &folderPath) const
{
    QSet<QString> folders;
    for (const MaterialItem &item : m_items) {
        QString fp = item.folderPath();
        if (folderPath.isEmpty()) {
            // Top-level folders
            if (!fp.isEmpty()) {
                int idx = fp.indexOf('/');
                folders.insert(idx >= 0 ? fp.left(idx) : fp);
            }
        } else {
            if (fp.startsWith(folderPath + "/")) {
                QString rest = fp.mid(folderPath.length() + 1);
                int idx = rest.indexOf('/');
                folders.insert(folderPath + "/" + (idx >= 0 ? rest.left(idx) : rest));
            }
        }
    }
    // Also check physical directories
    QDir dir(assetsPath() + "/" + folderPath);
    if (dir.exists()) {
        for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString full = folderPath.isEmpty() ? name : folderPath + "/" + name;
            folders.insert(full);
        }
    }
    QStringList list = folders.values();
    list.sort();
    return list;
}

QStringList MaterialLibrary::allFolderPaths() const
{
    QSet<QString> folders;
    for (const MaterialItem &item : m_items) {
        QString fp = item.folderPath();
        while (!fp.isEmpty()) {
            folders.insert(fp);
            int idx = fp.lastIndexOf('/');
            fp = idx >= 0 ? fp.left(idx) : QString();
        }
    }
    // Physical directories
    QDir root(assetsPath());
    if (root.exists()) {
        for (const QString &d : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            collectSubFolders(d, folders);
        }
    }
    QStringList list = folders.values();
    list.sort();
    return list;
}

void MaterialLibrary::collectSubFolders(const QString &path, QSet<QString> &folders) const
{
    folders.insert(path);
    QDir dir(assetsPath() + "/" + path);
    if (dir.exists()) {
        for (const QString &d : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            collectSubFolders(path + "/" + d, folders);
        }
    }
}

QString MaterialLibrary::uniqueFilePath(const QString &folder, const QString &fileName) const
{
    QString base = folder.isEmpty() ? fileName : folder + "/" + fileName;
    if (!QFile::exists(assetsPath() + "/" + base)) return base;
    QFileInfo fi(fileName);
    int i = 1;
    while (true) {
        QString candidate = folder.isEmpty()
            ? QString("%1_%2.%3").arg(fi.completeBaseName()).arg(i).arg(fi.suffix())
            : QString("%1/%2_%3.%4").arg(folder).arg(fi.completeBaseName()).arg(i).arg(fi.suffix());
        if (!QFile::exists(assetsPath() + "/" + candidate)) return candidate;
        i++;
    }
}

bool MaterialLibrary::importFiles(const QStringList &filePaths, const QString &targetFolder,
                                  const QString &sourceUrl, const QString &referrer)
{
    bool any = false;
    for (const QString &srcPath : filePaths) {
        QFileInfo fi(srcPath);
        if (!fi.exists()) continue;
        QString relPath = uniqueFilePath(targetFolder, fi.fileName());
        QString destPath = assetsPath() + "/" + relPath;
        QDir().mkpath(QFileInfo(destPath).absolutePath());
        if (!QFile::copy(srcPath, destPath)) continue;

        MaterialItem item;
        item.id = generateId();
        item.fileName = fi.fileName();
        item.filePath = relPath;
        item.type = detectType(fi.fileName());
        item.fileSize = fi.size();
        item.importedAt = QDateTime::currentDateTime();
        item.sourceUrl = sourceUrl;
        item.sourceReferrer = referrer;
        if (item.type == AssetImage) {
            QImage img(srcPath);
            if (!img.isNull()) {
                item.width = img.width();
                item.height = img.height();
            }
        }
        m_items.append(item);
        upsertItem(item);
        any = true;
    }
    if (any) emit libraryChanged();
    return any;
}

QString MaterialLibrary::importFromImage(const QImage &image, const QString &targetFolder,
                                         const QString &sourceUrl, const QString &referrer)
{
    if (image.isNull()) return QString();
    // Build a unique filename: "pasted_yyyyMMdd_hhmmss.png"
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    const QString baseName = QString("pasted_%1").arg(stamp);
    QString relPath = uniqueFilePath(targetFolder, baseName + ".png");
    QString destPath = assetsPath() + "/" + relPath;
    QDir().mkpath(QFileInfo(destPath).absolutePath());
    if (!image.save(destPath, "PNG")) return QString();

    MaterialItem item;
    item.id = generateId();
    item.fileName = QFileInfo(destPath).fileName();
    item.filePath = relPath;
    item.type = AssetImage;
    item.fileSize = QFileInfo(destPath).size();
    item.importedAt = QDateTime::currentDateTime();
    item.sourceUrl = sourceUrl;
    item.sourceReferrer = referrer;
    item.width = image.width();
    item.height = image.height();
    m_items.append(item);
    upsertItem(item);
    emit libraryChanged();
    return item.id;
}

bool MaterialLibrary::deleteItems(const QStringList &ids)
{
    bool any = false;
    for (const QString &id : ids) {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](const MaterialItem &i) { return i.id == id; });
        if (it != m_items.end()) {
            QFile::remove(assetsPath() + "/" + it->filePath);
            deleteItemRow(id);
            m_items.erase(it);
            any = true;
        }
    }
    if (any) emit libraryChanged();
    return any;
}

bool MaterialLibrary::renameItem(const QString &id, const QString &newName)
{
    auto it = std::find_if(m_items.begin(), m_items.end(), [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    QFileInfo fi(it->fileName);
    QString newFileName = newName.contains('.') ? newName : newName + "." + fi.suffix();
    QString folder = it->folderPath();
    QString newRelPath = folder.isEmpty() ? newFileName : folder + "/" + newFileName;
    QString oldFull = assetsPath() + "/" + it->filePath;
    QString newFull = assetsPath() + "/" + newRelPath;
    if (oldFull != newFull) {
        if (QFile::exists(newFull)) return false;
        QFile::rename(oldFull, newFull);
    }
    recordHistory(id, "file_name", it->fileName, newFileName);
    it->fileName = newFileName;
    it->filePath = newRelPath;
    upsertItem(*it);
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::renameFolder(const QString &oldPath, const QString &newName)
{
    if (oldPath.isEmpty()) return false;
    int idx = oldPath.lastIndexOf('/');
    QString parent = idx >= 0 ? oldPath.left(idx) : QString();
    QString newPath = parent.isEmpty() ? newName : parent + "/" + newName;

    // Rename physical directory
    QString oldDir = assetsPath() + "/" + oldPath;
    QString newDir = assetsPath() + "/" + newPath;
    if (QDir(oldDir).exists()) {
        QDir().rename(oldDir, newDir);
    }

    // Update items in this folder and subfolders
    for (MaterialItem &item : m_items) {
        if (item.filePath == oldPath || item.filePath.startsWith(oldPath + "/")) {
            item.filePath = newPath + item.filePath.mid(oldPath.length());
            upsertItem(item);
        }
    }

    emit libraryChanged();
    return true;
}

bool MaterialLibrary::moveItems(const QStringList &ids, const QString &targetFolder)
{
    bool any = false;
    for (const QString &id : ids) {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](const MaterialItem &i) { return i.id == id; });
        if (it == m_items.end()) continue;
        QString newRelPath = uniqueFilePath(targetFolder, it->fileName);
        QString oldFull = assetsPath() + "/" + it->filePath;
        QString newFull = assetsPath() + "/" + newRelPath;
        QDir().mkpath(QFileInfo(newFull).absolutePath());
        if (QFile::rename(oldFull, newFull)) {
            it->filePath = newRelPath;
            upsertItem(*it);
            any = true;
        }
    }
    if (any) emit libraryChanged();
    return any;
}

bool MaterialLibrary::createFolder(const QString &path)
{
    QString full = assetsPath() + "/" + path;
    if (QDir(full).exists()) return false;
    bool ok = QDir().mkpath(full);
    if (ok) emit libraryChanged();
    return ok;
}

bool MaterialLibrary::deleteFolder(const QString &path)
{
    QString full = assetsPath() + "/" + path;
    QDir dir(full);
    if (!dir.exists()) return false;
    // Delete items in this folder and subfolders
    QStringList toDelete;
    for (const MaterialItem &item : m_items) {
        if (item.filePath == path || item.filePath.startsWith(path + "/"))
            toDelete.append(item.id);
    }
    deleteItems(toDelete);
    dir.removeRecursively();
    emit libraryChanged();
    return true;
}

MaterialItem MaterialLibrary::itemById(const QString &id) const
{
    for (const MaterialItem &item : m_items)
        if (item.id == id) return item;
    return MaterialItem();
}

// ---- Metadata mutators ----
// All of these write-through to library.json and emit libraryChanged.
bool MaterialLibrary::setRating(const QString &id, int rating)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    const int old = it->rating;
    it->rating = qBound(0, rating, 5);
    upsertItem(*it);
    recordHistory(id, "rating", QString::number(old), QString::number(it->rating));
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::setTags(const QString &id, const QStringList &tags)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    // Keep tags tidy: trim, drop empties, dedupe, case-preserved order.
    QStringList cleaned;
    for (const QString &t : tags) {
        QString s = t.trimmed();
        if (s.isEmpty() || cleaned.contains(s, Qt::CaseInsensitive)) continue;
        cleaned << s;
    }
    const QString oldJoined = it->tags.join(",");
    it->tags = cleaned;
    upsertItem(*it);
    recordHistory(id, "tags", oldJoined, cleaned.join(","));
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::addTag(const QString &id, const QString &tag)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    QString s = tag.trimmed();
    if (s.isEmpty()) return false;
    for (const QString &t : it->tags)
        if (t.compare(s, Qt::CaseInsensitive) == 0) return true; // already present
    const QString oldJoined = it->tags.join(",");
    it->tags << s;
    upsertItem(*it);
    recordHistory(id, "tags", oldJoined, it->tags.join(","));
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::removeTag(const QString &id, const QString &tag)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    QString s = tag.trimmed();
    const QString oldJoined = it->tags.join(",");
    it->tags.erase(std::remove_if(it->tags.begin(), it->tags.end(),
        [&](const QString &t) { return t.compare(s, Qt::CaseInsensitive) == 0; }),
        it->tags.end());
    upsertItem(*it);
    recordHistory(id, "tags", oldJoined, it->tags.join(","));
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::setNote(const QString &id, const QString &note)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    const QString old = it->note;
    it->note = note;
    upsertItem(*it);
    recordHistory(id, "note", old, note);
    emit libraryChanged();
    return true;
}

bool MaterialLibrary::setSource(const QString &id, const QString &sourceUrl, const QString &referrer)
{
    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](const MaterialItem &i) { return i.id == id; });
    if (it == m_items.end()) return false;
    const QString oldUrl = it->sourceUrl;
    it->sourceUrl = sourceUrl;
    it->sourceReferrer = referrer;
    upsertItem(*it);
    recordHistory(id, "source_url", oldUrl, sourceUrl);
    emit libraryChanged();
    return true;
}

QStringList MaterialLibrary::allTags() const
{
    QSet<QString> seen;
    for (const MaterialItem &item : m_items)
        for (const QString &t : item.tags) {
            QString s = t.trimmed();
            if (!s.isEmpty()) seen.insert(s);
        }
    QStringList list = seen.values();
    list.sort(Qt::CaseInsensitive);
    return list;
}
