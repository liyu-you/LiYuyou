#ifndef READERWIDGET_H
#define READERWIDGET_H

#include <QWidget>
#include <QVector>
#include <QSet>
#include <QColor>
#include "booklibrary.h"

class QStackedWidget;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QSlider;
class QPushButton;
class QTextEdit;
class QTextDocument;
class QVBoxLayout;
class QHBoxLayout;
class QTimer;
class QMenu;

class ReaderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ReaderWidget(const QString &libraryRoot, QWidget *parent = nullptr);

    BookLibrary *library() const { return m_library; }
    void setCustomColors(const QColor &bg, const QColor &text);
    void setShelfBackground(const QString &imagePath);
    void setBackgroundImage(const QString &path);   // reading page background
    void setPanelsTransparent(bool transparent);
    void openBookById(const QString &id);
    void backToShelfPublic();
    void applyReaderSettings(const QString &themeKey, const QString &fontFamily,
                             int fontSizePx, qreal lineSpacing, const QString &margin);

signals:
    void themeChanged(const QString &themeKey);           // light / dark / eye / parchment
    void backgroundImageChanged(const QString &path);     // empty = cleared
    void customColorsChanged(const QColor &bg, const QColor &text);
    void fontFamilyChanged(const QString &displayName);   // e.g. "宋体" / "微软雅黑"
    void lineSpacingChanged(qreal spacing);
    // Cross-workspace: user right-clicked "存为笔记" on a selection.
    void saveExcerptRequested(const QString &title, const QString &excerpt, const QString &source);

