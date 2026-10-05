#pragma once
// Minimal flashcard store: JSON file in Documents/PureRefFlashcards/cards.json.
// SM-2 simplified: ease factor, interval in days, next review date.
#include <QString>
#include <QDateTime>
#include <QList>

struct Flashcard {
    QString id;
    QString front;       // question / prompt
    QString back;        // answer / excerpt
    QString source;      // book title / note title
    QDate created;
    QDate nextReview;
    int intervalDays = 1;
    double ease = 2.5;
};

class FlashcardStore {
public:
    static FlashcardStore &instance();
    QList<Flashcard> all() const;
    QList<Flashcard> dueToday() const;
    void add(const QString &front, const QString &back, const QString &source);
    void grade(const QString &id, bool known); // known=true -> ease up, interval*2; else reset to 1 day
    int dueCount() const;

private:
    FlashcardStore();
    QString path() const;
    void load();
    void save();
    QList<Flashcard> m_cards;
};
