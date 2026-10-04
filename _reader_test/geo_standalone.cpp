// Minimal standalone geometry probe (mirrors previously-working shot_fast setup)
#include <QApplication>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QLabel>
#include <QSlider>
#include <cstdio>
#include "readerwidget.h"
#include "booklibrary.h"

static void dump(const char *tag, QWidget *w)
{
    printf("%-22s x=%4d w=%4d\n", tag, w->x(), w->width());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString root = QStringLiteral("C:/Users/LiyuYou/Documents/PureRefBooks");

    ReaderWidget reader(root);
    reader.resize(977, 800);
    reader.show();
    app.processEvents();
    BookLibrary *lib = reader.library();
    lib->load();
    QString bookId;
    for (const auto &b : lib->books())
        if (b.title.contains(QStringLiteral("三国"))) bookId = b.id;
    reader.openBookById(bookId);
    for (int i = 0; i < 6; ++i) app.processEvents();

    QTextEdit *view = reader.findChild<QTextEdit *>();
    printf("=== standalone reader 977x800 ===\n");
    dump("readerWidget", &reader);
    dump("readingView", view);
    printf("view viewport     w=%d\n", view->viewport()->width());
    const QTextDocument *doc = view->document();
    const QTextBlock &b0 = doc->firstBlock();
    QTextLayout *tl = b0.layout();
    printf("doc textWidth    %d\n", doc->textWidth());
    printf("doc pageWidth    %.1f\n", doc->documentLayout()->documentSize().width());
    printf("firstBlock lineWidth %.1f\n", tl->lineAt(0).width());
    for (QLabel *lbl : reader.findChildren<QLabel *>())
        if (lbl->text().contains(QStringLiteral("第一回")) && lbl->parent() && lbl->parent()->objectName().isEmpty())
            printf("chapterLabel      x=%d w=%d\n", lbl->x(), lbl->width());
    reader.grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/geo_standalone.png"), "PNG");

    // ===== resize AFTER opening the book: does the doc relayout? =====
    printf("--- resize after open: 977 -> 1300 ---\n"); fflush(stdout);
    reader.resize(1300, 900);
    reader.show();
    for (int i = 0; i < 6; ++i) app.processEvents();
    printf("readerWidget        w=%d\n", reader.width()); fflush(stdout);
    printf("readingView         w=%d\n", view->width()); fflush(stdout);
    printf("view viewport       w=%d\n", view->viewport()->width()); fflush(stdout);
    printf("doc pageWidth       %.1f\n", doc->documentLayout()->documentSize().width()); fflush(stdout);
    printf("firstBlock lineWidth %.1f\n", tl->lineAt(0).width()); fflush(stdout);

    printf("done\n");
    return 0;
}
