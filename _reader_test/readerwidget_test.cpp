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
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include "readerwidget.h"
#include "booklibrary.h"

namespace {
// Write a tiny book file into the scratch dir, import it, exercise the
// reading-time accounting, then verify persistence across a reload.
int testReadingTimeAccounting(const QString &root)
{
    QDir().mkpath(root);
    const QString bookPath = root + "/_probe_book.txt";
    QFile f(bookPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::printf("FAIL: cannot write probe book\n"); return 1;
    }
    {
        QTextStream ts(&f);
        ts << "第一章 开端\n\n"
              "这是用于测试阅读时长统计的正文。"
              "重复若干次以凑够一定字数。一二三四五六七八九十。\n\n"
              "第二章 发展\n\n继续正文内容。\n";
    }
    f.close();

    BookLibrary lib(root);
    if (!lib.load()) { std::printf("FAIL: lib.load\n"); return 1; }
    const QString id = lib.importBook(bookPath);
    if (id.isEmpty()) { std::printf("FAIL: importBook returned empty id\n"); return 1; }

    // Accumulate 120 seconds of reading.
    lib.addReadingSeconds(id, 120);
    if (lib.totalTodayReadSec() != 120) {
        std::printf("FAIL: today seconds expected 120, got %lld\n",
                    (long long)lib.totalTodayReadSec()); return 1;
    }
    if (lib.totalAllReadSec() != 120) {
        std::printf("FAIL: all seconds expected 120, got %lld\n",
                    (long long)lib.totalAllReadSec()); return 1;
    }

    // Persist to disk.
    if (!lib.save()) { std::printf("FAIL: lib.save\n"); return 1; }

    // Reload from the same dir: counters must survive.
    BookLibrary lib2(root);
    if (!lib2.load()) { std::printf("FAIL: lib2.load\n"); return 1; }
    if (lib2.totalTodayReadSec() != 120) {
        std::printf("FAIL: after reload today expected 120, got %lld\n",
                    (long long)lib2.totalTodayReadSec()); return 1;
    }
    if (lib2.totalAllReadSec() != 120) {
        std::printf("FAIL: after reload all expected 120, got %lld\n",
                    (long long)lib2.totalAllReadSec()); return 1;
    }

    std::printf("PASS: reading-time accounting (120s persisted & reloaded)\n");
    return 0;
}
} // namespace

int main(int argc, char *argv[])
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app(argc, argv);

    // Fresh scratch library root so the real book store is never touched.
    const QString root = QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/scratch_bookshelf");

    // 1) Reading-time accounting unit checks (no widget needed).
    if (int rc = testReadingTimeAccounting(root)) return rc;

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
