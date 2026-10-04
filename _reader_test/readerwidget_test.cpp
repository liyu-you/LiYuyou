// Regression test: constructing ReaderWidget must not crash.
//
// Crash repro (fixed 2026-10-04): ReaderWidget::setupShelfPage() installs an
// event filter on the quick-import card and then calls setProperty(), which
// synchronously delivers a DynamicPropertyChange event into ReaderWidget::
// eventFilter(). At that moment m_readingView (created later in setupReaderPage)
// was an uninitialized raw pointer, so the `obj == m_readingView->viewport()`
// branch dereferenced garbage -> SIGSEGV in QAbstractScrollArea::viewport().
//
// Building a ReaderWidget exercises that exact event path; the test only needs
// to construct it, spin the event loop a few times, and verify it survives.
#include <QApplication>
#include <QWidget>
#include <QEvent>
#include <cstdio>
#include "readerwidget.h"
#include "booklibrary.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Fresh scratch library root so the real book store is never touched.
    const QString root = QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/scratch_bookshelf");

    // This constructor triggers setupShelfPage() -> quickImportCard->setProperty()
    // -> DynamicPropertyChange -> eventFilter() while m_readingView is still null.
    ReaderWidget *reader = new ReaderWidget(root, nullptr);
    reader->resize(1200, 800);
    reader->show();
    std::printf("shown, processing events\n"); std::fflush(stdout);
    for (int i = 0; i < 8; ++i)
        app.processEvents();
    std::printf("after 8 events\n"); std::fflush(stdout);

    // The widget must still be alive and its library created.
    if (!reader->library()) {
        std::printf("FAIL: ReaderWidget::library() is null\n");
        return 1;
    }

    // Explicitly re-trigger the same dynamic-property event on the timer
    // card to make sure eventFilter() tolerates it after construction too.
    const auto cards = reader->findChildren<QWidget *>();
    bool sawCard = false;
    for (QWidget *w : cards) {
        if (w->property("readerTimerCard").toBool()) {
            sawCard = true;
            w->setProperty("readerTimerCard", true); // DynamicPropertyChange again
            QCoreApplication::sendEvent(w, new QEvent(QEvent::DynamicPropertyChange));
        }
    }
    std::printf("after sendEvent, sawCard=%d\n", (int)sawCard); std::fflush(stdout);
    for (int i = 0; i < 4; ++i)
        app.processEvents();
    std::printf("after 4 more events\n"); std::fflush(stdout);

    if (!sawCard) {
        std::printf("FAIL: timer card (readerTimerCard) not found\n");
        return 1;
    }

    delete reader;
    std::printf("PASS: ReaderWidget constructed and survived event-filter paths\n");
    return 0;
}
