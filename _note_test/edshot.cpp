// Editor-page screenshot probe: opens a note, applies representative
// formatting, and grabs the editor page for visual inspection.
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QTextEdit>
#include <QDir>
#include <QMetaObject>
#include <QTextCursor>
#include <QTextTable>

#include "notewidget.h"

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString("edshot.png");
    const QString root = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                  : QApplication::applicationDirPath() + "/scratch_shot";

    QDir().mkpath(root);
    NoteWidget w(root);
    w.resize(1280, 860);
    w.show();
    pump(400);

    QMetaObject::invokeMethod(&w, "newNote");
    QTextEdit *ed = w.editor();
    ed->setPlainText("项目计划标题\n普通段落文字，用于观察行距与首行缩进。\n买牛奶\n写周报\n收尾");

    // heading
    {
        QTextCursor c(ed->document()->firstBlock());
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        QMetaObject::invokeMethod(&w, "textStyle", Q_ARG(int, 1));
    }
    // task list on the two middle lines
    {
        QTextCursor c(ed->document()->findBlockByNumber(2));
        c.setPosition(ed->document()->findBlockByNumber(3).position(),
                      QTextCursor::KeepAnchor);              // keep blk2 anchor!
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        QMetaObject::invokeMethod(&w, "insertTaskList");
        QMetaObject::invokeMethod(&w, "toggleTaskMarker", Q_ARG(int, 2));
    }
    // a small table at the end
    {
        QTextCursor c = ed->textCursor();
        c.movePosition(QTextCursor::End);
        ed->setTextCursor(c);
        QMetaObject::invokeMethod(&w, "insertTableWithParams", Q_ARG(int, 2), Q_ARG(int, 3), Q_ARG(bool, true));
        if (QTextTable *t = ed->textCursor().currentTable()) {
            t->cellAt(0, 0).firstCursorPosition().insertText("阶段");
            t->cellAt(0, 1).firstCursorPosition().insertText("负责人");
            t->cellAt(0, 2).firstCursorPosition().insertText("状态");
            t->cellAt(1, 0).firstCursorPosition().insertText("设计");
            t->cellAt(1, 1).firstCursorPosition().insertText("小李");
            t->cellAt(1, 2).firstCursorPosition().insertText("进行中");
        }
    }

    pump(100);
    const QPixmap pm = w.grab();
    pm.save(out, "PNG");
    printf("saved %s (%dx%d)\n", out.toUtf8().constData(), pm.width(), pm.height());
    return 0;
}
