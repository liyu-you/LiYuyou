#ifndef MATERIALLIBRARY_H
#define MATERIALLIBRARY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QDateTime>
#include <QSet>

class QFileSystemWatcher;
class QTimer;
class QImage;

enum AssetType { AssetImage, AssetVideo, AssetText, AssetOther };

struct MaterialItem {
    QString id;
    QString fileName;
    QString filePath;      // relative path, e.g. "folder/sub/name.png"
    AssetType type = AssetImage;
    QStringList tags;
    QString note;
    int rating = 0;            // 0..5 stars, 0 = unrated
    QString sourceUrl;         // origin URL when collected from web / browser extension
    QString sourceReferrer;    // page URL where the image was found
    int width = 0;
    int height = 0;
    qint64 fileSize = 0;
    QDateTime importedAt;

    QString fullPath(const QString &libraryRoot) const;
    QString folderPath() const; // parent folder path
    QString baseName() const;   // file name without extension
};

class MaterialLibrary : public QObject
{
    Q_OBJECT
public:
    explicit MaterialLibrary(const QString &rootPath, QObject *parent = nullptr);

    bool load();
    bool save();

    QString rootPath() const { return m_rootPath; }
    QString assetsPath() const;

    QList<MaterialItem> allItems() const { return m_items; }
    QList<MaterialItem> itemsInFolder(const QString &folderPath) const;
    QList<MaterialItem> searchItems(const QString &text) const;
    QStringList subFolders(const QString &folderPath) const;
    QStringList allFolderPaths() const;

    bool importFiles(const QStringList &filePaths, const QString &targetFolder = QString(),
                     const QString &sourceUrl = QString(), const QString &referrer = QString());
    // Save an in-memory QImage (clipboard paste / screenshot) into the library as PNG.
    // Returns the new item id, or empty on failure.
    QString importFromImage(const QImage &image, const QString &targetFolder = QString(),
                            const QString &sourceUrl = QString(), const QString &referrer = QString());
    bool deleteItems(const QStringList &ids);
    bool renameItem(const QString &id, const QString &newName);
    bool renameFolder(const QString &oldPath, const QString &newName);
    bool moveItems(const QStringList &ids, const QString &targetFolder);

    bool createFolder(const QString &path); // e.g. "NewFolder" or "Parent/Child"
    bool deleteFolder(const QString &path);

    MaterialItem itemById(const QString &id) const;
    static AssetType detectType(const QString &fileName);

    // --- Metadata mutators (Eagle-style: tags / rating / note / provenance) ---
    bool setRating(const QString &id, int rating);           // clamped to [0,5]
    bool setTags(const QString &id, const QStringList &tags); // replaces whole list
    bool addTag(const QString &id, const QString &tag);       // dedup, no-op if present
    bool removeTag(const QString &id, const QString &tag);
    bool setNote(const QString &id, const QString &note);
    bool setSource(const QString &id, const QString &sourceUrl, const QString &referrer);
    // Aggregate: every distinct tag used across the whole library, sorted.
    QStringList allTags() const;

signals:
    void libraryChanged();

private slots:
    void onDirChanged(const QString &path);

private:
    void setupWatcher();
    void scanAndImportNewFiles();

    QString m_rootPath;
    QList<MaterialItem> m_items;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_scanTimer = nullptr;

    QString metadataPath() const;
    QString generateId() const;
    QString uniqueFilePath(const QString &folder, const QString &fileName) const;
    void collectSubFolders(const QString &path, QSet<QString> &folders) const;
};

#endif
