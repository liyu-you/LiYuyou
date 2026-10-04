#ifndef BOOKLIBRARY_H
#define BOOKLIBRARY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QDateTime>

struct ChapterInfo {
    QString title;
    int startPos;
};

// Text highlight / annotation anchored to character positions in the book text.
struct Annotation {
    QString id;
    int start = 0;           // character position in book text
    int length = 0;          // span in characters
    QString color;           // yellow | green | blue | pink
    QString note;            // optional note attached to the highlight
    QDateTime createdAt;
};

// Free-form reading note anchored to a character position (where it was written).
struct BookNote {
    QString id;
    QString text;
    int startPos = 0;        // character position in book text
    QDateTime createdAt;
};

struct BookInfo {
    QString id;
    QString title;
    QString author;
    QString filePath;      // relative to library root
    QString coverColor;    // gradient color for cover
    int progress;          // current character position
    int totalChars;
    QList<int> bookmarks;  // character positions
    QList<Annotation> annotations;
    QDateTime lastRead;
    QDateTime addedAt;
    // Reading-time accounting (seconds). todayReadSec is the cumulative seconds
    // on `lastActiveDate`; it rolls over to 0 when the calendar day changes.
    qint64 totalReadSec = 0;
    qint64 todayReadSec = 0;
    QDate lastActiveDate;
};

class BookLibrary : public QObject
{
    Q_OBJECT
public:
    explicit BookLibrary(const QString &rootPath, QObject *parent = nullptr);

    bool load();
    bool save();

    QList<BookInfo> books() const { return m_books; }
    BookInfo bookById(const QString &id) const;

    QString importBook(const QString &filePath);
    bool removeBook(const QString &id);
    bool updateProgress(const QString &id, int pos);
    bool toggleBookmark(const QString &id, int pos);
    bool hasBookmark(const QString &id, int pos) const;
    QList<int> bookmarks(const QString &id) const;

    // Accumulate `secs` of active reading into the book's total + today counters.
    // Handles calendar-day rollover for todayReadSec. Does NOT persist — caller
    // decides the save cadence.
    void addReadingSeconds(const QString &id, qint64 secs);
    // Aggregate reading seconds across all books (today / all-time), with day rollover.
    qint64 totalTodayReadSec() const;
    qint64 totalAllReadSec() const;

    QString loadBookText(const QString &id) const;
    QList<ChapterInfo> extractChapters(const QString &text) const;
    QString findChapterTitle(const QString &text, int pos) const;

    // Annotations (highlights + notes). Persisted separately in annotations.json.
    QList<Annotation> annotations(const QString &bookId) const;
    QString addAnnotation(const QString &bookId, int start, int length,
                          const QString &color, const QString &note = QString());
    bool updateAnnotation(const QString &bookId, const QString &annId,
                          const QString &color, const QString &note);
    bool removeAnnotation(const QString &bookId, const QString &annId);

    // Free-form reading notes. Persisted separately in notes.json.
    QList<BookNote> notes(const QString &bookId) const;
    QString addNote(const QString &bookId, int startPos, const QString &text);
    bool removeNote(const QString &bookId, const QString &noteId);

    QString rootPath() const { return m_rootPath; }

signals:
    void libraryChanged();

private:
    QString m_rootPath;
    QList<BookInfo> m_books;
    QHash<QString, QList<Annotation>> m_annotations; // bookId -> annotations
    QHash<QString, QList<BookNote>> m_notes;         // bookId -> notes

    QString metadataPath() const;
    QString annotationsPath() const;
    QString notesPath() const;
    bool loadAnnotations();
    bool saveAnnotations() const;
    bool loadNotes();
    bool saveNotes() const;
    QString generateId() const;
    QString randomCoverColor() const;
};

#endif