private slots:
    void refreshShelf();
    void openBook(QListWidgetItem *item);
    void backToShelf();
    void prevChapter();
    void nextChapter();
    void showToc();
    void toggleToolbar();
    void changeFontSize(int delta);
    void showThemeMenu();
    void showFontMenu();
    void showSpacingMenu();
    void openSearch();
    void jumpToPercent();
    void pickBackgroundImage();
    void clearBackgroundImage();
    void toggleBookmark();
    void onReadTick();               // 1s reading-time clock while a book is open
    void onSliderPressed();
    void onSliderReleased();
    void importBook();
    void scanBookFolder();
    void onShelfItemMenu(const QPoint &pos);

    // Annotations (highlights + notes)
    void showSelectionMenu(const QPoint &globalPos);
    void showAnnotationMenu(const QString &annId, const QPoint &globalPos);
    void toggleAnnotationPanel();
    void refreshAnnotationList();
    void onAnnotationActivated(QListWidgetItem *item);
    void onAnnotationListMenu(const QPoint &pos);
    void relayoutPage();

    // Reading notes
    void toggleNotePanel();
    void writeNote();
    void refreshNoteList();
    void onNoteActivated(QListWidgetItem *item);
    void onNoteListMenu(const QPoint &pos);
    void updateChrome();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct Para {
        int src = 0;    // first line's source start
        int len = 0;    // source span (first line start .. last line end, gaps included)
        bool heading = false;
        // Reflow of hard-wrapped txt: per-line tables for merged paragraphs
        // (empty for single-line paragraphs, where doc content == source 1:1).
        QVector<int> lineSrc;   // absolute source start of each joined line
        QVector<int> lineLen;   // stripped length of each joined line
    };

    void setupUI();
    void setupShelfPage();
    void setupReadingSidebar();
    void setupReaderPage();
    void loadBook(const QString &id);
    void buildWholeDocument();
    void applyDocumentFormats(QTextDocument *doc, const QFont &base) const;
    void applyTheme();
    void buildDocument(QTextDocument *doc, int begin, int end) const;
    void continueReadingLatest();
    void changeBookCover(const QString &bookId);

    QColor highlightColor(const QString &key) const;
    QString themeTextColor() const;
    QString currentThemeKey() const;
    QString currentMarginKey() const;
    void clearTextSelection();
    QFont readerFont() const;
    QString marginFamilyToFont(const QString &displayName) const;
    QString ctrlButtonStyle() const;      // pill style for bottom-bar buttons
    QString barTextColor() const;         // theme-dependent bottom-bar text color
    QIcon noteIcon() const;               // feather icon tinted to the theme
    QMenu *buildColorMenu(QWidget *parent);
    Annotation annotationById(const QString &annId) const;

    int topCharPos() const;
    void restorePosition(int charPos);
    void scrollPage(int dir);
    void syncDocWidth();            // pin document wrap width to the actual viewport
    void jumpToPara(int idx);       // scroll to paragraph idx + temporary highlight
    QString findChapterCached(int pos) const;

    // Source text <-> rendered document position mapping
    int docFromSource(int srcPos) const;
    int sourceFromDoc(int docPos) const;
    bool selectionSourceRange(int &start, int &end) const;
    QString annotationAtPoint(const QPoint &viewPos) const;
    void jumpToAnnotation(const QString &annId);

    QPixmap generateCover(const BookInfo &book) const;

    enum Theme { Light, Dark, Sepia, Green, Custom };
    enum Page { ShelfPage, ReaderPage };

    BookLibrary *m_library = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Shelf
    QWidget *m_shelfPage = nullptr;
    QHBoxLayout *m_shelfLayout = nullptr;
    QWidget *m_readerSidebar = nullptr;
    QLabel *m_sidebarBookCount = nullptr;
    QLabel *m_sidebarRecentImportTitle = nullptr;
    QLabel *m_sidebarRecentImportTime = nullptr;
    QLabel *m_sidebarNoteCount = nullptr;
    QPushButton *m_sidebarNewGroupBtn = nullptr;
    QWidget *m_shelfContentArea = nullptr;
    QListWidget *m_shelfGrid = nullptr;
    QPushButton *m_importBtn = nullptr;
    QLabel *m_shelfTitle = nullptr;
    QLabel *m_shelfCountLabel = nullptr;
    QLabel *m_recentBookLabel = nullptr;
    QLabel *m_recordBadge = nullptr;
    QLabel *m_recordBookLabel = nullptr;
    QLabel *m_recordChapterLabel = nullptr;
    QLabel *m_timerTodayLabel = nullptr;
    QLabel *m_timerTotalLabel = nullptr;
    QString m_lastReadBookId;
    // Shelf caches: avoid re-reading every book's full text + regenerating covers
    // on every refreshShelf() call. Invalidate on library save / book open.
    QHash<QString, QString> m_chapterCache;   // bookId -> chapter title
    QHash<QString, QIcon> m_coverCache;       // bookId -> cover icon

    // Reader
    QWidget *m_readerPage = nullptr;
    QVBoxLayout *m_readerLayout = nullptr;
    QWidget *m_topBar = nullptr;
    QWidget *m_bottomBar = nullptr;
    QLabel *m_bookTitleLabel = nullptr;
    QLabel *m_chapterLabel = nullptr;
    QPushButton *m_pageLabel = nullptr;   // clickable progress percent (jump-to dialog)
    QTextEdit *m_readingView = nullptr;
    QVBoxLayout *m_textLayout = nullptr;
    QWidget *m_textCard = nullptr;   // translucent paper card over a custom bg image
    QWidget *m_annoPanel = nullptr;
    QListWidget *m_annoList = nullptr;
    QLabel *m_annoHeader = nullptr;
    QWidget *m_notePanel = nullptr;
    QListWidget *m_noteList = nullptr;
    QLabel *m_noteHeader = nullptr;
    QPushButton *m_writeNoteBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QSlider *m_progressSlider = nullptr;
    QPushButton *m_fontBtn = nullptr;
    QPushButton *m_themeBtn = nullptr;
    QPushButton *m_bookmarkBtn = nullptr;
    QPushButton *m_tocBtn = nullptr;
    QPushButton *m_annoBtn = nullptr;
    QPushButton *m_noteBtn = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_familyBtn = nullptr;   // 字体 (宋体/黑体/楷体/仿宋/微软雅黑)
    QPushButton *m_spacingBtn = nullptr;  // 行距 (1.2 ~ 2.5)
    QPushButton *m_searchBtn = nullptr;   // 全文搜索

    // State
    QString m_currentBookId;
    QString m_bookText;
    QList<ChapterInfo> m_chapters;
    int m_fontPx = 22;
    qreal m_lineSpacing = 1.5;
    QString m_fontFamily;
    int m_sideMargin = 56;
    Theme m_theme = Sepia;
    QColor m_customBg = QColor(255, 250, 240);
    QColor m_customText = QColor(60, 50, 40);
    bool m_toolbarVisible = true;
    bool m_sliderPressed = false;

    // Whole-book block mapping (built by buildWholeDocument)
    QVector<int> m_paraSrcStart;   // source char pos where the shown content begins
    QVector<int> m_docBlockStart;  // document pos where the block begins
    QVector<int> m_blockPrefix;    // indent chars added (0 or 2)
    QVector<int> m_blockSrcLen;    // shown content length in source chars

    // Whole-book paragraph index (built by loadBook)
    QVector<Para> m_paras;

    QTimer *m_relayoutTimer = nullptr;

    // Reading-time accounting: a 1s clock runs only while a book is open on the
    // reader page; seconds are flushed to the library every ~15s and on exit.
    QTimer *m_readClock = nullptr;
    int m_readFlushCount = 0;

    // Pomodoro focus timer (25 min)
    QPushButton *m_pomodoroBtn = nullptr;
    QTimer *m_pomodoroTimer = nullptr;
    int m_pomodoroRemaining = 25 * 60;

    int m_pendingRestorePos = 0;   // position to restore after a book opens
    int m_lastTopPos = 0;          // top char captured on resize (cheap re-scroll)

    QPoint m_pressPos;             // for click-vs-drag detection

    QString m_libraryRoot;
    QString m_shelfBgPath;
    QString m_bgPath;
    bool m_panelsTransparent = true;
};

#endif
