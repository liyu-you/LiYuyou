#ifndef NOTEWIDGET_H
#define NOTEWIDGET_H

#include <QWidget>
#include <QTextCharFormat>
#include <QList>
#include <QDateTime>
#include <QColor>

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

struct NoteData {
    QString id;
    QString title;
    QString content; // HTML
    QDateTime createdAt;
    QDateTime updatedAt;
    bool starred = false;
    QString folder;
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
    void hideSidebar();
    QList<NoteData> allNotes() const { return m_notes; }
    void openNote(const QString &noteId);

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

    // Note management
    void deleteNote();
    void backToGrid();
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
    void loadNotes();
    void loadNotesFromArray(const QJsonArray &arr);
    void saveNotes();
    void saveCurrentNote();
    void refreshCards();
    void refreshFolderList();
    void updateStatusBar();
    QString generateId() const;
    QString htmlToPreview(const QString &html, int maxLen = 120) const;
    QString noteToMarkdown(const NoteData &note) const;
    QString htmlToMarkdown(const QString &html) const;
    QWidget *createCard(const NoteData &note);

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

    QWidget *m_editorPage;
    QWidget *m_editorTopBar;
    QPushButton *m_backBtn;
    QLineEdit *m_titleEdit;
    QPushButton *m_starBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_exportBtn;   // 导出当前笔记
    QPushButton *m_copyBtn;     // 复制当前笔记为 Markdown
    QToolBar *m_toolbar;
    QTextEdit *m_editor;
    QLabel *m_statusLabel;

    // Data
    QString m_libraryRoot;
    QList<NoteData> m_notes;
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
};

#endif
