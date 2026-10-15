#include "spacemanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QUuid>
#include <QDate>

SpaceManager::SpaceManager(const QString &rootPath, QObject *parent)
    : QObject(parent), m_rootPath(rootPath)
{
}

QStringList SpaceManager::presetColors()
{
    // Google Material palette — one hue per space.
    return {
        "#1a73e8", // Blue
        "#34a853", // Green
        "#fbbc04", // Yellow
        "#ea4335", // Red
        "#9334e6", // Purple
        "#e91e63", // Pink
        "#00acc1", // Cyan
        "#ff6d01", // Orange
    };
}

QString SpaceManager::metadataPath() const
{
    return m_rootPath + "/spaces.json";
}

QString SpaceManager::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void SpaceManager::ensureAllSpace()
{
    for (const Space &s : m_spaces) {
        if (s.id == kAllSpaceId) return;
    }
    Space all;
    all.id = kAllSpaceId;
    all.name = QStringLiteral("全部");
    all.color = "#5f6368";
    all.createdAt = QDateTime::currentDateTime();
    m_spaces.prepend(all);
    if (m_currentId.isEmpty()) m_currentId = kAllSpaceId;
}

bool SpaceManager::load()
{
    m_spaces.clear();
    m_currentId = kAllSpaceId;

    QFile file(metadataPath());
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
            file.close();
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject root = doc.object();
                m_currentId = root.value("currentSpace").toString(kAllSpaceId);
                const QJsonArray arr = root.value("spaces").toArray();
                for (const QJsonValue &v : arr) {
                    QJsonObject o = v.toObject();
                    Space s;
                    s.id = o.value("id").toString();
                    s.name = o.value("name").toString();
                    s.color = o.value("color").toString("#1a73e8");
                    s.createdAt = QDateTime::fromString(
                        o.value("createdAt").toString(), Qt::ISODate);
                    s.lastPage = o.value("lastPage").toInt(0);
                    s.lastBookId = o.value("lastBookId").toString();
                    s.lastNoteId = o.value("lastNoteId").toString();
                    s.bgImage = o.value("bgImage").toString();
                    s.bgColor = o.value("bgColor").toString();
                    s.totalActiveSeconds = static_cast<qint64>(o.value("totalActiveSeconds").toDouble(0));
                    s.weekActiveSeconds = static_cast<qint64>(o.value("weekActiveSeconds").toDouble(0));
                    s.weekStart = o.value("weekStart").toString();
                    if (!s.id.isEmpty() && !s.name.isEmpty())
                        m_spaces.append(s);
                }
            }
        }
    }

    ensureAllSpace();

    // Validate current id; fall back to "all" if missing.
    bool found = false;
    for (const Space &s : m_spaces) {
        if (s.id == m_currentId) { found = true; break; }
    }
    if (!found) m_currentId = kAllSpaceId;

    return true;
}

bool SpaceManager::save()
{
    QDir().mkpath(m_rootPath);
    QJsonObject root;
    root["currentSpace"] = m_currentId;
    QJsonArray arr;
    for (const Space &s : m_spaces) {
        QJsonObject o;
        o["id"] = s.id;
        o["name"] = s.name;
        o["color"] = s.color;
        o["createdAt"] = s.createdAt.toString(Qt::ISODate);
        o["lastPage"] = s.lastPage;
        o["lastBookId"] = s.lastBookId;
        o["lastNoteId"] = s.lastNoteId;
        o["bgImage"] = s.bgImage;
        o["bgColor"] = s.bgColor;
        o["totalActiveSeconds"] = static_cast<double>(s.totalActiveSeconds);
        o["weekActiveSeconds"] = static_cast<double>(s.weekActiveSeconds);
        o["weekStart"] = s.weekStart;
        arr.append(o);
    }
    root["spaces"] = arr;

    QFile file(metadataPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

Space SpaceManager::spaceById(const QString &id) const
{
    for (const Space &s : m_spaces)
        if (s.id == id) return s;
    return Space();
}

Space SpaceManager::currentSpace() const
{
    return spaceById(m_currentId);
}

QString SpaceManager::addSpace(const QString &name, const QString &color)
{
    if (name.trimmed().isEmpty()) return QString();
    Space s;
    s.id = generateId();
    s.name = name.trimmed();
    s.color = color.isEmpty() ? presetColors().first() : color;
    s.createdAt = QDateTime::currentDateTime();
    m_spaces.append(s);
    save();
    emit spacesChanged();
    return s.id;
}

bool SpaceManager::renameSpace(const QString &id, const QString &name)
{
    if (id == kAllSpaceId) return false;
    if (name.trimmed().isEmpty()) return false;
    for (Space &s : m_spaces) {
        if (s.id == id) {
            s.name = name.trimmed();
            save();
            emit spacesChanged();
            return true;
        }
    }
    return false;
}

bool SpaceManager::removeSpace(const QString &id)
{
    if (id == kAllSpaceId) return false;
    for (int i = 0; i < m_spaces.size(); ++i) {
        if (m_spaces[i].id == id) {
            m_spaces.removeAt(i);
            if (m_currentId == id) {
                m_currentId = kAllSpaceId;
                emit currentSpaceChanged(m_currentId);
            }
            save();
            emit spacesChanged();
            return true;
        }
    }
    return false;
}

void SpaceManager::setCurrentSpace(const QString &id)
{
    if (m_currentId == id) return;
    bool found = false;
    for (const Space &s : m_spaces) {
        if (s.id == id) { found = true; break; }
    }
    if (!found) return;
    m_currentId = id;
    save();
    emit currentSpaceChanged(m_currentId);
}

void SpaceManager::updateSpaceState(const QString &id, int lastPage,
                                    const QString &lastBookId,
                                    const QString &lastNoteId)
{
    for (Space &s : m_spaces) {
        if (s.id == id) {
            s.lastPage = lastPage;
            s.lastBookId = lastBookId;
            s.lastNoteId = lastNoteId;
            save();
            return;
        }
    }
}

void SpaceManager::setSpaceBackground(const QString &id, const QString &bgImage,
                                      const QString &bgColor)
{
    for (Space &s : m_spaces) {
        if (s.id == id) {
            s.bgImage = bgImage;
            s.bgColor = bgColor;
            save();
            emit spacesChanged();
            return;
        }
    }
}

QString SpaceManager::currentWeekStart()
{
    QDate today = QDate::currentDate();
    int dow = today.dayOfWeek();  // 1 = Monday
    return today.addDays(-(dow - 1)).toString(Qt::ISODate);
}

void SpaceManager::recordActiveSeconds(const QString &id, int seconds)
{
    const QString thisWeek = currentWeekStart();
    for (Space &s : m_spaces) {
        if (s.id == id) {
            // Auto-reset weekly counter when a new week starts
            if (s.weekStart != thisWeek) {
                s.weekStart = thisWeek;
                s.weekActiveSeconds = 0;
            }
            s.totalActiveSeconds += seconds;
            s.weekActiveSeconds += seconds;
            // Throttle disk writes: save only every 30 accumulated seconds.
            if (s.weekActiveSeconds % 30 < seconds)
                save();
            return;
        }
    }
}
