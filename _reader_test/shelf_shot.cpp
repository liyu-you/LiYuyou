// Capture the CURRENT reader shelf page rendering to a PNG so layout issues
// visible in the user's screenshot can be compared 1:1 against real output.
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QThread>
#include <cstdio>
#include "mainwindow.h"
#include "readerwidget.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1282, 1041);
    w.show();
    for (int i = 0; i < 8; ++i) app.processEvents();

    // Switch to the 阅读 (reader) page via the real nav button.
    bool clicked = false;
    for (QPushButton *b : w.findChildren<QPushButton *>()) {
        if (b->text() == QStringLiteral("阅读")) { b->click(); clicked = true; break; }
    }
    printf("nav click: %d\n", clicked); fflush(stdout);
    // Let the deferred library load + refresh run.
    for (int i = 0; i < 60; ++i) { app.processEvents(); QThread::msleep(10); }

    ReaderWidget *reader = w.findChild<ReaderWidget *>();
    if (!reader) { printf("NO READER\n"); fflush(stdout); return 1; }
    printf("reader visible=%d size=%dx%d\n", reader->isVisible(), reader->width(), reader->height()); fflush(stdout);
    QListWidget *grid = reader->findChild<QListWidget *>(QStringLiteral("readerShelfGrid"));
    if (grid) printf("shelf grid rows=%d\n", grid->count()); fflush(stdout);

    const QString out = QStringLiteral("C:/Users/LiyuYou/Desktop/Q/_reader_test/_shelf_now.png");
    if (!reader->grab().save(out))
        printf("SAVE FAILED\n");
    else
        printf("saved %s\n", qPrintable(out));
    fflush(stdout);
    _Exit(0);
}
