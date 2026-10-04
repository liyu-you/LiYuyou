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
    load();
    setupWatcher();
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
        known.insert(rel);
        any = true;
    }
    if (any) {
        save(); // emits libraryChanged → grids refresh
        setupWatcher();
    }
}

QString MaterialLibrary::metadataPath() const
{
    return m_rootPath + "/library.json";
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
    QFile file(metadataPath());
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) return false;
    m_items.clear();
    for (const QJsonValue &val : doc.array()) {
        QJsonObject obj = val.toObject();
        MaterialItem item;
        item.id = obj["id"].toString();
        item.fileName = obj["fileName"].toString();
        item.filePath = obj["filePath"].toString();
        item.type = (AssetType)obj["type"].toInt(0);
        item.tags = obj["tags"].toVariant().toStringList();
        item.note = obj["note"].toString();
        item.width = obj["width"].toInt();
        item.height = obj["height"].toInt();
        item.fileSize = obj["fileSize"].toVariant().toLongLong();
        item.importedAt = QDateTime::fromString(obj["importedAt"].toString(), Qt::ISODate);
        m_items.append(item);
    }
    return true;
}

bool MaterialLibrary::save()
{
    QFile file(metadataPath());
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonArray arr;
    for (const MaterialItem &item : m_items) {
        QJsonObject obj;
        obj["id"] = item.id;
        obj["fileName"] = item.fileName;
        obj["filePath"] = item.filePath;
        obj["type"] = (int)item.type;
        obj["tags"] = QJsonArray::fromStringList(item.tags);
        obj["note"] = item.note;
        obj["width"] = item.width;
        obj["height"] = item.height;
        obj["fileSize"] = (double)item.fileSize;
        obj["importedAt"] = item.importedAt.toString(Qt::ISODate);
        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson());
    file.close();
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

QList<MaterialItem> MaterialLibrary::searchItems(const QString &text) const
{
    if (text.isEmpty()) return m_items;
    QString t = text.toLower();
    QList<MaterialItem> result;
    for (const MaterialItem &item : m_items) {
        if (item.fileName.toLower().contains(t) ||
            item.filePath.toLower().contains(t) ||
            item.note.toLower().contains(t))
            result.append(item);
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

bool MaterialLibrary::importFiles(const QStringList &filePaths, const QString &targetFolder)
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
        if (item.type == AssetImage) {
            QImage img(srcPath);
            if (!img.isNull()) {
                item.width = img.width();
                item.height = img.height();
            }
        }
        m_items.append(item);
        any = true;
    }
    if (any) save();
    return any;
}

bool MaterialLibrary::deleteItems(const QStringList &ids)
{
    bool any = false;
    for (const QString &id : ids) {
        auto it = std::find_if(m_items.begin(), m_items.end(), [&](const MaterialItem &i) { return i.id == id; });
        if (it != m_items.end()) {
            QFile::remove(assetsPath() + "/" + it->filePath);
            m_items.erase(it);
            any = true;
        }
    }
    if (any) save();
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
    it->fileName = newFileName;
    it->filePath = newRelPath;
    save();
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
        }
    }

    save();
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
            any = true;
        }
    }
    if (any) save();
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
