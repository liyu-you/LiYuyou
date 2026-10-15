#include "booklibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QRandomGenerator>
#include <QHash>
#include <QSaveFile>

namespace {
// QSaveFile writes to a temp file and renames into place, so a crash or power
// loss mid-write can never leave a truncated library.json / annotations.json /
// notes.json behind (which previously meant losing all books, annotations and
// reading notes at once).
bool writeJsonAtomic(const QString &path, const QJsonDocument &doc)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(doc.toJson(QJsonDocument::Compact)) < 0) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
}

BookLibrary::BookLibrary(const QString &rootPath, QObject *parent)
    : QObject(parent), m_rootPath(rootPath)
{
    QDir().mkpath(m_rootPath + "/books");
}

QString BookLibrary::metadataPath() const
{
    return m_rootPath + "/library.json";
}

QString BookLibrary::annotationsPath() const
{
    return m_rootPath + "/annotations.json";
}

QString BookLibrary::notesPath() const
{
    return m_rootPath + "/notes.json";
}

QString BookLibrary::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString BookLibrary::randomCoverColor() const
{
    const char *colors[] = {
        "#667eea,#764ba2", "#f093fb,#f5576c", "#4facfe,#00f2fe",
        "#43e97b,#38f9d7", "#fa709a,#fee140", "#30cfd0,#330867",
        "#a8edea,#fed6e3", "#ff9a9e,#fecfef", "#ffecd2,#fcb69f",
        "#84fab0,#8fd3f4", "#a1c4fd,#c2e9fb", "#d4fc79,#96e6a1"
    };
    int count = sizeof(colors) / sizeof(colors[0]);
    return QString::fromUtf8(colors[QRandomGenerator::global()->bounded(count)]);
}

bool BookLibrary::load()
{
    QFile file(metadataPath());
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    m_books.clear();
    QJsonArray arr = root["books"].toArray();
    for (const auto &v : arr) {
        QJsonObject obj = v.toObject();
        BookInfo book;
        book.id = obj["id"].toString();
        book.title = obj["title"].toString();
        book.author = obj["author"].toString();
        book.filePath = obj["filePath"].toString();
        book.coverColor = obj["coverColor"].toString();
        book.spaceId = obj["spaceId"].toString();
        book.progress = obj["progress"].toInt();
        book.totalChars = obj["totalChars"].toInt();
        book.lastRead = QDateTime::fromString(obj["lastRead"].toString(), Qt::ISODate);
        book.addedAt = QDateTime::fromString(obj["addedAt"].toString(), Qt::ISODate);
        book.totalReadSec = obj["totalReadSec"].toVariant().toLongLong();
        book.todayReadSec = obj["todayReadSec"].toVariant().toLongLong();
        book.lastActiveDate = QDate::fromString(obj["lastActiveDate"].toString(), "yyyy-MM-dd");
        // Roll over "today" counter if the saved session was on an earlier day.
        if (book.lastActiveDate.isValid() && book.lastActiveDate != QDate::currentDate())
            book.todayReadSec = 0;

        QJsonArray bmArr = obj["bookmarks"].toArray();
        for (const auto &b : bmArr) book.bookmarks << b.toInt();

        m_books << book;
    }
    loadAnnotations();
    loadNotes();
    return true;
}

bool BookLibrary::save()
{
    QJsonObject root;
    QJsonArray arr;
    for (const auto &book : m_books) {
        QJsonObject obj;
        obj["id"] = book.id;
        obj["title"] = book.title;
        obj["author"] = book.author;
        obj["filePath"] = book.filePath;
        obj["coverColor"] = book.coverColor;
        obj["spaceId"] = book.spaceId;
        obj["progress"] = book.progress;
        obj["totalChars"] = book.totalChars;
        obj["lastRead"] = book.lastRead.toString(Qt::ISODate);
        obj["addedAt"] = book.addedAt.toString(Qt::ISODate);
        obj["totalReadSec"] = static_cast<double>(book.totalReadSec);
        obj["todayReadSec"] = static_cast<double>(book.todayReadSec);
        obj["lastActiveDate"] = book.lastActiveDate.toString("yyyy-MM-dd");

        QJsonArray bmArr;
        for (int b : book.bookmarks) bmArr << b;
        obj["bookmarks"] = bmArr;
        arr << obj;
    }
    root["books"] = arr;

    return writeJsonAtomic(metadataPath(), QJsonDocument(root));
}

