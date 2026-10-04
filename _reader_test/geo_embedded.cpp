// Embedded geometry probe: ContentBrowserPanel | QStackedWidget(ReaderWidget),
// exactly mirroring MainWindow::setupPages central layout.
#include <QApplication>
#include <QHBoxLayout>
#include <QStackedWidget>
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
#include "contentbrowserpanel.h"
#include "materiallibrary.h"

static void dump(const char *tag, QWidget *w)
{
    printf("%-22s x=%4d w=%4d\n", tag, w->x(), w->width());
    fflush(stdout);
}

static void log(const char *msg)
{
    FILE *f = fopen("C:/Users/LiyuYou/Desktop/Q/_reader_test/probe.log", "a");
    if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

int main(int argc, char **argv)
{
    log("start");
    QApplication app(argc, argv);
    log("app ok");
    const QString root = QStringLiteral("C:/Users/LiyuYou/Documents/PureRefBooks");

    MaterialLibrary lib(QStringLiteral("C:/Users/LiyuYou/Documents/PureRefLibrary"));
    log("step1: library ok");
    ContentBrowserPanel browser(&lib);
    log("step2: browser ok");
    ReaderWidget reader(root);
    log("step3: reader ok");
    QStackedWidget stack;
    stack.addWidget(&reader);
    log("step4: stack ok");

    QWidget central;
    QHBoxLayout *hl = new QHBoxLayout(&central);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    hl->addWidget(&browser);
    hl->addWidget(&stack, 1);

    central.resize(1177, 1025);
    central.show();
    log("step5: shown");
    app.processEvents();
    log("step6: events ok");

    BookLibrary *bl = reader.library();
    bl->load();
    log("step7: lib loaded");
    QString bookId;
    for (const auto &b : bl->books())
        if (b.title.contains(QStringLiteral("三国"))) bookId = b.id;
    reader.openBookById(bookId);
    for (int i = 0; i < 6; ++i) app.processEvents();
    log("step8: book opened");

    QTextEdit *view = reader.findChild<QTextEdit *>();
    printf("=== embedded 1177x1025 ===\n");
    dump("contentBrowser", &browser);
    dump("stack", &stack);
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
    fflush(stdout);
    printf("done\n");
    fflush(stdout);
    _Exit(0);   // skip Qt teardown (grabbing/exit hangs on this machine)
    return 0;
}
