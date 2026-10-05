// Wrap-width probe: open a book at the user's window size (no resize), then
// shrink/widen — the doc must re-wrap to the viewport on every path. Built as
// the `wrapprobe` target in _reader_test (see CMakeLists.txt); run via
// runwrapprobe.py (offscreen + plugin path).
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <QTextDocument>
#include <QTextEdit>
#include <cstdio>

#include "readerwidget.h"
#include "booklibrary.h"

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static void report(ReaderWidget *reader, const char *tag)
{
    // Reach the reading view through the widget tree.
    QTextEdit *view = reader->findChild<QTextEdit *>();
    QTextDocument *doc = view ? view->document() : nullptr;
    if (doc)
        std::printf("%s: view=%d viewport=%d docPageW=%.0f idealW=%.0f\n", tag,
                    view->width(), view->viewport()->width(),
                    doc->pageSize().width(), doc->idealWidth());
    else
        std::printf("%s: no view\n", tag);
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const QString root = QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/scratch_wrapprobe");
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    // A long enough book to be realistic.
    QString text;
    const QString para = QStringLiteral("甄士隱夢幻識通靈賈雨村風塵懷閨秀，此開卷第一回也。作者自云曾歷過一番夢幻之後，故將真事隱去，而借通靈之說撰此石頭記一書也。故曰甄士隱云云。但書中所記何事何人，自又云而今風塵碌碌一事無成，忽念及當日所有之女子，一一細考較去，覺其行止見識皆出於我之上。");
    for (int i = 0; i < 60; ++i) text += para + "\n";
    text = QStringLiteral("第一回　甄士隱夢幻識通靈　賈雨村風塵懷閨秀\n") + text;
    const QString bookPath = root + "/_wrap_book.txt";
    QFile f(bookPath);
    f.open(QIODevice::WriteOnly);
    f.write(text.toUtf8());
    f.close();

    BookLibrary lib(root);
    const QString id = lib.importBook(bookPath);
    std::printf("imported id empty=%d\n", int(id.isEmpty())); std::fflush(stdout);

    ReaderWidget reader(root, nullptr);
    reader.resize(1280, 1078); // the user's window size, BEFORE any shrink/resize
    reader.show();
    pump(600); // allow the deferred library load to finish

    reader.openBookById(id);
    pump(400);
    report(&reader, "open-1280  ");
    {
        QTextEdit *view = reader.findChild<QTextEdit *>();
        if (view)
            std::printf("  chars=%d blocks=%d\n", view->document()->characterCount(),
                        view->document()->blockCount());
    }
    reader.grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_wrap_narrow.png"));

    // Then simulate a fresh app start at half width, then widen (stale pin path)
    reader.resize(640, 700);
    pump(300);
    report(&reader, "shrink-640 ");
    reader.resize(1280, 900);
    pump(300);
    report(&reader, "widen-1280 ");
    reader.grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_wrap_wide.png"));
    return 0;
}