BookInfo BookLibrary::bookById(const QString &id) const
{
    for (const auto &b : m_books)
        if (b.id == id) return b;
    return BookInfo();
}

QString BookLibrary::importBook(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) return QString();

    // Read text file, try to detect encoding
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    QByteArray raw = file.readAll();
    file.close();

    QString text;
    // Try UTF-8 BOM
    if (raw.startsWith("\xEF\xBB\xBF")) {
        text = QString::fromUtf8(raw.mid(3));
    } else {
        // Try UTF-8, fall back to GBK
        QString utf8 = QString::fromUtf8(raw);
        if (utf8.contains(QChar(0xFFFD)) && raw.size() > 0) {
            QStringDecoder decoder("GBK");
            QString gbkText = decoder(raw);
            text = decoder.hasError() ? utf8 : gbkText;
        } else {
            text = utf8;
        }
    }

    // Normalize line endings
    text.replace("\r\n", "\n").replace("\r", "\n");

    // Extract title from filename or first line
    QString title = fi.completeBaseName();
    QString author = "未知";

    // Try to find author in filename like "书名_作者.txt"
    if (title.contains("_")) {
        QStringList parts = title.split("_");
        if (parts.size() >= 2) {
            title = parts.first();
            author = parts.last();
        }
    }

    // Copy file to library
    QString destName = fi.fileName();
    QString destPath = m_rootPath + "/books/" + destName;
    int counter = 1;
    while (QFile::exists(destPath)) {
        destName = fi.baseName() + "_" + QString::number(counter) + "." + fi.suffix();
        destPath = m_rootPath + "/books/" + destName;
        counter++;
    }
    QFile::copy(filePath, destPath);

    BookInfo book;
    book.id = generateId();
    book.title = title;
    book.author = author;
    book.filePath = "books/" + destName;
    book.coverColor = randomCoverColor();
    book.progress = 0;
    book.totalChars = text.length();
    book.lastRead = QDateTime::currentDateTime();
    book.addedAt = QDateTime::currentDateTime();
    m_books << book;

    save();
    emit libraryChanged();
    return book.id;
}

bool BookLibrary::removeBook(const QString &id)
{
    for (int i = 0; i < m_books.size(); ++i) {
        if (m_books[i].id == id) {
            QFile::remove(m_rootPath + "/" + m_books[i].filePath);
            m_books.removeAt(i);
            m_annotations.remove(id);
            m_notes.remove(id);
            save();
            saveAnnotations();
            saveNotes();
            emit libraryChanged();
            return true;
        }
    }
    return false;
}

bool BookLibrary::setBookSpace(const QString &id, const QString &spaceId)
{
    for (auto &book : m_books) {
        if (book.id == id) {
            book.spaceId = spaceId;
            save();
            emit libraryChanged();
            return true;
        }
    }
    return false;
}

bool BookLibrary::updateProgress(const QString &id, int pos)
{
    for (auto &book : m_books) {
        if (book.id == id) {
            book.progress = pos;
            book.lastRead = QDateTime::currentDateTime();
            save();
            return true;
        }
    }
    return false;
}

void BookLibrary::addReadingSeconds(const QString &id, qint64 secs)
{
    if (id.isEmpty() || secs <= 0) return;
    const QDate today = QDate::currentDate();
    for (auto &book : m_books) {
        if (book.id == id) {
            // New calendar day: "today" counter starts fresh.
            if (book.lastActiveDate != today) {
                book.lastActiveDate = today;
                book.todayReadSec = 0;
            }
            book.totalReadSec += secs;
            book.todayReadSec += secs;
            return;
        }
    }
}

