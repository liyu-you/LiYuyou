#ifndef SPACEMANAGER_H
#define SPACEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

// A Space is a top-level theme container (e.g. "考研", "小说写作").
// Content items (books, notes, ...) carry a spaceId; empty / "all" means
// "belongs to every space". The manager persists to <root>/spaces.json.
struct Space {
    QString id;
    QString name;
    QString color;       // hex, e.g. "#1a73e8"
    QDateTime createdAt;
};

class SpaceManager : public QObject
{
    Q_OBJECT
public:
    // The built-in "全部" space is immutable and always present.
    static constexpr const char *kAllSpaceId = "all";

    explicit SpaceManager(const QString &rootPath, QObject *parent = nullptr);

    bool load();
    bool save();

    QList<Space> spaces() const { return m_spaces; }
    Space spaceById(const QString &id) const;
    Space currentSpace() const;
    QString currentSpaceId() const { return m_currentId; }
    // True when the active space is the built-in "全部" view (no filtering).
    bool isAllSpace() const { return m_currentId == kAllSpaceId; }

    // Palette used by the "new space" picker.
    static QStringList presetColors();

    QString addSpace(const QString &name, const QString &color);
    bool renameSpace(const QString &id, const QString &name);
    // Cannot remove the built-in "全部" space. Returns false on failure.
    bool removeSpace(const QString &id);

public slots:
    void setCurrentSpace(const QString &id);

signals:
    void currentSpaceChanged(const QString &spaceId);
    void spacesChanged();

private:
    QString m_rootPath;
    QList<Space> m_spaces;
    QString m_currentId;

    QString metadataPath() const;
    QString generateId() const;
    void ensureAllSpace();
};

#endif // SPACEMANAGER_H
