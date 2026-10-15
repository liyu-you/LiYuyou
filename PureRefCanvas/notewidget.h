#ifndef NOTEWIDGET_H
#define NOTEWIDGET_H

#include <QWidget>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QList>
#include <QDateTime>
#include <QColor>
#include <QVector>
#include <QPair>
#include <functional>

class QTextEdit;
class QToolBar;
class QAction;
class QFontComboBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QScrollArea;
class QVBoxLayout;
class QGridLayout;
class QToolButton;
class QCheckBox;
class QTextTable;
class QTextBlock;
class QKeyEvent;

struct NoteData {
    QString id;
    QString title;
    QString content; // HTML
    QDateTime createdAt;
    QDateTime updatedAt;
    bool starred = false;
    QString folder;
    QString spaceId; // empty = belongs to "全部" (every space)
};

class NoteWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NoteWidget(const QString &libraryRoot, QWidget *parent = nullptr);
    ~NoteWidget();

    QString toHtml() const;
    void setHtml(const QString &html);
    QString toPlainText() const;
    QString title() const;
    bool isModified() const { return m_modified; }
    void setModified(bool modified);
    QTextEdit *editor() const { return m_editor; }
    void setBgColor(const QColor &c);
    void setBackgroundImage(const QString &path);
    void setPanelsTransparent(bool transparent);
    void setEditorBackground(const QString &path);
    void changeEditorBackground();
    void clearEditorBackground();
    void setSpaceManager(class SpaceManager *mgr);
    void setNoteSpace(const QString &noteId, const QString &spaceId);
    void setImmersiveMode(bool immersive);
    QString currentNoteId() const { return m_currentNoteId; }
    void backToGrid();
    void hideSidebar();
    QList<NoteData> allNotes() const { return m_notes; }
    void openNote(const QString &noteId);
    QString htmlToMarkdown(const QString &html) const;

private:
    QPixmap m_bgPixmap;
    QString m_bgPath;
    QString m_editorBgPath;
    bool m_panelsTransparent = true;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void modificationChanged(bool modified);

public slots:
    void newNote();
    void journalToday();  // Logseq-style: open/create today's daily note
    void openFindBar(bool withReplace = false);  // Word-style Ctrl+F / Ctrl+H
    void openOrCreateByTitle(const QString &title);  // [[wiki-link]] resolver
    // Silently create a note with given HTML content (for weekly report / flashcards).
    // Does NOT switch to it; caller decides whether to open it. Returns new note id.
    QString createNoteWithContent(const QString &title, const QString &htmlContent,
                                  const QString &folder = QStringLiteral("日记"));
    void toggleSidebar();  // 折叠/展开左侧内容浏览器（Ctrl+8）
    // Create a new note pre-filled with an excerpt from another workspace
    // (e.g. a reader selection). title is the note title, excerpt is the
    // plain text quote, source is a footer line like "——《书》· 章节".
    void newNoteWithExcerpt(const QString &title, const QString &excerpt, const QString &source);