qint64 BookLibrary::totalTodayReadSec() const
{
    const QDate today = QDate::currentDate();
    qint64 sum = 0;
    for (const auto &b : m_books)
        if (b.lastActiveDate == today) sum += b.todayReadSec;
    return sum;
}

qint64 BookLibrary::totalAllReadSec() const
{
    qint64 sum = 0;
    for (const auto &b : m_books) sum += b.totalReadSec;
    return sum;
}

bool BookLibrary::toggleBookmark(const QString &id, int pos)
{
    for (auto &book : m_books) {
        if (book.id == id) {
            if (book.bookmarks.contains(pos))
                book.bookmarks.removeAll(pos);
            else
                book.bookmarks << pos;
            std::sort(book.bookmarks.begin(), book.bookmarks.end());
            save();
            return true;
        }
    }
    return false;
}

bool BookLibrary::hasBookmark(const QString &id, int pos) const
{
    BookInfo book = bookById(id);
    return book.bookmarks.contains(pos);
}

QList<int> BookLibrary::bookmarks(const QString &id) const
{
    return bookById(id).bookmarks;
}

// ========== Annotations ==========

QList<Annotation> BookLibrary::annotations(const QString &bookId) const
{
    return m_annotations.value(bookId);
}

QString BookLibrary::addAnnotation(const QString &bookId, int start, int length,
                                   const QString &color, const QString &note)
{
    if (bookId.isEmpty() || length <= 0) return QString();

    Annotation ann;
    ann.id = generateId();
    ann.start = start;
    ann.length = length;
    ann.color = color;
    ann.note = note;
    ann.createdAt = QDateTime::currentDateTime();

    QList<Annotation> &list = m_annotations[bookId];

    // Skip if the exact same span is already highlighted.
    for (const Annotation &a : list) {
        if (a.start == start && a.length == length) return a.id;
    }

    list.append(ann);
    std::sort(list.begin(), list.end(),
              [](const Annotation &a, const Annotation &b) { return a.start < b.start; });
    saveAnnotations();
    return ann.id;
}

bool BookLibrary::updateAnnotation(const QString &bookId, const QString &annId,
                                   const QString &color, const QString &note)
{
    QList<Annotation> &list = m_annotations[bookId];
    for (auto &a : list) {
        if (a.id == annId) {
            if (!color.isEmpty()) a.color = color;
            a.note = note;
            saveAnnotations();
            return true;
        }
    }
    return false;
}

bool BookLibrary::removeAnnotation(const QString &bookId, const QString &annId)
{
    QList<Annotation> &list = m_annotations[bookId];
    const int n = list.removeIf([&](const Annotation &a) { return a.id == annId; });
    if (n > 0) {
        saveAnnotations();
        return true;
    }
    return false;
}

bool BookLibrary::loadAnnotations()
{
    m_annotations.clear();
    QFile file(annotationsPath());
    if (!file.open(QIODevice::ReadOnly)) return true; // no annotations yet

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QList<Annotation> list;
        QJsonArray arr = it.value().toArray();
        for (const auto &v : arr) {
            QJsonObject obj = v.toObject();
            Annotation a;
            a.id = obj["id"].toString();
            a.start = obj["start"].toInt();
            a.length = obj["length"].toInt();
            a.color = obj["color"].toString("yellow");
            a.note = obj["note"].toString();
            a.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
            if (a.id.isEmpty() || a.length <= 0) continue;
            list.append(a);
        }
        m_annotations[it.key()] = list;
    }
    return true;
}

bool BookLibrary::saveAnnotations() const
{
    QJsonObject root;
    for (auto it = m_annotations.constBegin(); it != m_annotations.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        QJsonArray arr;
        for (const Annotation &a : it.value()) {
            QJsonObject obj;
            obj["id"] = a.id;
            obj["start"] = a.start;
            obj["length"] = a.length;
            obj["color"] = a.color;
            obj["note"] = a.note;
            obj["createdAt"] = a.createdAt.toString(Qt::ISODate);
            arr << obj;
        }
        root[it.key()] = arr;
    }
    return writeJsonAtomic(annotationsPath(), QJsonDocument(root));
}

// ========== Reading notes ==========

