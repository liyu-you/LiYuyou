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
    // ---- State memory: where the user was when they left this space ----
    int     lastPage = 0;       // page index in m_stack (0=canvas,1=note,...)
    QString lastBookId;         // book being read, empty if none
    QString lastNoteId;         // note being edited, empty if none
    // ---- Environment preset ----
    QString bgImage;            // custom background image path, empty = use global
    QString bgColor;            // custom background color, empty = use global
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

    // State memory: persist where the user was in this space.
    void updateSpaceState(const QString &id, int lastPage,
                          const QString &lastBookId, const QString &lastNoteId);
    // Environment preset: custom background for this space.
    void setSpaceBackground(const QString &id, const QString &bgImage,
                            const QString &bgColor);

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