private slots:
    void onTextChanged();
    void onCursorPositionChanged();
    void mergeFormatOnSelection(const QTextCharFormat &format);

    void textBold();
    void textItalic();
    void textUnderline();
    void textStrike();
    void textColor();
    void textAlign(QAction *action);
    void textStyle(int index);
    void fontChanged(const QFont &font);
    void fontSizeChanged(int index);
    void insertImage();
    void insertLink();
    void insertCodeBlock();
    void insertQuote();
    void insertList(QAction *action);
    void insertHR();
    void clearFormat();

    // Word-style rich text: character level
    void textSuperScript();
    void textSubScript();
    void applyHighlightColor(const QColor &color);
    void clearHighlight();
    void insertInlineCode();
    void toggleFormatPainter();

    // Word-style rich text: paragraph level
    void indentIncrease();
    void indentDecrease();
    void lineSpacingChanged(int index);
    void toggleFirstLineIndent();

    // Task list (Logseq/Word-style checklist, ☐/☑ text markers)
    void insertTaskList();
    void toggleTaskMarker(int blockNumber);

    // Tables
    void insertTableDialog();
    void insertTableWithParams(int rows, int cols, bool headerRow);
    void tableInsertRowAbove();
    void tableInsertRowBelow();
    void tableInsertColumnLeft();
    void tableInsertColumnRight();
    void tableDeleteRow();
    void tableDeleteColumn();
    void tableMergeCells();
    void tableSplitCell();
    void tableToggleHeaderShading();
    void showEditorContextMenu(const QPoint &pos);

    // Insert & export
    void insertDateTime(int format);
    void exportPdfDialog();
    bool exportPdfTo(const QString &path);

    // Find & replace
    void closeFindBar();
    void findNext();
    void findPrev();
    void findReplaceOne();
    void findReplaceAll();
    void onFindQueryChanged();

    void applyFormatPainter();  // invoked on selection while painter is armed

    // Note management
    void deleteNote();
    void onTitleChanged();
    void onSearchChanged(const QString &text);
    void onCategoryChanged(QListWidgetItem *item);
    void toggleStar();

    // Folder management (自定义分组，语雀/印象笔记标准)
    void onFolderMenu(const QPoint &pos);
    void newFolder();
    void renameFolder();
    void deleteFolder();

    // Export & copy (微信读书/Notion/印象笔记标准)
    void exportCurrentNote();
    void exportAllNotes();
    void copyCurrentMarkdown();
    void copyAllMarkdown();

    // Sorting (按更新时间/创建时间/标题/收藏优先)
    void onSortChanged(int index);

