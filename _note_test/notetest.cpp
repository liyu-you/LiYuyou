// NoteWidget rich-text regression tests (offscreen).
// Covers the Word-style additions: character formats (super/sub, highlight,
// inline code, format painter), paragraph formats (justify, indent, line
// spacing, first-line indent, headings 1-6), task lists, tables, find &
// replace, PDF export, atomic notes.json persistence and Markdown export.
#include <QApplication>
#include <QTimer>
#include <QEventLoop>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextTable>
#include <QTextDocument>
#include <QTextEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QAction>
#include <QCheckBox>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>
#include <QTemporaryDir>
#include <cstdio>
#include <cstdlib>

#include "notewidget.h"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond) do { \
    if (cond) { ++g_passed; printf("PASS: %s\n", #cond); } \
    else { ++g_failed; printf("FAIL: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); } \
} while (0)

// Invoke a private slot by name; returns false when the method is missing.
template <typename... Args>
static bool invoke(NoteWidget *w, const char *slot, Args... args)
{
    return QMetaObject::invokeMethod(w, slot, Q_ARG(Args, args)...);
}

static void pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

static QLineEdit *findEdit(NoteWidget *w) { return w->findChild<QLineEdit *>("noteFindEdit"); }
static QLineEdit *replaceEdit(NoteWidget *w) { return w->findChild<QLineEdit *>("noteReplaceEdit"); }
static QListWidget *outline(NoteWidget *w) { return w->findChild<QListWidget *>("noteOutline"); }

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // QTemporaryDir can fail in stripped-down CI environments; fall back to
    // a scratch dir next to the test binary.
    QString scratch;
    {
        QTemporaryDir tmp;
        if (tmp.isValid())
            scratch = tmp.path();
        else
            scratch = QCoreApplication::applicationDirPath() + "/scratch_test";
    }
    const QString root = scratch + "/notes";
    QDir(scratch).removeRecursively();
    QDir().mkpath(root);
    CHECK(QDir(root).exists());

    NoteWidget w(root);
    w.resize(1200, 800);
    w.show();
    pump(400); // deferred loadNotes()

    // ---- newNote (public slot) opens the editor page ----
    invoke(&w, "newNote");
    QTextEdit *ed = w.editor();
    CHECK(ed != nullptr);

    // ---- Headings 1-6 + outline sync ----
    {
        ed->setPlainText("H1\nplain\nH6");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);
        c.setPosition(2, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        invoke(&w, "textStyle", 1);
        CHECK(ed->document()->firstBlock().blockFormat().headingLevel() == 1);

        c = ed->textCursor();
        const int h6pos = ed->toPlainText().indexOf("H6");
        c.setPosition(h6pos);
        c.setPosition(h6pos + 2, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        invoke(&w, "textStyle", 6);
        const QTextBlock h6blk = ed->document()->findBlock(h6pos);
        CHECK(h6blk.blockFormat().headingLevel() == 6);

        pump(20); // refreshOutline runs on textChanged
        QListWidget *ol = outline(&w);
        CHECK(ol != nullptr);
        CHECK(ol && ol->count() == 2);
        CHECK(ol && ol->item(0) && ol->item(0)->text().contains("H1"));
        CHECK(ol && ol->item(1) && ol->item(1)->text().contains("H6"));
    }

    // ---- Superscript / subscript (checkable actions: setChecked first) ----
    {
        QAction *sup = nullptr, *sub = nullptr;
        for (QAction *a : w.findChildren<QAction *>()) {
            if (a->text() == "x²") sup = a;
            if (a->text() == "x₂") sub = a;
        }
        CHECK(sup != nullptr && sub != nullptr);

        ed->setPlainText("x2");
        QTextCursor c = ed->textCursor();
        c.setPosition(1);
        c.setPosition(2, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        sup->setChecked(true);
        invoke(&w, "textSuperScript");
        QTextCursor probe = ed->textCursor();
        probe.setPosition(2); // char before pos 2 is "2"
        CHECK(probe.charFormat().verticalAlignment() == QTextCharFormat::AlignSuperScript);

        c.setPosition(1);
        c.setPosition(2, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        sup->setChecked(false);
        sub->setChecked(true);
        invoke(&w, "textSubScript");
        probe.setPosition(2);
        CHECK(probe.charFormat().verticalAlignment() == QTextCharFormat::AlignSubScript);
        CHECK(probe.charFormat().verticalAlignment() != QTextCharFormat::AlignSuperScript);
    }

    // ---- Highlight pen ----
    {
        ed->setPlainText("highlight me");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);
        c.setPosition(9, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        invoke(&w, "applyHighlightColor", QColor(0, 255, 0));
        QTextCursor probe = ed->textCursor();
        probe.setPosition(8);
        CHECK(probe.charFormat().background().color() == QColor(0, 255, 0));

        ed->setTextCursor(c);
        invoke(&w, "clearHighlight");
        probe.setPosition(8);
        const QBrush bg = probe.charFormat().background();
        CHECK(bg.style() == Qt::NoBrush || bg.color().alpha() == 0);
    }

    // ---- Inline code ----
    {
        ed->setPlainText("some code here");
        QTextCursor c = ed->textCursor();
        c.setPosition(5);
        c.setPosition(9, QTextCursor::KeepAnchor);
        ed->setTextCursor(c);
        invoke(&w, "insertInlineCode");
        QTextCursor probe = ed->textCursor();
        probe.setPosition(9);
        const QTextCharFormat fmt = probe.charFormat();
        CHECK(fmt.fontFamilies().toStringList().contains("Consolas"));
        CHECK(fmt.background().color() == QColor(238, 238, 238));
    }

    // ---- Format painter: copy bold from A onto B ----
    {
        ed->setPlainText("bold source\ntarget text");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);
        c.setPosition(4, QTextCursor::KeepAnchor); // "bold"
        QTextCharFormat bf;
        bf.setFontWeight(QFont::Bold);
        c.mergeCharFormat(bf);
        ed->setTextCursor(c);

        QAction *painter = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == "格式刷") painter = a;
        CHECK(painter != nullptr);
        painter->setChecked(true);
        invoke(&w, "toggleFormatPainter");

        c.setPosition(12);
        c.setPosition(18, QTextCursor::KeepAnchor); // "target"
        ed->setTextCursor(c);
        invoke(&w, "applyFormatPainter");
        QTextCursor probe = ed->textCursor();
        probe.setPosition(13);
        CHECK(probe.charFormat().fontWeight() == QFont::Bold);
    }

    // ---- Justify / indent / line spacing / first-line indent ----
    {
        ed->setPlainText("para one");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);
        ed->setTextCursor(c);

        QAction *justify = w.findChild<QAction *>(QString(), Qt::FindChildrenRecursively);
        // locate the justify action by text
        QAction *just = nullptr;
        const QList<QAction *> acts = w.findChildren<QAction *>();
        for (QAction *a : acts)
            if (a->text() == "两端对齐") just = a;
        CHECK(just != nullptr);
        if (just) {
            invoke(&w, "textAlign", just);
            CHECK(ed->textCursor().blockFormat().alignment() & Qt::AlignJustify);
        }

        invoke(&w, "indentIncrease");
        CHECK(ed->textCursor().blockFormat().leftMargin() == 36.0);
        invoke(&w, "indentDecrease");
        CHECK(ed->textCursor().blockFormat().leftMargin() == 0.0);

        invoke(&w, "lineSpacingChanged", 2); // 1.5x
        CHECK(ed->textCursor().blockFormat().lineHeightType() == QTextBlockFormat::ProportionalHeight);
        CHECK(ed->textCursor().blockFormat().lineHeight() == 150.0);

        QAction *fli = nullptr;
        for (QAction *a : w.findChildren<QAction *>())
            if (a->text() == "首行缩进") fli = a;
        CHECK(fli != nullptr);
        if (fli) {
            fli->setChecked(true);
            invoke(&w, "toggleFirstLineIndent");
            CHECK(ed->textCursor().blockFormat().textIndent() > 0.0);
            fli->setChecked(false);
            invoke(&w, "toggleFirstLineIndent");
            CHECK(ed->textCursor().blockFormat().textIndent() == 0.0);
        }
    }

    // ---- Task list (☐/☑ markers survive the HTML round trip) ----
    {
        ed->setPlainText("buy milk");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);
        ed->setTextCursor(c);
        invoke(&w, "insertTaskList");
        const QTextBlock blk = ed->document()->firstBlock();
        CHECK(blk.text().startsWith(QChar(0x2610)));

        invoke(&w, "toggleTaskMarker", blk.blockNumber());
        CHECK(ed->document()->firstBlock().text().startsWith(QChar(0x2611)));

        // HTML round trip keeps the marker
        const QString html = ed->toHtml();
        ed->setHtml(html);
        CHECK(ed->document()->firstBlock().text().startsWith(QChar(0x2611)));
    }

    // ---- Task list over a multi-block selection marks every block ----
    {
        ed->setPlainText("one\ntwo\nthree");
        QTextCursor c = ed->textCursor();
        c.setPosition(0);                                            // blk0 start
        c.setPosition(ed->document()->findBlockByNumber(1).position(),
                      QTextCursor::KeepAnchor);                      // keep blk0 anchor!
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor); // blk1 end
        ed->setTextCursor(c);
        invoke(&w, "insertTaskList");
        CHECK(ed->document()->firstBlock().text().startsWith(QChar(0x2610)));
        CHECK(ed->document()->findBlockByNumber(1).text().startsWith(QChar(0x2610)));
        CHECK(!ed->document()->findBlockByNumber(2).text().startsWith(QChar(0x2610)));
    }

    // ---- Tables ----
    {
        ed->clear();
        QTextCursor c = ed->textCursor();
        ed->setTextCursor(c);
        invoke(&w, "insertTableWithParams", 3, 4, true);
        QTextTable *t = ed->textCursor().currentTable();
        CHECK(t != nullptr);
        if (t) {
            CHECK(t->rows() == 3 && t->columns() == 4);
            CHECK(t->format().headerRowCount() == 1);
            // insert-time header styling
            CHECK(t->cellAt(0, 0).firstCursorPosition().blockFormat()
                      .background().color() == QColor(0xF3, 0xF4, 0xF6));

            invoke(&w, "tableInsertRowBelow");
            CHECK(t->rows() == 4);
            invoke(&w, "tableInsertColumnRight");
            CHECK(t->columns() == 5);
            ed->setTextCursor(t->cellAt(2, 1).firstCursorPosition());
            invoke(&w, "tableDeleteRow");
            CHECK(t->rows() == 3);
            ed->setTextCursor(t->cellAt(1, 1).firstCursorPosition());
            invoke(&w, "tableDeleteColumn");
            CHECK(t->columns() == 4);

            // header shading toggle: on applies bg+bold to content, off reverts
            t->cellAt(0, 0).firstCursorPosition().insertText("H");
            ed->setTextCursor(t->cellAt(1, 0).firstCursorPosition()); // inside table
            auto hdrWeight = [&]() {
                QTextCursor p(ed->document());
                p.setPosition(t->cellAt(0, 0).firstPosition() + 1);
                return int(p.charFormat().fontWeight());
            };
            invoke(&w, "tableToggleHeaderShading");   // on
            CHECK(hdrWeight() == QFont::Bold);
            invoke(&w, "tableToggleHeaderShading");   // off
            CHECK(hdrWeight() != QFont::Bold);
            invoke(&w, "tableToggleHeaderShading");   // on again
            CHECK(hdrWeight() == QFont::Bold);

            // merge two cells of the first row
            QTextCursor sel = ed->textCursor();
            sel.setPosition(t->cellAt(0, 0).firstPosition());
            sel.setPosition(t->cellAt(0, 1).lastPosition(), QTextCursor::KeepAnchor);
            ed->setTextCursor(sel);
            invoke(&w, "tableMergeCells");
            CHECK(t->cellAt(0, 0).columnSpan() == 2);
        }
    }

    // ---- Find & replace ----
    {
        ed->clear();
        ed->setPlainText("cat cat cat");

        // Ctrl+F is intercepted inside the editor (ShortcutOverride) so it
        // opens the find bar instead of triggering the canvas fit action.
        QKeyEvent ov(QEvent::ShortcutOverride, Qt::Key_F, Qt::ControlModifier);
        QApplication::sendEvent(ed, &ov);
        CHECK(ov.isAccepted());
        QKeyEvent kp(QEvent::KeyPress, Qt::Key_F, Qt::ControlModifier);
        QApplication::sendEvent(ed, &kp);
        QWidget *fb = w.findChild<QWidget *>("noteFindBar");
        CHECK(fb != nullptr && fb->isVisible());
        invoke(&w, "closeFindBar");

        invoke(&w, "openFindBar", false);
        QLineEdit *fe = findEdit(&w);
        QLineEdit *re = replaceEdit(&w);
        CHECK(fe != nullptr && re != nullptr);
        if (fe && re) {
            fe->setText("cat");
            pump(10);
            QLabel *count = nullptr;
            for (QLabel *l : w.findChildren<QLabel *>())
                if (QRegularExpression("\\d+/\\d+").match(l->text()).hasMatch()) count = l;
            CHECK(count != nullptr && count->text() == "0/3");

            invoke(&w, "findNext");
            CHECK(count && count->text() == "1/3");

            re->setText("dog");
            invoke(&w, "findReplaceAll");
            pump(10);
            CHECK(ed->toPlainText() == "dog dog dog");
            CHECK(count && count->text() == "0/0");
        }

        // case sensitivity
        ed->setPlainText("Cat cat");
        fe->setText("cat");
        pump(10);
        QLabel *count = nullptr;
        for (QLabel *l : w.findChildren<QLabel *>())
            if (QRegularExpression("\\d+/\\d+").match(l->text()).hasMatch()) count = l;
        CHECK(count && count->text() == "0/2");
        QCheckBox *caseCheck = w.findChild<QCheckBox *>();
        for (QCheckBox *cb : w.findChildren<QCheckBox *>())
            if (cb->text().contains("大小写")) caseCheck = cb;
        CHECK(caseCheck != nullptr);
        if (caseCheck) {
            caseCheck->setChecked(true);
            pump(10);
            CHECK(count && count->text() == "0/1");
        }
    }

    // ---- Markdown export: headings, task lists, tables, wiki links ----
    {
        ed->clear();
        // two plain lines first, then style them (avoids list/heading mixups)
        ed->setPlainText("Second");
        QTextCursor endC = ed->textCursor();
        endC.movePosition(QTextCursor::End);
        endC.insertBlock();
        endC.insertText("todo keep");

        QTextCursor c2(ed->document()->firstBlock());
        c2.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        ed->setTextCursor(c2);
        invoke(&w, "textStyle", 1);   // "Second" → H1

        const QTextBlock blk1 = ed->document()->findBlockByNumber(1);
        QTextCursor c3(blk1);
        c3.movePosition(QTextCursor::EndOfBlock);
        ed->setTextCursor(c3);
        invoke(&w, "insertTaskList");        // "☐ todo keep"
        invoke(&w, "toggleTaskMarker", 1);   // "☑ todo keep"

        const QString md = w.htmlToMarkdown(ed->toHtml());
        CHECK(md.contains("- [x] todo keep"));
        CHECK(md.contains("# Second"));
    }
    {
        // table markdown
        ed->clear();
        invoke(&w, "insertTableWithParams", 2, 2, false);
        QTextTable *t = ed->textCursor().currentTable();
        if (t) {
            t->cellAt(0, 0).firstCursorPosition().insertText("A1");
            t->cellAt(0, 1).firstCursorPosition().insertText("B1");
            t->cellAt(1, 0).firstCursorPosition().insertText("A2");
            t->cellAt(1, 1).firstCursorPosition().insertText("B2");
            const QString md = w.htmlToMarkdown(ed->toHtml());
            CHECK(md.contains("| A1 | B1 |"));
            CHECK(md.contains("| --- | --- |"));
            CHECK(md.contains("| A2 | B2 |"));
        }
    }
    {
        // wiki link markdown + resolver
        ed->clear();
        QTextCursor c = ed->textCursor();
        QTextCharFormat fmt;
        fmt.setAnchor(true);
        fmt.setAnchorHref("wikilink://Project Ideas");
        c.insertText("[[Project Ideas]]", fmt);
        const QString md = w.htmlToMarkdown(ed->toHtml());
        CHECK(md.contains("[[Project Ideas]]"));

        invoke(&w, "openOrCreateByTitle", QString("Project Ideas"));
        CHECK(w.title() == "Project Ideas");
    }

    // ---- PDF export ----
    {
        ed->clear();
        ed->setPlainText("PDF export smoke test");
        const QString pdfPath = scratch + "/out.pdf";
        QFile::remove(pdfPath);
        invoke(&w, "exportPdfTo", pdfPath);
        QFile pdf(pdfPath);
        CHECK(pdf.exists() && pdf.open(QIODevice::ReadOnly));
        if (pdf.isOpen()) {
            const QByteArray head = pdf.read(4);
            CHECK(head == QByteArray("%PDF"));
            pdf.close();
        }
    }

    // ---- Sidebar toggle (Ctrl+8 folds/unfolds the content browser) ----
    {
        QWidget *sb = w.findChild<QWidget *>("noteSidebar");
        CHECK(sb != nullptr);
        if (sb) {
            const bool wasVisible = sb->isVisible();
            invoke(&w, "toggleSidebar");
            CHECK(sb->isVisible() != wasVisible);
            invoke(&w, "toggleSidebar");
            CHECK(sb->isVisible() == wasVisible);
            // width restored to the pre-fold value
            if (wasVisible) CHECK(sb->width() >= 200);
        }
    }

    // ---- Persistence: destructor saves; a fresh instance loads; JSON valid ----
    {
        invoke(&w, "newNote");
        ed->setPlainText("persist me");
        invoke(&w, "textStyle", 2);
    }
    { // destroy -> flush
        NoteWidget *w2 = new NoteWidget(root);
        w2->show();
        pump(400);
        invoke(w2, "newNote");
        w2->editor()->setPlainText("round trip body");
        const QString firstId = w2->allNotes().isEmpty() ? QString() : w2->allNotes().first().id;
        delete w2; // destructor: saveCurrentNote + saveNotes

        QFile f(root + "/notes.json");
        CHECK(f.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        CHECK(doc.isObject());
        CHECK(doc.object()["notes"].toArray().size() >= 1);

        NoteWidget w3(root);
        pump(400);
        bool found = false;
        for (const NoteData &n : w3.allNotes())
            if (n.content.contains("round trip body")) found = true;
        CHECK(found);
        Q_UNUSED(firstId);
    }

    printf("\n=== %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