QList<BookNote> BookLibrary::notes(const QString &bookId) const
{
    return m_notes.value(bookId);
}

QString BookLibrary::addNote(const QString &bookId, int startPos, const QString &text)
{
    if (bookId.isEmpty() || text.trimmed().isEmpty()) return QString();

    BookNote n;
    n.id = generateId();
    n.text = text.trimmed();
    n.startPos = startPos;
    n.createdAt = QDateTime::currentDateTime();

    QList<BookNote> &list = m_notes[bookId];
    list.append(n);
    std::sort(list.begin(), list.end(),
              [](const BookNote &a, const BookNote &b) { return a.startPos < b.startPos; });
    saveNotes();
    return n.id;
}

bool BookLibrary::removeNote(const QString &bookId, const QString &noteId)
{
    QList<BookNote> &list = m_notes[bookId];
    const int n = list.removeIf([&](const BookNote &x) { return x.id == noteId; });
    if (n > 0) {
        saveNotes();
        return true;
    }
    return false;
}

bool BookLibrary::loadNotes()
{
    m_notes.clear();
    QFile file(notesPath());
    if (!file.open(QIODevice::ReadOnly)) return true; // no notes yet

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        QList<BookNote> list;
        QJsonArray arr = it.value().toArray();
        for (const auto &v : arr) {
            QJsonObject obj = v.toObject();
            BookNote n;
            n.id = obj["id"].toString();
            n.text = obj["text"].toString();
            n.startPos = obj["startPos"].toInt();
            n.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
            if (n.id.isEmpty() || n.text.isEmpty()) continue;
            list.append(n);
        }
        m_notes[it.key()] = list;
    }
    return true;
}

bool BookLibrary::saveNotes() const
{
    QJsonObject root;
    for (auto it = m_notes.constBegin(); it != m_notes.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        QJsonArray arr;
        for (const BookNote &n : it.value()) {
            QJsonObject obj;
            obj["id"] = n.id;
            obj["text"] = n.text;
            obj["startPos"] = n.startPos;
            obj["createdAt"] = n.createdAt.toString(Qt::ISODate);
            arr << obj;
        }
        root[it.key()] = arr;
    }
    return writeJsonAtomic(notesPath(), QJsonDocument(root));
}

QString BookLibrary::loadBookText(const QString &id) const
{
    BookInfo book = bookById(id);
    if (book.id.isEmpty()) return QString();

    QFile file(m_rootPath + "/" + book.filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    QByteArray raw = file.readAll();
    file.close();

    QString text;
    if (raw.startsWith("\xEF\xBB\xBF")) {
        text = QString::fromUtf8(raw.mid(3));
    } else {
        QString utf8 = QString::fromUtf8(raw);
        if (utf8.contains(QChar(0xFFFD))) {
            QStringDecoder decoder("GBK");
            QString gbkText = decoder(raw);
            text = decoder.hasError() ? utf8 : gbkText;
        } else {
            text = utf8;
        }
    }
    text.replace("\r\n", "\n").replace("\r", "\n");
    return text;
}

QList<ChapterInfo> BookLibrary::extractChapters(const QString &text) const
{
    QList<ChapterInfo> chapters;
    // NOTE: only horizontal whitespace before 第… — \s* would match newlines and
    // drift the captured start position backwards onto the preceding blank line,
    // which breaks heading detection and chapter jumps.
    QRegularExpression re("^([ \\t\\x{3000}]*)(第[一二三四五六七八九十百千万零〇两0-9]+[章回卷节篇][^\\n]*)",
                           QRegularExpression::MultilineOption);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        ChapterInfo ch;
        ch.title = match.captured(2).trimmed();
        ch.startPos = match.capturedStart(2); // exact position of the 第… heading
        chapters << ch;
    }
    return chapters;
}

QString BookLibrary::findChapterTitle(const QString &text, int pos) const
{
    QList<ChapterInfo> chapters = extractChapters(text);
    QString current = "正文";
    for (const auto &ch : chapters) {
        if (ch.startPos <= pos) current = ch.title;
        else break;
    }
    return current;
}