private:
    void setupUI();
    void setupSidebar();
    void setupCardGrid();
    void setupEditor();
    void setupToolbar();
    void setupFindBar();
    void loadNotes();
    void loadNotesFromArray(const QJsonArray &arr);
    void saveNotes();
    void saveCurrentNote();
    void refreshCards();
    void showCardContextMenu(const QString &noteId, const QPoint &globalPos);
    void refreshFolderList();
    void updateStatusBar();
    void refreshOutline();  // VNote: extract headings from current note
    void refreshBacklinks(); // list notes whose [[...]] points to current note
    void showWikiCompleter(); // popup [[...]] autocomplete near cursor
    void hideWikiCompleter();
    QString generateId() const;
    QString htmlToPreview(const QString &html, int maxLen = 120) const;
    QString noteToMarkdown(const NoteData &note) const;
    QString blockToMarkdown(const QTextBlock &block) const;
    QString tableToMarkdown(QTextTable *table) const;
    QWidget *createCard(const NoteData &note);
    void insertWikiLinkDialog();

    // Helpers for the Word-style extensions
    void applyBlockFormatOnSelection(const std::function<void(QTextBlockFormat &)> &mutator);
    QTextTable *currentTable() const;
    void styleTableRow(QTextTable *table, int row, bool headerStyle);
    bool handleEditorMousePress(const QPointF &viewportPos);
    bool handleEditorKeyPress(QKeyEvent *event);
    void deactivateFormatPainter();
    void recomputeFindMatches();
    void updateFindHighlights();
    static bool isTaskMarkerChar(QChar c);
    static QChar taskMarkerChar(bool checked);

    QColor m_currentColor;

    // Layout
    QWidget *m_sidebar;
    QLineEdit *m_searchEdit;
    QPushButton *m_newNoteBtn;
    QListWidget *m_categoryList;
    QListWidget *m_folderList;
    QComboBox *m_sortCombo;      // 排序方式
    QPushButton *m_exportAllBtn; // 导出全部笔记

    QStackedWidget *m_stack;
    QScrollArea *m_gridScroll;
    QWidget *m_gridWidget;
    QGridLayout *m_gridLayout;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_totalCountLabel = nullptr;
    QLabel *m_starredCountLabel = nullptr;

    QWidget *m_editorPage;
    QWidget *m_editorTopBar;
    QPushButton *m_backBtn;
    QLineEdit *m_titleEdit;
    QPushButton *m_starBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_exportBtn;   // 导出当前笔记（Markdown / PDF 菜单）
    QPushButton *m_copyBtn;     // 复制当前笔记为 Markdown
    QToolBar *m_toolbar;        // 工具栏第 1 行：字符格式
    QToolBar *m_toolbar2;       // 工具栏第 2 行：段落/插入
    QTextEdit *m_editor;
    QListWidget *m_outlineList = nullptr;  // VNote-style outline
    QListWidget *m_backlinksList = nullptr; // notes that link to current note
    QListWidget *m_wikiPopup = nullptr;    // [[...]] autocompleter popup
    QString m_wikiPrefix;                   // partial text after [[ being typed
    QLabel *m_statusLabel;

    // Data
    int m_sidebarWidth = 220;   // 侧栏折叠前宽度（toggleSidebar 恢复用）
    QString m_libraryRoot;
    // False once notes.json was found unreadable and quarantined: saveNotes then
    // refuses to write, so an in-memory empty list can never overwrite the file.
    bool m_notesStoreHealthy = true;
    QList<NoteData> m_notes;
    class SpaceManager *m_spaceMgr = nullptr;
    QStringList m_folders;          // 自定义文件夹（持久化）
    QString m_currentNoteId;
    QString m_currentCategory = "all";
    QString m_sortMode = "updated"; // updated | created | title | starred
    bool m_modified = false;
    bool m_loading = false;

    // Toolbar actions
    QAction *m_actionBold, *m_actionItalic, *m_actionUnderline, *m_actionStrike;
    QAction *m_actionColor, *m_actionClearFormat;
    QAction *m_actionAlignLeft, *m_actionAlignCenter, *m_actionAlignRight, *m_actionAlignJustify;
    QAction *m_actionListBullet, *m_actionListNumber;
    QAction *m_actionQuote, *m_actionCode, *m_actionHR;
    QAction *m_actionImage, *m_actionLink;
    QAction *m_actionUndo, *m_actionRedo;
    QColor m_bgColor = QColor(255, 255, 255);

    QFontComboBox *m_fontCombo;
    QComboBox *m_sizeCombo;
    QComboBox *m_styleCombo;

    // Word-style extensions
    QAction *m_actionSuper = nullptr;        // 上标
    QAction *m_actionSub = nullptr;          // 下标
    QAction *m_actionInlineCode = nullptr;   // 行内代码
    QAction *m_actionFormatPainter = nullptr; // 格式刷
    QToolButton *m_painterBtn = nullptr;     // 格式刷按钮（双击进入连续模式）
    QToolButton *m_highlightBtn = nullptr;   // 高亮笔（菜单选色）
    QColor m_currentHighlight = QColor(0xFF, 0xF3, 0x00);
    QAction *m_actionTaskList = nullptr;     // 任务列表
    QAction *m_actionTable = nullptr;        // 插入表格
    QAction *m_actionIndentInc = nullptr;    // 增加缩进
    QAction *m_actionIndentDec = nullptr;    // 减少缩进
    QAction *m_actionFirstLineIndent = nullptr; // 首行缩进（可勾选）
    QComboBox *m_lineSpacingCombo = nullptr; // 行距

    // Format painter state
    bool m_painterSticky = false;            // 双击后连续刷
    int m_painterSourceAnchor = -1;
    QTextCharFormat m_painterCharFmt;
    QTextBlockFormat m_painterBlockFmt;

    // Find & replace bar
    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_findCaseCheck = nullptr;
    QLabel *m_findCountLabel = nullptr;
    QVector<QPair<int, int>> m_findMatches;  // (position, length)
    int m_findIndex = -1;
};

#endif
