// Full real-app reproduction: construct MainWindow exactly like main.cpp does,
// switch to the reader page, open the book, dump geometry.
#include <QApplication>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QThread>
#include <QLayout>
#include <QStackedWidget>
#include <QComboBox>
#include <QListWidget>
#include <QGraphicsView>
#include <QWheelEvent>
#include "notewidget.h"
#include "materialwidget.h"
#include <cstdio>
#include "mainwindow.h"
#include "readerwidget.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1177, 1025);
    w.show();
    for (int i = 0; i < 8; ++i) app.processEvents();

    ReaderWidget *reader = w.findChild<ReaderWidget *>();
    printf("readerWidget found: %p\n", (void *)reader); fflush(stdout);
    if (!reader) { printf("NO READER\n"); fflush(stdout); return 1; }

    QWidget *stack = qobject_cast<QWidget *>(reader->parentWidget());
    QWidget *central = stack ? qobject_cast<QWidget *>(stack->parentWidget()) : nullptr;
    printf("reader visible: %d  stack=%p central=%p\n", reader->isVisible(),
           (void *)stack, (void *)central); fflush(stdout);
    if (stack) printf("stack           x=%d w=%d visible=%d\n", stack->x(), stack->width(), stack->isVisible());
    if (central) printf("central        x=%d w=%d\n", central->x(), central->width());
    printf("readerWidget      x=%d w=%d\n", reader->x(), reader->width()); fflush(stdout);
    // open the real book
    auto *bl = reader->library();
    bl->load();
    QString bookId;
    for (const auto &b : bl->books())
        if (b.title.contains(QStringLiteral("三国"))) bookId = b.id;
    printf("bookId: %s\n", qPrintable(bookId)); fflush(stdout);
    reader->openBookById(bookId);
    for (int i = 0; i < 8; ++i) app.processEvents();

    QTextEdit *view = reader->findChild<QTextEdit *>();
    printf("readingView       x=%d w=%d\n", view->x(), view->width()); fflush(stdout);
    printf("view viewport     w=%d\n", view->viewport()->width()); fflush(stdout);
    const QTextDocument *doc = view->document();
    printf("doc textWidth     %d\n", doc->textWidth()); fflush(stdout);
    printf("doc pageWidth     %.1f\n", doc->documentLayout()->documentSize().width()); fflush(stdout);
    const QTextBlock &b0 = doc->firstBlock();
    if (b0.layout()->lineCount() > 0)
        printf("firstBlock lineWidth %.1f\n", b0.layout()->lineAt(0).width()); fflush(stdout);

    for (QLabel *lbl : reader->findChildren<QLabel *>())
        if (lbl->text().contains(QStringLiteral("第一回")) && lbl->parent()
            && lbl->parent() != reader && lbl->parent()->objectName().isEmpty())
            printf("chapterLabel      x=%d w=%d\n", lbl->x(), lbl->width());
    fflush(stdout);

    // Resize the window after the book is open (as users do), then re-measure.
    w.resize(1282, 832);
    for (int i = 0; i < 8; ++i) app.processEvents();
    printf("--- after resize to 1282x832 ---\n"); fflush(stdout);
    printf("readerWidget      w=%d visible=%d\n", reader->width(), reader->isVisible()); fflush(stdout);
    if (stack) printf("stack           w=%d\n", stack->width());
    printf("readingView       w=%d\n", view->width()); fflush(stdout);
    printf("view viewport     w=%d\n", view->viewport()->width()); fflush(stdout);
    printf("doc pageWidth     %.1f\n", doc->documentLayout()->documentSize().width()); fflush(stdout);

    printf("done\n"); fflush(stdout);

    // ===== EXACT user flow: open book while reader hidden, then switch to reader =====
    printf("--- user flow: open book then click 阅读 ---\n"); fflush(stdout);
    {
        // Reopen the book (fresh doc build at the CURRENT stale width)
        reader->openBookById(bookId);
        for (int i = 0; i < 8; ++i) app.processEvents();
        printf("after reopen (reader hidden): view w=%d doc pageWidth=%.1f\n",
               view->width(), doc->documentLayout()->documentSize().width()); fflush(stdout);

        // Click the 阅读 nav button (public UI path to switchToPage(3))
        bool clicked = false;
        const auto btns = w.findChildren<QPushButton *>();
        for (QPushButton *b : btns) {
            if (b->text() == QStringLiteral("阅读")) { b->click(); clicked = true; break; }
        }
        printf("nav click: %d\n", clicked); fflush(stdout);
        for (int i = 0; i < 8; ++i) app.processEvents();
        const QTextDocument *curDoc = view->document();   // CURRENT doc (may be a NEW one after reopen)
        printf("after nav: reader visible=%d w=%d, view w=%d vp=%d, doc pageWidth=%.1f\n",
               reader->isVisible(), reader->width(), view->width(), view->viewport()->width(),
               curDoc->documentLayout()->documentSize().width()); fflush(stdout);
        {
            // Dump the full geometry chain inside the reader widget
            QWidget *vp = view->parentWidget();
            while (vp) {
                printf("  [chain] %s vis=%d geo=%d,%d %dx%d\n",
                       vp->objectName().isEmpty() ? vp->metaObject()->className() : qPrintable(vp->objectName()),
                       vp->isVisible(), vp->x(), vp->y(), vp->width(), vp->height()); fflush(stdout);
                vp = vp->parentWidget();
                if (vp == reader) { printf("  [chain] ReaderWidget geo=%d,%d %dx%d\n", vp->x(), vp->y(), vp->width(), vp->height()); break; }
            }
            for (QWidget *p : reader->findChildren<QWidget *>()) {
                if (p->objectName().contains(QStringLiteral("annoPanel")) ||
                    p->objectName().contains(QStringLiteral("notePanel")))
                    printf("  [panel] %s vis=%d geo=%d,%d %dx%d\n", qPrintable(p->objectName()),
                           p->isVisible(), p->x(), p->y(), p->width(), p->height());
            }
        }
        const QTextBlock &bn = curDoc->firstBlock();
        if (bn.layout()->lineCount() > 0) {
            printf("  current-doc firstBlock lineWidth=%.1f\n", bn.layout()->lineAt(0).width()); fflush(stdout);
        }
        reader->grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_after_nav.png"));
        // Then resize the window and check again
        w.resize(1282, 832);
        for (int i = 0; i < 40; ++i) { app.processEvents(); QThread::msleep(10); }
        printf("after window resize: window=%dx%d central=%d stack=%d reader=%d view=%d vp=%d\n",
               w.width(), w.height(),
               central->width(), (stack ? stack->width() : -1), reader->width(),
               view->width(), view->viewport()->width()); fflush(stdout);
        if (stack) {
            printf("  stack max=%dx%d policy=%d/%d  layoutActive=%d\n",
                   stack->maximumWidth(), stack->maximumHeight(),
                   int(stack->sizePolicy().horizontalPolicy()), int(stack->sizePolicy().verticalPolicy()),
                   (central->layout() && central->layout()->isEnabled()) ? 1 : 0); fflush(stdout);
            QWidget *cb = stack->parentWidget()->findChild<QWidget *>(QStringLiteral("contentBrowserPanel"));
            if (cb) printf("  contentBrowser w=%d hint=%d min=%d\n", cb->width(), cb->sizeHint().width(), cb->minimumSizeHint().width());
            if (central->layout()) { central->layout()->activate(); for (int i = 0; i < 8; ++i) app.processEvents(); }
            printf("  after layout->activate(): stack=%d reader=%d view=%d\n",
                   stack->width(), reader->width(), view->width()); fflush(stdout);
            w.resize(1700, 900);
            for (int i = 0; i < 40; ++i) { app.processEvents(); QThread::msleep(10); }
            printf("  after window 1700: window=%d stack=%d reader=%d view=%d\n",
                   w.width(), stack->width(), reader->width(), view->width()); fflush(stdout);
            if (central->layout()) central->layout()->activate();
            for (int i = 0; i < 8; ++i) app.processEvents();
            printf("  after activate 1700: stack=%d reader=%d view=%d\n",
                   stack->width(), reader->width(), view->width()); fflush(stdout);
            const QTextBlock &b1700 = view->document()->firstBlock();
            printf("  doc pageWidth=%.1f firstBlock lineWidth=%.1f\n",
                   view->document()->documentLayout()->documentSize().width(),
                   b1700.layout()->lineCount() > 0 ? b1700.layout()->lineAt(0).width() : 0.0); fflush(stdout);
        }
        printf("  doc pageWidth=%.1f\n",
               view->document()->documentLayout()->documentSize().width()); fflush(stdout);
        const QTextBlock &bn2 = view->document()->firstBlock();
        if (bn2.layout()->lineCount() > 0) {
            printf("  current-doc firstBlock lineWidth=%.1f\n", bn2.layout()->lineAt(0).width()); fflush(stdout);
        }
        reader->grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_after_resize.png"));
    }

    // ===== Note workspace (笔记页) =====
    printf("--- note workspace ---\n"); fflush(stdout);
    {
        bool clicked = false;
        const auto btns = w.findChildren<QPushButton *>();
        for (QPushButton *b : btns) {
            if (b->text() == QStringLiteral("笔记")) { b->click(); clicked = true; break; }
        }
        printf("note nav click: %d\n", clicked); fflush(stdout);
        for (int i = 0; i < 60; ++i) { app.processEvents(); QThread::msleep(10); }  // wait deferred load

        const auto notes = w.findChildren<NoteWidget *>();
        if (!notes.isEmpty()) {
            NoteWidget *nw = notes.first();
            printf("NoteWidget found: visible=%d\n", nw->isVisible()); fflush(stdout);
            nw->grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_note_ws.png"));
            const auto lists = nw->findChildren<QListWidget *>();
            for (QListWidget *l : lists) {
                if (l->objectName().isEmpty())
                    printf("  sidebar list rows=%d\n", l->count()); fflush(stdout);
            }
            const auto combos = nw->findChildren<QComboBox *>();
            for (QComboBox *c : combos) {
                printf("  combo items=[%s] cur=%d\n", qPrintable(c->itemText(0)), c->currentIndex()); fflush(stdout);
            }
            const auto btns2 = nw->findChildren<QPushButton *>();
            for (QPushButton *b : btns2) {
                if (b->text().contains(QStringLiteral("导出")))
                    printf("  button: %s\n", qPrintable(b->text())); fflush(stdout);
            }
            // sidebar toggle (Ctrl+8): fold → unfold, width must come back
            QWidget *sb = nw->findChild<QWidget *>(QStringLiteral("noteSidebar"));
            if (sb) {
                const bool before = sb->isVisible();
                QMetaObject::invokeMethod(&w, "toggleSidebar");
                for (int i = 0; i < 5; ++i) app.processEvents();
                const bool folded = sb->isVisible();
                QMetaObject::invokeMethod(&w, "toggleSidebar");
                for (int i = 0; i < 5; ++i) app.processEvents();
                printf("  sidebar toggle: before=%d folded=%d restored=%d\n",
                       int(before), int(folded), int(sb->isVisible())); fflush(stdout);
            }
        } else {
            printf("NoteWidget NOT FOUND\n"); fflush(stdout);
        }
        w.show();
        for (int i = 0; i < 8; ++i) app.processEvents();
    }

    // ===== Library preview (PureRef-style zoom) =====
    printf("--- library preview ---\n"); fflush(stdout);
    {
        bool clicked = false;
        const auto btns = w.findChildren<QPushButton *>();
        for (QPushButton *b : btns) {
            if (b->text() == QStringLiteral("库")) { b->click(); clicked = true; break; }
        }
        printf("library nav click: %d\n", clicked); fflush(stdout);
        for (int i = 0; i < 20; ++i) app.processEvents();

        const auto mats = w.findChildren<MaterialWidget *>();
        if (mats.isEmpty()) { printf("MaterialWidget NOT FOUND\n"); fflush(stdout); }
        else {
            MaterialWidget *mw = mats.first();
            MasonryContainer *mc = mw->findChild<MasonryContainer *>();
            printf("masonry cards=%d\n", mc ? mc->count() : -1); fflush(stdout);
            if (mc && mc->count() > 0) {
                QWidget *card = mc->cards().first();
                mc->cardDoubleClicked(card);   // public signal -> opens preview page
                for (int i = 0; i < 30; ++i) { app.processEvents(); QThread::msleep(10); }
                QGraphicsView *pv = nullptr;
                const auto views = mw->findChildren<QGraphicsView *>();
                for (QGraphicsView *v : views) { if (v->isVisible()) { pv = v; break; } }
                if (pv) {
                    printf("preview view visible=%d size=%dx%d\n", pv->isVisible(), pv->width(), pv->height()); fflush(stdout);
                    // read zoom label (text ending with %)
                    QString zoomText;
                    const auto labs = mw->findChildren<QLabel *>();
                    for (QLabel *l : labs) {
                        if (l->isVisible() && l->text().contains(QChar('%'))) { zoomText = l->text(); break; }
                    }
                    printf("zoom label = %s\n", qPrintable(zoomText)); fflush(stdout);
                    // send wheel-up (zoom in) at view center
                    QWheelEvent we(QPointF(pv->width() / 2.0, pv->height() / 2.0),
                                   QPointF(200, 200), QPoint(), QPoint(0, 120),
                                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                    QApplication::sendEvent(pv, &we);
                    for (int i = 0; i < 10; ++i) app.processEvents();
                    QString zoomAfter;
                    for (QLabel *l : labs) {
                        if (l->isVisible() && l->text().contains(QChar('%'))) { zoomAfter = l->text(); break; }
                    }
                    printf("zoom after wheel = %s\n", qPrintable(zoomAfter)); fflush(stdout);
                    mw->grab().save(QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_lib_preview.png"));
                }
            }
        }
    }
    printf("all done\n"); fflush(stdout);
    _Exit(0);
}
