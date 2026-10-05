#include "flashcardstore.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>

FlashcardStore &FlashcardStore::instance()
{
    static FlashcardStore s;
    return s;
}

FlashcardStore::FlashcardStore() { load(); }

QString FlashcardStore::path() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + "/PureRefFlashcards/cards.json";
}

void FlashcardStore::load()
{
    m_cards.clear();
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        Flashcard c;
        c.id = o["id"].toString();
        c.front = o["front"].toString();
        c.back = o["back"].toString();
        c.source = o["source"].toString();
        c.created = QDate::fromString(o["created"].toString(), Qt::ISODate);
        c.nextReview = QDate::fromString(o["nextReview"].toString(), Qt::ISODate);
        c.intervalDays = o["interval"].toInt(1);
        c.ease = o["ease"].toDouble(2.5);
        m_cards.append(c);
    }
}

void FlashcardStore::save()
{
    QDir().mkpath(QFileInfo(path()).absolutePath());
    QJsonArray arr;
    for (const auto &c : m_cards) {
        QJsonObject o;
        o["id"] = c.id;
        o["front"] = c.front;
        o["back"] = c.back;
        o["source"] = c.source;
        o["created"] = c.created.toString(Qt::ISODate);
        o["nextReview"] = c.nextReview.toString(Qt::ISODate);
        o["interval"] = c.intervalDays;
        o["ease"] = c.ease;
        arr.append(o);
    }
    QFile f(path());
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(arr).toJson());
}

QList<Flashcard> FlashcardStore::all() const { return m_cards; }

QList<Flashcard> FlashcardStore::dueToday() const
{
    const QDate today = QDate::currentDate();
    QList<Flashcard> out;
    for (const auto &c : m_cards)
        if (c.nextReview <= today) out.append(c);
    return out;
}

int FlashcardStore::dueCount() const { return dueToday().size(); }

void FlashcardStore::add(const QString &front, const QString &back, const QString &source)
{
    Flashcard c;
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    c.front = front;
    c.back = back;
    c.source = source;
    c.created = QDate::currentDate();
    c.nextReview = QDate::currentDate().addDays(1);
    c.intervalDays = 1;
    c.ease = 2.5;
    m_cards.prepend(c);
    save();
}

void FlashcardStore::grade(const QString &id, bool known)
{
    for (auto &c : m_cards) {
        if (c.id != id) continue;
        if (known) {
            c.ease = qMin(3.0, c.ease + 0.1);
            c.intervalDays = qMax(1, static_cast<int>(c.intervalDays * c.ease));
        } else {
            c.ease = qMax(1.3, c.ease - 0.2);
            c.intervalDays = 1;
        }
        c.nextReview = QDate::currentDate().addDays(c.intervalDays);
        break;
    }
    save();
}
