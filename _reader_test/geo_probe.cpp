// Geometry probe: does the reading container fill the reader page?
// Mirrors mainwindow's central layout: ContentBrowserPanel | QStackedWidget(ReaderWidget)
#include <QApplication>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QLabel>
#include <QListWidget>
#include <QSlider>
#include <QPushButton>
#include <cstdio>
#include "readerwidget.h"
#include "booklibrary.h"
#include "contentbrowserpanel.h"
#include "materiallibrary.h"

static void dump(const char *tag, QWidget *w)
{
    printf("%-22s x=%4d w=%4d\n", tag, w->x(), w->width());
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString root = QStringLiteral("C:/Users/LiyuYou/Documents/PureRefBooks");
    const QString mode = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral("all");

    // ============ 1) standalone reader ============
    if (mode == QStringLiteral("all") || mode == QStringLiteral("standalone"))
    {
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
        // chapter label
        for (QLabel *lbl : reader.findChildren<QLabel *>())
            if (lbl->text().contains(QStringLiteral("第一回")) && lbl->parent() && lbl->parent()->objectName().isEmpty())
                printf("chapterLabel      x=%d w=%d\n", lbl->x(), lbl->width());
        QWidget *sliderParent = reader.findChild<QSlider *>()->parentWidget();
        dump("progressSlider parent", sliderParent);
        reader.grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/geo_standalone.png"), "PNG");
    }

    // ============ 2) embedded like the real main window ============
    if (mode == QStringLiteral("all") || mode == QStringLiteral("embedded"))
    {
        MaterialLibrary lib(QStringLiteral("C:/Users/LiyuYou/Documents/PureRefLibrary"));
        ContentBrowserPanel browser(&lib);
        ReaderWidget reader(root);
        QStackedWidget stack;
        stack.addWidget(&reader);

        QWidget central;
        QHBoxLayout *hl = new QHBoxLayout(&central);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        hl->addWidget(&browser);
        hl->addWidget(&stack, 1);

        central.resize(1177, 1025);
        central.show();
        app.processEvents();

        BookLibrary *bl = reader.library();
        bl->load();
        QString bookId;
        for (const auto &b : bl->books())
            if (b.title.contains(QStringLiteral("三国"))) bookId = b.id;
        reader.openBookById(bookId);
        for (int i = 0; i < 6; ++i) app.processEvents();

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
        central.grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/geo_embedded.png"), "PNG");
    }
    printf("done\n");
    return 0;
}
