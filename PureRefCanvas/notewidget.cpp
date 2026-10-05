#include "notewidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QFontComboBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QFile>
#include <QDir>
#include <QPixmap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextBlockFormat>
#include <QTextList>
#include <QTextListFormat>
#include <QTextFragment>
#include <QFont>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QRegularExpression>
#include <QDateTime>
#include <QResizeEvent>
#include <QUuid>
#include <QResizeEvent>
#include <QSettings>
#include <QSaveFile>
#include <QFileInfo>
#include <QTextTable>
#include <QTextFrame>
#include <QTextTableCell>
#include <QPrinter>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QShortcut>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QLocale>
#include <functional>
#include <algorithm>

namespace {
// Shared dark style for the two toolbar rows (Steam-like palette).
const char kToolbarSS[] =
    "QToolBar { background: #1b2838; border-bottom: 1px solid #2a475e; spacing: 1px; padding: 3px 6px; }"
    "QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 5px; font-size: 13px; }"
    "QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; }"
    "QToolButton:checked { background: #2a475e; color: #66c0f4; }"
    "QComboBox { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 4px; padding: 2px 8px; font-size: 12px; min-width: 60px; }"
    "QComboBox:hover { border-color: #66c0f4; }"
    "QComboBox QAbstractItemView { background: #16202d; color: #c7d5e0; selection-background-color: #2a475e; selection-color: #66c0f4; outline: none; }";
const char kDarkMenuSS[] =
    "QMenu { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; }"
    "QMenu::item { padding: 8px 24px; }"
    "QMenu::item:selected { background: #2a475e; color: #66c0f4; }";
} // namespace

NoteWidget::NoteWidget(const QString &libraryRoot, QWidget *parent)
    : QWidget(parent), m_libraryRoot(libraryRoot)
{
    setupUI();
    // Restore saved editor background image (if any)
    {
        QSettings settings("PureRefCanvas", "里与由");
        QString savedBg = settings.value("note/editorBg", "").toString();
        if (!savedBg.isEmpty() && QFile::exists(savedBg))
            setEditorBackground(savedBg);
        // Restore sort mode and sync the sidebar combo
        m_sortMode = settings.value("note/sortMode", "updated").toString();
        if (m_sortCombo) {
            int idx = m_sortMode == "created" ? 1 : m_sortMode == "title" ? 2
                     : m_sortMode == "starred" ? 3 : 0;
            m_sortCombo->setCurrentIndex(idx);
        }
    }
    // Defer heavy loading — staggered to avoid main window freeze on show
    QTimer::singleShot(150, this, [this]() {
        loadNotes();
        refreshCards();
    });
}

NoteWidget::~NoteWidget()
{
    saveCurrentNote();
    saveNotes();
}

void NoteWidget::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QWidget;
    m_sidebar->setFixedWidth(220);
    m_sidebar->setObjectName("noteSidebar");
    m_sidebar->setStyleSheet("QWidget#noteSidebar { background: #FFFFFF; border-right: 1px solid #E5E7EB; } QLabel { background: transparent; }");
    setupSidebar();
    mainLayout->addWidget(m_sidebar);

    // Right stack: card grid or editor
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("background: transparent;");

    // Card grid page
    m_gridScroll = new QScrollArea;
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setFrameShape(QFrame::NoFrame);
    m_gridScroll->setStyleSheet("QScrollArea { background: #F5F7FA; border: none; } QScrollBar:vertical { background: transparent; width: 10px; } QScrollBar::handle:vertical { background: #D1D5DB; border-radius: 5px; min-height: 32px; margin: 2px; } QScrollBar::handle:vertical:hover { background: #9CA3AF; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    m_gridWidget = new QWidget;
    m_gridWidget->setStyleSheet("background: #F5F7FA;");
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setContentsMargins(24, 22, 24, 22);
    m_gridLayout->setSpacing(14);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_gridScroll->setWidget(m_gridWidget);
    m_stack->addWidget(m_gridScroll);

    // Editor page
    m_editorPage = new QWidget;
    m_editorPage->setObjectName("noteEditorPage");
    m_editorPage->setStyleSheet("QWidget#noteEditorPage { background: #1a1a1a; }");
    QVBoxLayout *editorLayout = new QVBoxLayout(m_editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    // Top bar
    m_editorTopBar = new QWidget;
    m_editorTopBar->setFixedHeight(44);
    m_editorTopBar->setStyleSheet("background: #16202d; border-bottom: 1px solid #2a475e;");
    QHBoxLayout *topLayout = new QHBoxLayout(m_editorTopBar);
    topLayout->setContentsMargins(12, 4, 12, 4);
    topLayout->setSpacing(8);

    m_backBtn = new QPushButton("返回");
    m_backBtn->setFixedHeight(30);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    m_backBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.06); color: #c7d5e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 15px; padding: 2px 14px; font-size: 13px; } QPushButton:hover { background: rgba(42,71,94,0.8); color: #fff; border-color: #66c0f4; }");
    connect(m_backBtn, &QPushButton::clicked, this, &NoteWidget::backToGrid);
    topLayout->addWidget(m_backBtn);

    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText("笔记标题");
    m_titleEdit->setStyleSheet("QLineEdit { background: transparent; color: #c7d5e0; border: none; font-size: 16px; font-weight: 600; padding: 4px 10px; } QLineEdit:focus { background: rgba(42,71,94,0.3); border-radius: 6px; }");
    connect(m_titleEdit, &QLineEdit::textChanged, this, &NoteWidget::onTitleChanged);
    topLayout->addWidget(m_titleEdit, 1);

    m_starBtn = new QPushButton("收藏");
    m_starBtn->setFixedHeight(30);
    m_starBtn->setCursor(Qt::PointingHandCursor);
    m_starBtn->setCheckable(true);
    m_starBtn->setStyleSheet("QPushButton { background: transparent; color: #8f98a0; border: 1px solid transparent; border-radius: 15px; padding: 2px 12px; font-size: 13px; } QPushButton:hover { background: rgba(255,193,7,0.15); color: #ffc107; border-color: rgba(255,193,7,0.4); } QPushButton:checked { background: rgba(255,193,7,0.18); color: #ffc107; border-color: rgba(255,193,7,0.5); }");
    connect(m_starBtn, &QPushButton::clicked, this, &NoteWidget::toggleStar);
    topLayout->addWidget(m_starBtn);

    m_deleteBtn = new QPushButton("删除");
    m_deleteBtn->setFixedHeight(30);
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setStyleSheet("QPushButton { background: transparent; color: #8f98a0; border: 1px solid transparent; border-radius: 15px; padding: 2px 12px; font-size: 13px; } QPushButton:hover { background: rgba(255,85,85,0.18); color: #ff6b6b; border-color: rgba(255,85,85,0.45); }");
    connect(m_deleteBtn, &QPushButton::clicked, this, &NoteWidget::deleteNote);
    topLayout->addWidget(m_deleteBtn);

    m_exportBtn = new QPushButton("导出");
    m_exportBtn->setFixedHeight(30);
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setToolTip("导出当前笔记");
    m_exportBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.06); color: #c7d5e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 15px; padding: 2px 12px; font-size: 13px; } QPushButton:hover { background: rgba(42,71,94,0.8); color: #fff; border-color: #66c0f4; }");
    QMenu *exportMenu = new QMenu(m_exportBtn);
    exportMenu->setStyleSheet(kDarkMenuSS);
    exportMenu->addAction("导出 Markdown…", this, &NoteWidget::exportCurrentNote);
    exportMenu->addAction("导出 PDF…", this, &NoteWidget::exportPdfDialog);
    m_exportBtn->setMenu(exportMenu);
    topLayout->addWidget(m_exportBtn);

    m_copyBtn = new QPushButton("复制");
    m_copyBtn->setFixedHeight(30);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setToolTip("复制当前笔记为 Markdown");
    m_copyBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.06); color: #c7d5e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 15px; padding: 2px 12px; font-size: 13px; } QPushButton:hover { background: rgba(42,71,94,0.8); color: #fff; border-color: #66c0f4; }");
    connect(m_copyBtn, &QPushButton::clicked, this, &NoteWidget::copyCurrentMarkdown);
    topLayout->addWidget(m_copyBtn);

    editorLayout->addWidget(m_editorTopBar);

    // Editor (create before toolbar since toolbar connects to it)
    m_editor = new QTextEdit;
    // Logseq-style [[wiki-links]]: detected via setSource on anchor clicks.
    m_editor->setStyleSheet(R"(
        QTextEdit {
            background-color: rgba(255,255,255,0.95);
            color: #2c2c2c;
            border: none;
            padding: 32px 72px;
            font-size: 14px;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            selection-background-color: #cfe8ff;
        }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; }
        QScrollBar::handle:vertical { background: rgba(42,71,94,0.5); border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #2a475e; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
    m_editor->viewport()->setAutoFillBackground(false);
    m_editor->installEventFilter(this);
    m_editor->viewport()->installEventFilter(this);   // task checkbox / anchor clicks
    m_editor->setContextMenuPolicy(Qt::CustomContextMenu);  // table context menu
    connect(m_editor, &QTextEdit::customContextMenuRequested, this, &NoteWidget::showEditorContextMenu);
    connect(m_editor, &QTextEdit::textChanged, this, &NoteWidget::onTextChanged);
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &NoteWidget::onCursorPositionChanged);

    // Toolbars (two rows: character formatting / paragraph & insert)
    setupToolbar();
    editorLayout->addWidget(m_toolbar);
    editorLayout->addWidget(m_toolbar2);

    // Find & replace bar (hidden until Ctrl+F / Ctrl+H)
    setupFindBar();
    editorLayout->addWidget(m_findBar);
    m_findBar->hide();

    // Editor container with margins for card effect
    QWidget *editorContainer = new QWidget;
    editorContainer->setObjectName("noteEditorContainer");
    editorContainer->setStyleSheet("QWidget#noteEditorContainer { background: transparent; }");
    QHBoxLayout *containerLayout = new QHBoxLayout(editorContainer);
    containerLayout->setContentsMargins(40, 16, 40, 16);
    containerLayout->addWidget(m_editor, 1);

    // VNote-style outline pane (right side of editor).
    m_outlineList = new QListWidget;
    m_outlineList->setObjectName("noteOutline");
    m_outlineList->setMaximumWidth(220);
    m_outlineList->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.9); border:none; border-left:1px solid #eee; font-size:12px; }"
        "QListWidget::item { padding:4px 8px; color:#555; }"
        "QListWidget::item:selected { background:#EAF7F0; color:#2FA66A; }");
    connect(m_outlineList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int block = item->data(Qt::UserRole).toInt();
        QTextCursor cur(m_editor->document()->findBlockByNumber(block));
        m_editor->setTextCursor(cur);
        m_editor->setFocus();
    });
    containerLayout->addWidget(m_outlineList);
    editorLayout->addWidget(editorContainer, 1);

    // Status bar
    m_statusLabel = new QLabel("字数：0  词数：0");
    m_statusLabel->setFixedHeight(28);
    m_statusLabel->setStyleSheet("background: #16202d; color: #8f98a0; font-size: 12px; padding: 0 16px; border-top: 1px solid #2a475e;");
    editorLayout->addWidget(m_statusLabel);

    m_stack->addWidget(m_editorPage);
    mainLayout->addWidget(m_stack, 1);

    m_stack->setCurrentIndex(0); // Show grid by default
}

void NoteWidget::setupSidebar()
{
    QVBoxLayout *sideLayout = new QVBoxLayout(m_sidebar);
    sideLayout->setContentsMargins(16, 20, 16, 20);
    sideLayout->setSpacing(18);

    // Section header
    QLabel *browserLabel = new QLabel("内容浏览器");
    browserLabel->setStyleSheet("font-size:13px; font-weight:500; color:#6B7280;");
    sideLayout->addWidget(browserLabel);

    // Active "笔记" row
    QWidget *activeRow = new QWidget;
    activeRow->setStyleSheet("background:#EAF3DE; border-radius:10px;");
    QHBoxLayout *al = new QHBoxLayout(activeRow);
    al->setContentsMargins(12,8,8,8); al->setSpacing(8);
    QLabel *ai = new QLabel("▮"); ai->setStyleSheet("color:#27500A; font-size:14px;");
    QLabel *at = new QLabel("笔记"); at->setStyleSheet("color:#27500A; font-size:14px; font-weight:500;");
    al->addWidget(ai); al->addWidget(at); al->addStretch();
    QLabel *ch = new QLabel("›"); ch->setStyleSheet("color:#27500A; font-size:16px;");
    al->addWidget(ch);
    sideLayout->addWidget(activeRow);

    // Stats row: total notes + starred
    QHBoxLayout *statsRow = new QHBoxLayout; statsRow->setSpacing(12);
    {
        QWidget *s1 = new QWidget; s1->setStyleSheet("background:#F3F4F6; border-radius:10px;");
        QVBoxLayout *s1l = new QVBoxLayout(s1); s1l->setContentsMargins(12,12,12,12);
        QLabel *l1 = new QLabel("0 篇");
        l1->setAlignment(Qt::AlignCenter);
        l1->setStyleSheet("font-size:14px; font-weight:600; color:#111827;");
        s1l->addWidget(l1); statsRow->addWidget(s1,1);

        QWidget *s2 = new QWidget; s2->setStyleSheet("background:#F3F4F6; border-radius:10px;");
        QVBoxLayout *s2l = new QVBoxLayout(s2); s2l->setContentsMargins(12,12,12,12);
        QLabel *l2 = new QLabel("0 收藏");
        l2->setAlignment(Qt::AlignCenter);
        l2->setStyleSheet("font-size:14px; font-weight:600; color:#111827;");
        s2l->addWidget(l2); statsRow->addWidget(s2,1);
    }
    sideLayout->addLayout(statsRow);

    // Search
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("搜索笔记");
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setStyleSheet("QLineEdit { background:#F3F4F6; color:#374151; border:1px solid transparent; border-radius:17px; padding:4px 14px; font-size:13px; } QLineEdit:focus { border:1px solid #2FA66A; background:#fff; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &NoteWidget::onSearchChanged);
    sideLayout->addWidget(m_searchEdit);

    // Category list (全部/收藏/最近编辑)
    m_categoryList = new QListWidget;
    m_categoryList->setFixedHeight(108);
    m_categoryList->setStyleSheet("QListWidget { background:transparent; border:none; outline:none; } QListWidget::item { color:#6B7280; padding:7px 12px; border-radius:8px; font-size:13px; margin:1px 0; } QListWidget::item:selected { background:#EAF3DE; color:#27500A; font-weight:600; } QListWidget::item:hover:!selected { background:#F3F4F6; }");
    m_categoryList->addItem("全部笔记");
    m_categoryList->addItem("收藏");
    m_categoryList->addItem("最近编辑");
    m_categoryList->blockSignals(true);
    m_categoryList->setCurrentRow(0);
    m_categoryList->blockSignals(false);
    connect(m_categoryList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem *) {
        if (!cur) return;
        if (cur->text().contains("全部")) m_currentCategory = "all";
        else if (cur->text().contains("收藏")) m_currentCategory = "starred";
        else if (cur->text().contains("最近")) m_currentCategory = "recent";
        refreshCards();
    });
    sideLayout->addWidget(m_categoryList);

    // Folder label
    QLabel *folderLabel = new QLabel("文件夹");
    folderLabel->setStyleSheet("font-size:13px; font-weight:500; color:#6B7280;");
    sideLayout->addWidget(folderLabel);

    // Folder list
    m_folderList = new QListWidget;
    m_folderList->setStyleSheet("QListWidget { background:transparent; border:none; outline:none; } QListWidget::item { color:#6B7280; padding:6px 12px; border-radius:8px; font-size:13px; margin:1px 0; } QListWidget::item:selected { background:#EAF3DE; color:#27500A; font-weight:600; } QListWidget::item:hover:!selected { background:#F3F4F6; }");
    m_folderList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_folderList, &QListWidget::customContextMenuRequested, this, &NoteWidget::onFolderMenu);
    connect(m_folderList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem *) {
        if (!cur) return;
        QString text = cur->text();
        if (text.contains("全部笔记")) m_currentCategory = "all";
        else m_currentCategory = text.trimmed();
        refreshCards();
    });
    sideLayout->addWidget(m_folderList, 1);

    sideLayout->addStretch();

    // New note button (green, like reader import)
    m_newNoteBtn = new QPushButton("+ 新建笔记");
    m_newNoteBtn->setFixedHeight(38);
    m_newNoteBtn->setCursor(Qt::PointingHandCursor);
    m_newNoteBtn->setStyleSheet("QPushButton { background:#2FA66A; color:#fff; border:none; border-radius:19px; font-size:13px; font-weight:500; } QPushButton:hover { background:#27945D; } QPushButton:pressed { background:#1F7A4B; }");
    connect(m_newNoteBtn, &QPushButton::clicked, this, &NoteWidget::newNote);
    sideLayout->addWidget(m_newNoteBtn);

    QPushButton *journalBtn = new QPushButton("📔 今日日记");
    journalBtn->setFixedHeight(32);
    journalBtn->setCursor(Qt::PointingHandCursor);
    journalBtn->setStyleSheet("QPushButton { background:#fff; color:#2FA66A; border:1px solid #2FA66A; border-radius:16px; font-size:12px; } QPushButton:hover { background:#EAF7F0; }");
    connect(journalBtn, &QPushButton::clicked, this, &NoteWidget::journalToday);
    sideLayout->addWidget(journalBtn);

    // Sort combo
    QHBoxLayout *sortRow = new QHBoxLayout;
    sortRow->setContentsMargins(0,0,0,0);
    QLabel *sortLabel = new QLabel("排序");
    sortLabel->setStyleSheet("color:#6B7280; font-size:12px;");
    m_sortCombo = new QComboBox;
    m_sortCombo->addItems({"最近更新", "最近创建", "标题 A-Z", "收藏优先"});
    m_sortCombo->setFixedHeight(28);
    m_sortCombo->setStyleSheet("QComboBox { background:#F3F4F6; color:#374151; border:none; border-radius:14px; padding:2px 10px; font-size:12px; } QComboBox:hover { background:#E5E7EB; } QComboBox QAbstractItemView { background:#fff; color:#374151; selection-background-color:#EAF3DE; selection-color:#27500A; outline:none; border:1px solid #E5E7EB; }");
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::onSortChanged);
    sortRow->addWidget(sortLabel); sortRow->addWidget(m_sortCombo, 1);
    sideLayout->addLayout(sortRow);

    // Export all (ghost button)
    m_exportAllBtn = new QPushButton("导出全部笔记");
    m_exportAllBtn->setFixedHeight(32);
    m_exportAllBtn->setCursor(Qt::PointingHandCursor);
    m_exportAllBtn->setToolTip("将所有笔记导出为一份 Markdown 文件（按文件夹分组）");
    m_exportAllBtn->setStyleSheet("QPushButton { background:transparent; color:#6B7280; border:1px solid #E5E7EB; border-radius:16px; font-size:12px; } QPushButton:hover { border-color:#2FA66A; color:#2FA66A; }");
    connect(m_exportAllBtn, &QPushButton::clicked, this, &NoteWidget::exportAllNotes);
    sideLayout->addWidget(m_exportAllBtn);
}

void NoteWidget::setupToolbar()
{
    // ===== Row 1: character formatting =====
    m_toolbar = new QToolBar;
    m_toolbar->setFixedHeight(42);
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(kToolbarSS);

    m_actionUndo = m_toolbar->addAction("撤销");
    m_actionUndo->setToolTip("撤销");
    connect(m_actionUndo, &QAction::triggered, m_editor, &QTextEdit::undo);

    m_actionRedo = m_toolbar->addAction("重做");
    m_actionRedo->setToolTip("重做");
    connect(m_actionRedo, &QAction::triggered, m_editor, &QTextEdit::redo);

    m_toolbar->addSeparator();

    m_styleCombo = new QComboBox;
    m_styleCombo->addItems({"正文", "标题 1", "标题 2", "标题 3", "标题 4", "标题 5", "标题 6"});
    m_styleCombo->setFixedHeight(26);
    connect(m_styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::textStyle);
    m_toolbar->addWidget(m_styleCombo);

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setFixedHeight(26);
    m_fontCombo->setFixedWidth(110);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &NoteWidget::fontChanged);
    m_toolbar->addWidget(m_fontCombo);

    m_sizeCombo = new QComboBox;
    m_sizeCombo->addItems({"10", "11", "12", "13", "14", "15", "16", "18", "20", "24", "28", "32"});
    m_sizeCombo->setCurrentText("14");
    m_sizeCombo->setFixedHeight(26);
    m_sizeCombo->setFixedWidth(52);
    connect(m_sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::fontSizeChanged);
    m_toolbar->addWidget(m_sizeCombo);

    m_toolbar->addSeparator();

    m_actionBold = m_toolbar->addAction("B");
    m_actionBold->setToolTip("加粗");
    QFont boldFont = m_actionBold->font(); boldFont.setBold(true);
    m_actionBold->setFont(boldFont);
    m_actionBold->setCheckable(true);
    connect(m_actionBold, &QAction::triggered, this, &NoteWidget::textBold);

    m_actionItalic = m_toolbar->addAction("I");
    m_actionItalic->setToolTip("斜体");
    QFont italicFont = m_actionItalic->font(); italicFont.setItalic(true);
    m_actionItalic->setFont(italicFont);
    m_actionItalic->setCheckable(true);
    connect(m_actionItalic, &QAction::triggered, this, &NoteWidget::textItalic);

    m_actionUnderline = m_toolbar->addAction("U");
    m_actionUnderline->setToolTip("下划线");
    QFont ulFont = m_actionUnderline->font(); ulFont.setUnderline(true);
    m_actionUnderline->setFont(ulFont);
    m_actionUnderline->setCheckable(true);
    connect(m_actionUnderline, &QAction::triggered, this, &NoteWidget::textUnderline);

    m_actionStrike = m_toolbar->addAction("S");
    m_actionStrike->setToolTip("删除线");
    QFont strikeFont = m_actionStrike->font(); strikeFont.setStrikeOut(true);
    m_actionStrike->setFont(strikeFont);
    m_actionStrike->setCheckable(true);
    connect(m_actionStrike, &QAction::triggered, this, &NoteWidget::textStrike);

    m_actionSuper = m_toolbar->addAction("x²");
    m_actionSuper->setToolTip("上标");
    m_actionSuper->setCheckable(true);
    connect(m_actionSuper, &QAction::triggered, this, &NoteWidget::textSuperScript);

    m_actionSub = m_toolbar->addAction("x₂");
    m_actionSub->setToolTip("下标");
    m_actionSub->setCheckable(true);
    connect(m_actionSub, &QAction::triggered, this, &NoteWidget::textSubScript);

    m_toolbar->addSeparator();

    // Highlight pen: click applies the current color, arrow opens the palette.
    m_highlightBtn = new QToolButton;
    m_highlightBtn->setText("高亮");
    m_highlightBtn->setToolTip("文字高亮笔：点击套用当前颜色，箭头选色");
    m_highlightBtn->setPopupMode(QToolButton::MenuButtonPopup);
    m_highlightBtn->setStyleSheet("QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 7px; font-size: 13px; } QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; } QToolButton::menu-indicator { subcontrol-position: right center; subcontrol-origin: padding; left: -2px; }");
    QAction *highlightNow = new QAction("高亮", this);
    connect(highlightNow, &QAction::triggered, this, [this]() { applyHighlightColor(m_currentHighlight); });
    m_highlightBtn->setDefaultAction(highlightNow);
    QMenu *hlMenu = new QMenu(m_highlightBtn);
    hlMenu->setStyleSheet(kDarkMenuSS);
    auto addHlColor = [this, hlMenu](const QColor &c, const QString &name) {
        QAction *a = new QAction(name, this);
        QPixmap pm(12, 12);
        pm.fill(c);
        a->setIcon(QIcon(pm));
        connect(a, &QAction::triggered, this, [this, c]() {
            m_currentHighlight = c;
            applyHighlightColor(c);
        });
        hlMenu->addAction(a);
    };
    addHlColor(QColor(0xFF, 0xF3, 0x00), "黄色");
    addHlColor(QColor(0x7C, 0xFC, 0x00), "绿色");
    addHlColor(QColor(0x00, 0xE5, 0xFF), "青色");
    addHlColor(QColor(0xFF, 0x80, 0xAB), "粉色");
    addHlColor(QColor(0xFF, 0xA7, 0x26), "橙色");
    addHlColor(QColor(0xD5, 0x00, 0xF9), "紫色");
    hlMenu->addSeparator();
    QAction *hlCustom = new QAction("自定义颜色…", this);
    connect(hlCustom, &QAction::triggered, this, [this]() {
        const QColor c = QColorDialog::getColor(m_currentHighlight, this, "选择高亮颜色");
        if (c.isValid()) {
            m_currentHighlight = c;
            applyHighlightColor(c);
        }
    });
    hlMenu->addAction(hlCustom);
    hlMenu->addAction("清除高亮", this, &NoteWidget::clearHighlight);
    m_highlightBtn->setMenu(hlMenu);
    m_toolbar->addWidget(m_highlightBtn);

    m_actionColor = m_toolbar->addAction("A");
    m_actionColor->setToolTip("文字颜色");
    connect(m_actionColor, &QAction::triggered, this, &NoteWidget::textColor);

    m_actionInlineCode = m_toolbar->addAction("</>");
    m_actionInlineCode->setToolTip("行内代码");
    connect(m_actionInlineCode, &QAction::triggered, this, &NoteWidget::insertInlineCode);

    // Format painter: click = paint once, double-click = continuous (Esc exits).
    m_painterBtn = new QToolButton;
    m_painterBtn->setToolTip("格式刷：单击刷一次，双击连续刷（Esc 退出）");
    m_painterBtn->setStyleSheet("QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 7px; font-size: 13px; } QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; } QToolButton:checked { background: #2a475e; color: #66c0f4; }");
    m_actionFormatPainter = new QAction("格式刷", this);
    m_actionFormatPainter->setCheckable(true);
    connect(m_actionFormatPainter, &QAction::triggered, this, &NoteWidget::toggleFormatPainter);
    m_painterBtn->setDefaultAction(m_actionFormatPainter);
    m_painterBtn->installEventFilter(this);
    m_toolbar->addWidget(m_painterBtn);

    m_actionClearFormat = m_toolbar->addAction("清除格式");
    m_actionClearFormat->setToolTip("清除格式");
    connect(m_actionClearFormat, &QAction::triggered, this, &NoteWidget::clearFormat);

    // ===== Row 2: paragraph & insert =====
    m_toolbar2 = new QToolBar;
    m_toolbar2->setFixedHeight(42);
    m_toolbar2->setMovable(false);
    m_toolbar2->setStyleSheet(kToolbarSS);

    m_actionAlignLeft = m_toolbar2->addAction("左对齐");
    m_actionAlignLeft->setToolTip("左对齐");
    m_actionAlignLeft->setCheckable(true);
    connect(m_actionAlignLeft, &QAction::triggered, this, [this]() { textAlign(m_actionAlignLeft); });

    m_actionAlignCenter = m_toolbar2->addAction("居中");
    m_actionAlignCenter->setToolTip("居中");
    m_actionAlignCenter->setCheckable(true);
    connect(m_actionAlignCenter, &QAction::triggered, this, [this]() { textAlign(m_actionAlignCenter); });

    m_actionAlignRight = m_toolbar2->addAction("右对齐");
    m_actionAlignRight->setToolTip("右对齐");
    m_actionAlignRight->setCheckable(true);
    connect(m_actionAlignRight, &QAction::triggered, this, [this]() { textAlign(m_actionAlignRight); });

    m_actionAlignJustify = m_toolbar2->addAction("两端对齐");
    m_actionAlignJustify->setToolTip("两端对齐");
    m_actionAlignJustify->setCheckable(true);
    connect(m_actionAlignJustify, &QAction::triggered, this, [this]() { textAlign(m_actionAlignJustify); });

    m_toolbar2->addSeparator();

    m_actionIndentDec = m_toolbar2->addAction("减少缩进");
    m_actionIndentDec->setToolTip("减少缩进");
    connect(m_actionIndentDec, &QAction::triggered, this, &NoteWidget::indentDecrease);

    m_actionIndentInc = m_toolbar2->addAction("增加缩进");
    m_actionIndentInc->setToolTip("增加缩进");
    connect(m_actionIndentInc, &QAction::triggered, this, &NoteWidget::indentIncrease);

    m_toolbar2->addSeparator();

    m_actionListBullet = m_toolbar2->addAction("无序列表");
    m_actionListBullet->setToolTip("无序列表");
    connect(m_actionListBullet, &QAction::triggered, this, [this]() { insertList(m_actionListBullet); });

    m_actionListNumber = m_toolbar2->addAction("有序列表");
    m_actionListNumber->setToolTip("有序列表");
    connect(m_actionListNumber, &QAction::triggered, this, [this]() { insertList(m_actionListNumber); });

    m_actionTaskList = m_toolbar2->addAction("任务列表");
    m_actionTaskList->setToolTip("任务列表（复选框，点击勾选）");
    connect(m_actionTaskList, &QAction::triggered, this, &NoteWidget::insertTaskList);

    m_toolbar2->addSeparator();

    m_lineSpacingCombo = new QComboBox;
    m_lineSpacingCombo->addItems({"1.0", "1.15", "1.5", "2.0", "2.5", "3.0"});
    m_lineSpacingCombo->setToolTip("行距（倍数）");
    m_lineSpacingCombo->setFixedHeight(26);
    m_lineSpacingCombo->setFixedWidth(58);
    connect(m_lineSpacingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::lineSpacingChanged);
    m_toolbar2->addWidget(m_lineSpacingCombo);

    m_actionFirstLineIndent = m_toolbar2->addAction("首行缩进");
    m_actionFirstLineIndent->setToolTip("首行缩进 2 字符");
    m_actionFirstLineIndent->setCheckable(true);
    connect(m_actionFirstLineIndent, &QAction::triggered, this, &NoteWidget::toggleFirstLineIndent);

    m_toolbar2->addSeparator();

    m_actionQuote = m_toolbar2->addAction("引用");
    connect(m_actionQuote, &QAction::triggered, this, &NoteWidget::insertQuote);

    m_actionCode = m_toolbar2->addAction("代码块");
    connect(m_actionCode, &QAction::triggered, this, &NoteWidget::insertCodeBlock);

    m_actionTable = m_toolbar2->addAction("表格");
    m_actionTable->setToolTip("插入表格（右键编辑行列）");
    connect(m_actionTable, &QAction::triggered, this, &NoteWidget::insertTableDialog);

    // 插入 menu keeps row 2 compact: media, links, date/time, HR, background.
    QToolButton *insertBtn = new QToolButton;
    insertBtn->setText("插入 ▾");
    insertBtn->setToolTip("插入图片 / 链接 / 双链 / 日期 / 分割线 / 背景");
    insertBtn->setPopupMode(QToolButton::InstantPopup);
    insertBtn->setStyleSheet("QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 8px; font-size: 13px; } QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; } QToolButton::menu-indicator { image: none; }");
    QMenu *insertMenu = new QMenu(insertBtn);
    insertMenu->setStyleSheet(kDarkMenuSS);
    m_actionImage = insertMenu->addAction("图片…", this, &NoteWidget::insertImage);
    m_actionLink = insertMenu->addAction("链接…", this, &NoteWidget::insertLink);
    QAction *wikiAct = insertMenu->addAction("[[双链]]…", this, &NoteWidget::insertWikiLinkDialog);
    wikiAct->setToolTip("插入 [[笔记双链]]，Ctrl+点击跳转");
    QMenu *dtMenu = insertMenu->addMenu("日期和时间");
    auto addDt = [this, dtMenu](const QString &label, int fmtId) {
        dtMenu->addAction(label, this, [this, fmtId]() { insertDateTime(fmtId); });
    };
    addDt("2026-10-04（短日期）", 0);
    addDt("2026年10月4日（长日期）", 1);
    addDt("14:30（时间）", 2);
    addDt("2026-10-04 14:30:05（日期+时间）", 3);
    addDt("星期日（星期）", 4);
    m_actionHR = insertMenu->addAction("分割线", this, &NoteWidget::insertHR);
    insertMenu->addSeparator();
    QMenu *bgMenu = insertMenu->addMenu("编辑器背景");
    bgMenu->addAction("选择背景图片...", this, &NoteWidget::changeEditorBackground);
    bgMenu->addAction("恢复默认背景", this, &NoteWidget::clearEditorBackground);
    insertBtn->setMenu(insertMenu);
    m_toolbar2->addWidget(insertBtn);

    // Ctrl+H opens the find bar with the replace row focused (Ctrl+F is
    // intercepted in the editor's ShortcutOverride handler).
    QShortcut *replaceSc = new QShortcut(QKeySequence("Ctrl+H"), this);
    connect(replaceSc, &QShortcut::activated, this, [this]() { openFindBar(true); });
}

QWidget *NoteWidget::createCard(const NoteData &note)
{
    QWidget *card = new QWidget;
    // Minimum width keeps small windows usable; the column stretch in
    // refreshCards() lets cards grow to fill wide monitors instead of
    // leaving empty space on the right.
    card->setMinimumWidth(240);
    card->setMaximumWidth(420);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setMinimumHeight(160);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(R"(
        QWidget { background: #FFFFFF; border-radius: 14px; border: 1px solid #E5E7EB; }
        QWidget:hover { border: 1px solid #2FA66A; background: #FAFBFC; }
    )");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 12);
    cardLayout->setSpacing(8);

    // Title
    QLabel *titleLabel = new QLabel(note.title.isEmpty() ? "无标题" : note.title);
    titleLabel->setStyleSheet("color: #111827; font-size: 14px; font-weight: 600; background: transparent;");
    titleLabel->setWordWrap(true);
    titleLabel->setMaximumHeight(40);
    cardLayout->addWidget(titleLabel);

    // Preview
    QString preview = htmlToPreview(note.content);
    QLabel *previewLabel = new QLabel(preview);
    previewLabel->setStyleSheet("color: #9CA3AF; font-size: 12px; background: transparent;");
    previewLabel->setWordWrap(true);
    previewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    cardLayout->addWidget(previewLabel, 1);

    // Bottom: starred tag + date
    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);
    if (note.starred) {
        QLabel *starTag = new QLabel("★ 已收藏");
        starTag->setStyleSheet("background: #FEF3C7; color: #B45309; font-size: 11px; padding: 2px 8px; border-radius: 8px;");
        bottomLayout->addWidget(starTag);
    }
    bottomLayout->addStretch();
    QLabel *dateLabel = new QLabel(note.updatedAt.toString("MM-dd"));
    dateLabel->setStyleSheet("color: #9CA3AF; font-size: 11px; background: transparent;");
    bottomLayout->addWidget(dateLabel);
    cardLayout->addLayout(bottomLayout);

    // Make card clickable
    card->installEventFilter(this);
    card->setProperty("noteId", note.id);

    return card;
}

void NoteWidget::refreshCards()
{
    if (!m_gridLayout) return;
    // Clear existing cards
    QLayoutItem *child;
    while ((child = m_gridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    // Filter notes
    QList<NoteData> filtered;
    QString search = m_searchEdit ? m_searchEdit->text().toLower() : "";

    for (const NoteData &note : m_notes) {
        // Category filter
        if (m_currentCategory == "starred" && !note.starred) continue;
        if (m_currentCategory != "all" && m_currentCategory != "starred" && m_currentCategory != "recent") {
            if (note.folder != m_currentCategory) continue;
        }
        // Search filter
        if (!search.isEmpty()) {
            if (!note.title.toLower().contains(search) && !note.content.toLower().contains(search)) continue;
        }
        filtered.append(note);
    }

    // Sort: starred always first, then by selected mode (微信读书/印象笔记标准)
    std::sort(filtered.begin(), filtered.end(), [this](const NoteData &a, const NoteData &b) {
        if (a.starred != b.starred) return a.starred;
        if (m_sortMode == "created") return a.createdAt > b.createdAt;
        if (m_sortMode == "title") return a.title.toLower() < b.title.toLower();
        return a.updatedAt > b.updatedAt;
    });

    // Calculate columns
    int viewWidth = m_gridScroll->viewport()->width() - 40;
    int cols = qMax(1, viewWidth / 260);

    // Every column gets equal stretch so cards fill the available width
    // instead of piling up on the left.
    for (int c = 0; c < cols; ++c)
        m_gridLayout->setColumnStretch(c, 1);

    for (int i = 0; i < filtered.size(); ++i) {
        QWidget *card = createCard(filtered[i]);
        m_gridLayout->addWidget(card, i / cols, i % cols);
    }

    // Add stretch to keep cards at top
    m_gridLayout->setRowStretch(filtered.size() / cols + 1, 1);

    // Empty-state hint
    if (filtered.isEmpty()) {
        if (!m_emptyLabel) {
            m_emptyLabel = new QLabel(m_gridScroll->viewport());
            m_emptyLabel->setAlignment(Qt::AlignCenter);
            m_emptyLabel->setWordWrap(true);
            m_emptyLabel->setStyleSheet("background: transparent; color: #7a8794; font-size: 13px;");
            m_gridScroll->viewport()->installEventFilter(this);
        }
            m_emptyLabel->setText(
            "<div style='color:#6B7280; font-size:14px; font-weight:600; margin-bottom:6px;'>还没有笔记</div>"
            "<div style='color:#9CA3AF; font-size:12px;'>点击「新建笔记」开始记录，<br>或把文字、图片拖进编辑器</div>");
        m_emptyLabel->setGeometry(m_gridScroll->viewport()->rect());
        m_emptyLabel->show();
        m_emptyLabel->raise();
    } else if (m_emptyLabel) {
        m_emptyLabel->hide();
    }
}

void NoteWidget::openNote(const QString &noteId)
{
    saveCurrentNote();
    m_currentNoteId = noteId;
    m_loading = true;

    auto it = std::find_if(m_notes.begin(), m_notes.end(), [&](const NoteData &n) { return n.id == noteId; });
    if (it != m_notes.end()) {
        m_titleEdit->setText(it->title);
        m_editor->setHtml(it->content);
        m_starBtn->setChecked(it->starred);
    }

    m_loading = false;
    m_modified = false;
    deactivateFormatPainter();
    if (m_findBar && m_findBar->isVisible()) closeFindBar();
    refreshOutline();
    m_stack->setCurrentIndex(1);
    updateStatusBar();
}

void NoteWidget::backToGrid()
{
    saveCurrentNote();
    m_currentNoteId.clear();
    m_stack->setCurrentIndex(0);
    refreshCards();
}

void NoteWidget::newNote()
{
    saveCurrentNote();
    NoteData note;
    note.id = generateId();
    note.title = "无标题笔记";
    note.content = "";
    note.createdAt = QDateTime::currentDateTime();
    note.updatedAt = QDateTime::currentDateTime();
    note.folder = m_currentCategory == "all" || m_currentCategory == "starred" || m_currentCategory == "recent" ? "" : m_currentCategory;
    m_notes.prepend(note);
    saveNotes();
    openNote(note.id);
    m_titleEdit->selectAll();
    m_titleEdit->setFocus();
}

void NoteWidget::newNoteWithExcerpt(const QString &title, const QString &excerpt, const QString &source)
{
    saveCurrentNote();
    NoteData note;
    note.id = generateId();
    note.title = title;
    // Build a styled blockquote note: indented quote + source footer.
    note.content = QStringLiteral(
        "<blockquote style=\"margin: 12px 0; padding: 12px 16px; "
        "border-left: 3px solid #1a73e8; background: #f1f3f4; "
        "color: #202124; border-radius: 0 8px 8px 0;\">"
        "%1"
        "</blockquote>"
        "<p style=\"color:#5f6368; font-size:12px;\">—— %2</p>"
        "<p><br></p>"
    ).arg(excerpt.toHtmlEscaped().replace("\n", "<br>"), source.toHtmlEscaped());
    note.createdAt = QDateTime::currentDateTime();
    note.updatedAt = QDateTime::currentDateTime();
    note.folder = "";
    m_notes.prepend(note);
    saveNotes();
    openNote(note.id);
}

void NoteWidget::journalToday()
{
    // Logseq-style daily journal: title "日记 YYYY-MM-DD".
    const QString title = QStringLiteral("日记 %1").arg(QDate::currentDate().toString("yyyy-MM-dd"));
    openOrCreateByTitle(title);
}

void NoteWidget::openOrCreateByTitle(const QString &title)
{
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [&](const NoteData &n) {
        return n.title.compare(title, Qt::CaseInsensitive) == 0;
    });
    if (it != m_notes.end()) {
        openNote(it->id);
        return;
    }
    saveCurrentNote();
    NoteData note;
    note.id = generateId();
    note.title = title;
    note.content = "";
    note.createdAt = QDateTime::currentDateTime();
    note.updatedAt = QDateTime::currentDateTime();
    note.folder = "日记";
    m_notes.prepend(note);
    saveNotes();
    openNote(note.id);
}

void NoteWidget::deleteNote()
{
    if (m_currentNoteId.isEmpty()) return;
    if (QMessageBox::question(this, "删除笔记", "确定删除这篇笔记吗？") != QMessageBox::Yes) return;
    m_notes.removeIf([this](const NoteData &n) { return n.id == m_currentNoteId; });
    m_currentNoteId.clear();
    saveNotes();
    m_stack->setCurrentIndex(0);
    refreshCards();
}

void NoteWidget::toggleStar()
{
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [this](const NoteData &n) { return n.id == m_currentNoteId; });
    if (it != m_notes.end()) {
        it->starred = !it->starred;
        it->updatedAt = QDateTime::currentDateTime();
        m_starBtn->setChecked(it->starred);
        saveNotes();
    }
}

void NoteWidget::onTitleChanged()
{
    if (m_loading) return;
    m_modified = true;
    emit modificationChanged(true);
}

void NoteWidget::onSearchChanged(const QString &)
{
    refreshCards();
}

void NoteWidget::onCategoryChanged(QListWidgetItem *)
{
    refreshCards();
}

void NoteWidget::refreshFolderList()
{
    if (!m_folderList) return;
    m_folderList->blockSignals(true);
    m_folderList->clear();
    m_folderList->addItem("全部笔记");
    for (const QString &f : m_folders)
        m_folderList->addItem(f);
    // Restore current selection if it is a real folder
    int row = 0;
    if (m_currentCategory != "all" && m_currentCategory != "starred" && m_currentCategory != "recent") {
        int idx = m_folders.indexOf(m_currentCategory);
        if (idx >= 0) row = idx + 1;
    }
    m_folderList->setCurrentRow(row);
    m_folderList->blockSignals(false);
}

void NoteWidget::onFolderMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: #1b2838; border: 1px solid #2a475e; } QMenu::item { padding: 8px 24px; color: #c7d5e0; font-size: 13px; } QMenu::item:selected { background: #2a475e; color: #66c0f4; }");
    QAction *newAct = menu.addAction("新建文件夹", this, &NoteWidget::newFolder);
    QListWidgetItem *item = m_folderList->itemAt(pos);
    if (item && m_folderList->row(item) > 0) {
        QAction *renameAct = menu.addAction("重命名", this, &NoteWidget::renameFolder);
        QAction *delAct = menu.addAction("删除文件夹", this, &NoteWidget::deleteFolder);
        Q_UNUSED(renameAct); Q_UNUSED(delAct);
    }
    Q_UNUSED(newAct);
    menu.exec(m_folderList->viewport()->mapToGlobal(pos));
}

void NoteWidget::newFolder()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, "新建文件夹", "文件夹名称：", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    const QString f = name.trimmed();
    if (m_folders.contains(f)) { QMessageBox::information(this, "提示", "该文件夹已存在"); return; }
    m_folders.append(f);
    saveNotes();
    refreshFolderList();
    m_currentCategory = f;
    refreshCards();
}

void NoteWidget::renameFolder()
{
    QListWidgetItem *item = m_folderList->currentItem();
    if (!item || m_folderList->row(item) <= 0) return;
    const QString old = item->text().trimmed();
    bool ok = false;
    const QString name = QInputDialog::getText(this, "重命名文件夹", "新名称：", QLineEdit::Normal, old, &ok);
    if (!ok || name.trimmed().isEmpty() || name.trimmed() == old) return;
    const QString f = name.trimmed();
    if (m_folders.contains(f)) { QMessageBox::information(this, "提示", "该文件夹已存在"); return; }
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders[i] == old) m_folders[i] = f;
    }
    for (NoteData &n : m_notes) {
        if (n.folder == old) n.folder = f;
    }
    if (m_currentCategory == old) m_currentCategory = f;
    saveNotes();
    refreshFolderList();
    refreshCards();
}

void NoteWidget::deleteFolder()
{
    QListWidgetItem *item = m_folderList->currentItem();
    if (!item || m_folderList->row(item) <= 0) return;
    const QString f = item->text().trimmed();
    int count = 0;
    for (const NoteData &n : m_notes)
        if (n.folder == f) ++count;
    const QString msg = QString("删除文件夹「%1」？其中 %2 条笔记将移入「未分类」。")
                            .arg(f).arg(count);
    if (QMessageBox::question(this, "删除文件夹", msg) != QMessageBox::Yes)
        return;
    m_folders.removeAll(f);
    for (NoteData &n : m_notes) {
        if (n.folder == f) n.folder = "";
    }
    if (m_currentCategory == f) m_currentCategory = "all";
    saveNotes();
    refreshFolderList();
    refreshCards();
}

void NoteWidget::onSortChanged(int index)
{
    switch (index) {
    case 1: m_sortMode = "created"; break;
    case 2: m_sortMode = "title"; break;
    case 3: m_sortMode = "starred"; break;
    default: m_sortMode = "updated"; break;
    }
    QSettings settings("PureRefCanvas", "里与由");
    settings.setValue("note/sortMode", m_sortMode);
    refreshCards();
}

// ========== Export & copy (Markdown) ==========

QString NoteWidget::htmlToMarkdown(const QString &html) const
{
    QTextDocument doc;
    doc.setHtml(html);
    QString md;
    for (QTextBlock blk = doc.begin(); blk != doc.end(); blk = blk.next()) {
        // Emit each table once (at its first cell block); skip the other cells.
        if (QTextTable *tbl = QTextCursor(blk).currentTable()) {
            if (tbl->firstPosition() == blk.position())
                md += tableToMarkdown(tbl) + "\n";
            continue;
        }
        const QString line = blockToMarkdown(blk);
        if (!line.isNull())
            md += line + "\n";
    }
    return md;
}

QString NoteWidget::blockToMarkdown(const QTextBlock &block) const
{
    const QString raw = block.text();
    if (raw.trimmed().isEmpty()) return QString();

    const QTextBlockFormat bf = block.blockFormat();
    const int h = bf.headingLevel();

    // Task list item: ☐/☑ text marker → GFM task syntax
    if (isTaskMarkerChar(raw.at(0))) {
        const bool checked = raw.at(0) == QChar(0x2611);
        QString rest = raw.mid(1);
        if (rest.startsWith(' ')) rest.remove(0, 1);
        return QString("- [%1] %2").arg(checked ? "x" : " ", rest.trimmed());
    }

    const bool isCodeBlock = block.charFormat().fontFamilies().toStringList().contains("Consolas")
                             && bf.background().color() == QColor(245, 245, 245);
    const bool isQuote = bf.leftMargin() >= 30 && bf.background().color() == QColor(240, 248, 255);

    QString line;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment frag = it.fragment();
        if (!frag.isValid() || frag.text().isEmpty()) continue;
        QString t = frag.text();
        const QTextCharFormat fmt = frag.charFormat();
        const QString href = fmt.anchorHref();
        if (href.startsWith("wikilink://"))
            t = "[[" + href.mid(11) + "]]";
        else if (!href.isEmpty())
            t = QString("[%1](%2)").arg(t, href);
        // headings are bold by definition — don't wrap them in **
        if (fmt.fontWeight() == QFont::Bold && h <= 0) t = "**" + t + "**";
        if (fmt.fontItalic()) t = "*" + t + "*";
        if (fmt.fontStrikeOut()) t = "~~" + t + "~~";
        const QBrush bg = fmt.background();
        const bool isInlineCode = fmt.fontFamilies().toStringList().contains("Consolas")
                                  && bg.style() != Qt::NoBrush && bg.color() != QColor(240, 248, 255);
        if (isInlineCode) t = "`" + t + "`";
        // Highlight pen colors → ==mark==
        static const QColor hlColors[] = {
            QColor(0xFF, 0xF3, 0x00), QColor(0x7C, 0xFC, 0x00), QColor(0x00, 0xE5, 0xFF),
            QColor(0xFF, 0x80, 0xAB), QColor(0xFF, 0xA7, 0x26), QColor(0xD5, 0x00, 0xF9)
        };
        if (bg.style() != Qt::NoBrush
            && std::any_of(std::begin(hlColors), std::end(hlColors), [&bg](const QColor &c) { return bg.color() == c; }))
            t = "==" + t + "==";
        if (fmt.verticalAlignment() == QTextCharFormat::AlignSuperScript && t.length() == 1) t = "^" + t + "^";
        if (fmt.verticalAlignment() == QTextCharFormat::AlignSubScript && t.length() == 1) t = "~" + t + "~";
        line += t;
    }
    if (line.trimmed().isEmpty()) return QString();

    QString prefix;
    if (h > 0) {
        prefix = QString(h, '#') + " ";  // a heading is never a list item
    } else if (block.textList()) {
        const QTextListFormat lf = block.textList()->format();
        prefix = (lf.style() == QTextListFormat::ListDecimal) ? "1. " : "- ";
    }
    if (isQuote) prefix += "> ";
    if (isCodeBlock) return "`" + line.trimmed() + "`";
    return prefix + line;
}

QString NoteWidget::tableToMarkdown(QTextTable *table) const
{
    if (!table) return QString();
    const int rows = table->rows();
    const int cols = table->columns();
    if (rows <= 0 || cols <= 0) return QString();

    QVector<QStringList> grid(rows);
    for (int r = 0; r < rows; ++r) {
        grid[r].reserve(cols);
        for (int c = 0; c < cols; ++c) {
            const QTextTableCell cell = table->cellAt(r, c);
            QString cellText;
            if (cell.isValid()) {
                QTextBlock b = cell.firstCursorPosition().block();
                const QTextBlock end = cell.lastCursorPosition().block();
                for (; b.isValid(); b = b.next()) {
                    if (!cellText.isEmpty()) cellText += " ";
                    cellText += b.text().trimmed();
                    if (b == end) break;
                }
            }
            cellText.replace("|", "\\|");
            grid[r].append(cellText.isEmpty() ? " " : cellText);
        }
    }

    auto emitRow = [](const QStringList &cells) { return "| " + cells.join(" | ") + " |"; };
    QString md = emitRow(grid[0]) + "\n"; // GFM needs a header row
    md += emitRow(QStringList(cols, QString("---"))) + "\n";
    for (int r = 1; r < rows; ++r) md += emitRow(grid[r]) + "\n";
    return md;
}

QString NoteWidget::noteToMarkdown(const NoteData &note) const
{
    QString md = QString("# %1\n\n").arg(note.title.isEmpty() ? "无标题笔记" : note.title);
    md += QString("- 文件夹：%1\n").arg(note.folder.isEmpty() ? "未分类" : note.folder);
    md += QString("- 创建：%1\n").arg(note.createdAt.toString("yyyy-MM-dd HH:mm"));
    md += QString("- 更新：%1\n").arg(note.updatedAt.toString("yyyy-MM-dd HH:mm"));
    md += note.starred ? "- 收藏：⭐\n" : "";
    md += "\n";
    QString body = htmlToMarkdown(note.content);
    if (body.trimmed().isEmpty()) body = "_（空笔记）_\n";
    md += body;
    md += "\n\n---\n\n";
    return md;
}

void NoteWidget::exportCurrentNote()
{
    saveCurrentNote();
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [this](const NoteData &n) { return n.id == m_currentNoteId; });
    if (it == m_notes.end()) { QMessageBox::information(this, "提示", "请先打开一篇笔记"); return; }
    const QString title = it->title.isEmpty() ? "无标题笔记" : it->title;
    QString fileName = QFileDialog::getSaveFileName(this, "导出笔记", title + ".md",
                                                    "Markdown (*.md);;纯文本 (*.txt);;HTML (*.html)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件：" + fileName);
        return;
    }
    file.write(noteToMarkdown(*it).toUtf8());
    file.close();
    QMessageBox::information(this, "导出完成", QString("已导出到：\n%1").arg(fileName));
}

void NoteWidget::exportAllNotes()
{
    saveCurrentNote();
    if (m_notes.isEmpty()) { QMessageBox::information(this, "提示", "还没有笔记"); return; }
    QString fileName = QFileDialog::getSaveFileName(this, "导出全部笔记", "全部笔记.md",
                                                    "Markdown (*.md);;纯文本 (*.txt)");
    if (fileName.isEmpty()) return;

    QString md = "# 全部笔记\n\n";
    // Group by folder, folders first (保持与侧栏一致的浏览逻辑)
    QStringList groupKeys = m_folders;
    groupKeys << "未分类";
    for (const QString &key : groupKeys) {
        const QString folderName = key == "未分类" ? "" : key;
        QList<NoteData> inGroup;
        for (const NoteData &n : m_notes)
            if ((folderName.isEmpty() && n.folder.isEmpty()) || n.folder == folderName)
                inGroup.append(n);
        if (inGroup.isEmpty()) continue;
        md += QString("## %1（%2 条）\n\n").arg(key, QString::number(inGroup.size()));
        for (const NoteData &n : inGroup) md += noteToMarkdown(n);
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件：" + fileName);
        return;
    }
    file.write(md.toUtf8());
    file.close();
    QMessageBox::information(this, "导出完成", QString("已导出 %1 条笔记到：\n%2").arg(m_notes.size()).arg(fileName));
}

void NoteWidget::copyCurrentMarkdown()
{
    saveCurrentNote();
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [this](const NoteData &n) { return n.id == m_currentNoteId; });
    if (it == m_notes.end()) { QMessageBox::information(this, "提示", "请先打开一篇笔记"); return; }
    QApplication::clipboard()->setText(noteToMarkdown(*it));
    QMessageBox::information(this, "已复制", "笔记已以 Markdown 格式复制到剪贴板");
}

void NoteWidget::copyAllMarkdown()
{
    saveCurrentNote();
    if (m_notes.isEmpty()) { QMessageBox::information(this, "提示", "还没有笔记"); return; }
    QString md = "# 全部笔记\n\n";
    for (const NoteData &n : m_notes) md += noteToMarkdown(n);
    QApplication::clipboard()->setText(md);
    QMessageBox::information(this, "已复制", QString("已复制 %1 条笔记到剪贴板").arg(m_notes.size()));
}

// ========== Editor formatting (same as before) ==========

void NoteWidget::onTextChanged()
{
    if (m_loading) return;
    m_modified = true;
    emit modificationChanged(true);
    updateStatusBar();
    refreshOutline();
}

void NoteWidget::refreshOutline()
{
    if (!m_outlineList || !m_editor) return;
    m_outlineList->clear();
    QTextBlock block = m_editor->document()->firstBlock();
    while (block.isValid()) {
        // True heading levels (set via the 样式 combo), not font heuristics.
        const int level = block.blockFormat().headingLevel();
        if (level > 0) {
            const QString text = block.text().trimmed();
            if (!text.isEmpty()) {
                QListWidgetItem *item = new QListWidgetItem(m_outlineList);
                item->setText(QString((qMin(level, 6) - 1) * 4, ' ') + (level <= 2 ? "▸ " : "· ") + text);
                item->setData(Qt::UserRole, block.blockNumber());
                item->setForeground(level <= 2 ? QColor(0x27, 0x50, 0x0A) : QColor(0x6B, 0x72, 0x80));
            }
        }
        block = block.next();
    }
}

void NoteWidget::onCursorPositionChanged()
{
    QTextCharFormat fmt = m_editor->currentCharFormat();
    m_actionBold->setChecked(fmt.fontWeight() == QFont::Bold);
    m_actionItalic->setChecked(fmt.fontItalic());
    m_actionUnderline->setChecked(fmt.fontUnderline());
    m_actionStrike->setChecked(fmt.fontStrikeOut());
    m_actionSuper->setChecked(fmt.verticalAlignment() == QTextCharFormat::AlignSuperScript);
    m_actionSub->setChecked(fmt.verticalAlignment() == QTextCharFormat::AlignSubScript);

    // Live format painter: apply while the target selection is being made.
    if (m_actionFormatPainter->isChecked())
        applyFormatPainter();

    // Paragraph state → toolbar sync
    const QTextBlockFormat bf = m_editor->textCursor().blockFormat();
    const Qt::Alignment align = bf.alignment();
    m_actionAlignLeft->setChecked(align & Qt::AlignLeft);
    m_actionAlignCenter->setChecked(align & Qt::AlignCenter);
    m_actionAlignRight->setChecked(align & Qt::AlignRight);
    m_actionAlignJustify->setChecked(align & Qt::AlignJustify);
    m_actionFirstLineIndent->setChecked(bf.textIndent() > 0.0);

    // Line spacing combo: map proportional height back to the closest entry.
    if (m_lineSpacingCombo) {
        static const qreal mults[] = {100.0, 115.0, 150.0, 200.0, 250.0, 300.0};
        int lsIdx = 0;
        if (bf.lineHeightType() == QTextBlockFormat::ProportionalHeight) {
            const qreal h = bf.lineHeight();
            qreal bestDiff = 1e9;
            for (int i = 0; i < 6; ++i) {
                const qreal d = qAbs(h - mults[i]);
                if (d < bestDiff) { bestDiff = d; lsIdx = i; }
            }
        }
        if (m_lineSpacingCombo->currentIndex() != lsIdx) {
            m_lineSpacingCombo->blockSignals(true);
            m_lineSpacingCombo->setCurrentIndex(lsIdx);
            m_lineSpacingCombo->blockSignals(false);
        }
    }

    // Character state → combos sync (guarded to avoid re-applying formats)
    const int head = bf.headingLevel();
    const int styleIdx = (head >= 1 && head <= 6) ? head : 0;
    if (m_styleCombo->currentIndex() != styleIdx) {
        m_styleCombo->blockSignals(true);
        m_styleCombo->setCurrentIndex(styleIdx);
        m_styleCombo->blockSignals(false);
    }
    m_fontCombo->blockSignals(true);
    m_fontCombo->setCurrentFont(fmt.font());
    m_fontCombo->blockSignals(false);
    const qreal ps = fmt.fontPointSize();
    if (ps > 0) {
        m_sizeCombo->blockSignals(true);
        const QString psTxt = QString::number(qRound(ps));
        if (m_sizeCombo->findText(psTxt) < 0) m_sizeCombo->addItem(psTxt);
        m_sizeCombo->setCurrentText(psTxt);
        m_sizeCombo->blockSignals(false);
    }
}

void NoteWidget::mergeFormatOnSelection(const QTextCharFormat &format)
{
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    m_editor->mergeCurrentCharFormat(format);
}

void NoteWidget::textBold()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(m_actionBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(m_actionItalic->isChecked());
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(m_actionUnderline->isChecked());
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textStrike()
{
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(m_actionStrike->isChecked());
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textColor()
{
    QColor c = QColorDialog::getColor(m_currentColor, this, "选择文字颜色");
    if (!c.isValid()) return;
    m_currentColor = c;
    QTextCharFormat fmt;
    fmt.setForeground(c);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textAlign(QAction *action)
{
    Qt::Alignment a = Qt::AlignLeft;
    if (action == m_actionAlignCenter) a = Qt::AlignCenter;
    else if (action == m_actionAlignRight) a = Qt::AlignRight;
    else if (action == m_actionAlignJustify) a = Qt::AlignJustify;
    m_editor->setAlignment(a);
    m_actionAlignLeft->setChecked(action == m_actionAlignLeft);
    m_actionAlignCenter->setChecked(action == m_actionAlignCenter);
    m_actionAlignRight->setChecked(action == m_actionAlignRight);
    m_actionAlignJustify->setChecked(action == m_actionAlignJustify);
}

void NoteWidget::textStyle(int index)
{
    // index 0 = 正文, 1..6 = 标题层级
    const int level = index <= 0 ? -1 : index;
    QTextCursor cursor = m_editor->textCursor();

    cursor.beginEditBlock();
    QTextBlockFormat blockFmt = cursor.blockFormat();
    blockFmt.setHeadingLevel(level);
    cursor.mergeBlockFormat(blockFmt);

    // Word-like visual feedback: heading sizes/bold applied over the whole
    // block(s), so the outline and the canvas agree with what the user sees.
    QTextCursor full = cursor;
    full.setPosition(cursor.selectionStart());
    full.movePosition(QTextCursor::StartOfBlock);
    if (cursor.hasSelection()) {
        full.setPosition(cursor.selectionEnd());
        full.movePosition(QTextCursor::EndOfBlock);
    } else {
        full.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    QTextCharFormat charFmt;
    if (level > 0) {
        static const int sizes[] = {24, 20, 17, 15, 14, 13};
        charFmt.setFontPointSize(sizes[level - 1]);
        charFmt.setFontWeight(level <= 3 ? QFont::Bold : QFont::Normal);
    } else {
        charFmt.setFontPointSize(14);
        charFmt.setFontWeight(QFont::Normal);
    }
    full.mergeCharFormat(charFmt);
    cursor.endEditBlock();
}

void NoteWidget::fontChanged(const QFont &font)
{
    QTextCharFormat fmt;
    fmt.setFontFamilies(QStringList(font.family()));
    mergeFormatOnSelection(fmt);
}

void NoteWidget::fontSizeChanged(int)
{
    QTextCharFormat fmt;
    fmt.setFontPointSize(m_sizeCombo->currentText().toDouble());
    mergeFormatOnSelection(fmt);
}

void NoteWidget::insertImage()
{
    QString path = QFileDialog::getOpenFileName(this, "插入图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
    if (path.isEmpty()) return;
    QImage img(path);
    if (img.isNull()) return;
    if (img.width() > 600) img = img.scaledToWidth(600, Qt::SmoothTransformation);
    m_editor->document()->addResource(QTextDocument::ImageResource, QUrl(path), QVariant(img));
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertImage(path);
}

void NoteWidget::insertLink()
{
    bool ok;
    QString text = QInputDialog::getText(this, "插入链接", "链接文字：", QLineEdit::Normal, "", &ok);
    if (!ok || text.isEmpty()) return;
    QString url = QInputDialog::getText(this, "插入链接", "链接地址：", QLineEdit::Normal, "https://", &ok);
    if (!ok || url.isEmpty()) return;
    QTextCursor cursor = m_editor->textCursor();
    QTextCharFormat fmt;
    fmt.setAnchor(true);
    fmt.setAnchorHref(url);
    fmt.setForeground(QColor(0, 102, 204));
    fmt.setFontUnderline(true);
    cursor.insertText(text, fmt);
}

void NoteWidget::insertCodeBlock()
{
    QTextCursor cursor = m_editor->textCursor();
    QTextBlockFormat blockFmt;
    blockFmt.setBackground(QColor(245, 245, 245));
    blockFmt.setLeftMargin(20);
    blockFmt.setRightMargin(20);
    cursor.mergeBlockFormat(blockFmt);
    QTextCharFormat charFmt;
    charFmt.setFontFamilies(QStringList("Consolas"));
    charFmt.setFontPointSize(12);
    cursor.mergeCharFormat(charFmt);
}

void NoteWidget::insertQuote()
{
    QTextCursor cursor = m_editor->textCursor();
    QTextBlockFormat blockFmt;
    blockFmt.setLeftMargin(30);
    blockFmt.setBackground(QColor(240, 248, 255));
    cursor.mergeBlockFormat(blockFmt);
}

void NoteWidget::insertList(QAction *action)
{
    QTextCursor cursor = m_editor->textCursor();
    QTextListFormat listFmt;
    if (action == m_actionListBullet) listFmt.setStyle(QTextListFormat::ListDisc);
    else listFmt.setStyle(QTextListFormat::ListDecimal);
    cursor.createList(listFmt);
}

void NoteWidget::insertHR()
{
    m_editor->insertHtml("<hr>");
}

void NoteWidget::clearFormat()
{
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    QTextCharFormat defaultFmt;
    defaultFmt.setFontPointSize(14);
    defaultFmt.setFontWeight(QFont::Normal);
    defaultFmt.setFontItalic(false);
    defaultFmt.setFontUnderline(false);
    defaultFmt.setFontStrikeOut(false);
    defaultFmt.setForeground(Qt::black);
    defaultFmt.setAnchor(false);
    cursor.setCharFormat(defaultFmt);
    QTextBlockFormat blockFmt;
    blockFmt.setHeadingLevel(-1);
    blockFmt.setLeftMargin(0);
    blockFmt.setTextIndent(0);
    blockFmt.setLineHeight(100, QTextBlockFormat::SingleHeight);
    blockFmt.setBackground(Qt::transparent);
    cursor.mergeBlockFormat(blockFmt);
}

// ========== Word-style: character formatting ==========

void NoteWidget::textSuperScript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment(m_actionSuper->isChecked() ? QTextCharFormat::AlignSuperScript
                                                        : QTextCharFormat::AlignNormal);
    if (m_actionSuper->isChecked()) m_actionSub->setChecked(false);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::textSubScript()
{
    QTextCharFormat fmt;
    fmt.setVerticalAlignment(m_actionSub->isChecked() ? QTextCharFormat::AlignSubScript
                                                      : QTextCharFormat::AlignNormal);
    if (m_actionSub->isChecked()) m_actionSuper->setChecked(false);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::applyHighlightColor(const QColor &color)
{
    QTextCharFormat fmt;
    fmt.setBackground(color);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::clearHighlight()
{
    QTextCharFormat fmt;
    fmt.setBackground(Qt::transparent);
    mergeFormatOnSelection(fmt);
}

void NoteWidget::insertInlineCode()
{
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    const QString text = cursor.selectedText();
    if (text.isEmpty()) return;
    QTextCharFormat fmt;
    fmt.setFontFamilies({"Consolas"});
    fmt.setBackground(QColor(238, 238, 238));
    fmt.setForeground(QColor(0xC7, 0x25, 0x4E));
    cursor.insertText(text, fmt);
}

// ========== Word-style: format painter ==========

void NoteWidget::toggleFormatPainter()
{
    if (!m_actionFormatPainter->isChecked()) {
        deactivateFormatPainter();
        return;
    }
    // Capture the source format from the current selection (word under cursor
    // when nothing is selected).
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
    QTextCursor probe(m_editor->document());
    const int probePos = qMin(cursor.selectionStart() + 1, m_editor->document()->characterCount() - 1);
    probe.setPosition(qMax(0, probePos));
    m_painterCharFmt = probe.charFormat();
    m_painterBlockFmt = cursor.blockFormat();
    m_painterSticky = false;
    m_painterBtn->setToolTip("格式刷模式：选中文字即套用（Esc 退出）");
}

void NoteWidget::applyFormatPainter()
{
    QTextCursor cursor = m_editor->textCursor();
    if (!cursor.hasSelection()) return;
    cursor.beginEditBlock();
    QTextCharFormat cf = m_painterCharFmt;
    cf.setAnchor(false);
    cf.setAnchorHref(QString());
    cursor.mergeCharFormat(cf);
    cursor.mergeBlockFormat(m_painterBlockFmt);
    cursor.endEditBlock();
    if (!m_painterSticky)
        deactivateFormatPainter();
}

void NoteWidget::deactivateFormatPainter()
{
    m_painterSticky = false;
    m_actionFormatPainter->setChecked(false);
    m_painterBtn->setChecked(false);
    m_painterBtn->setToolTip("格式刷：单击刷一次，双击连续刷（Esc 退出）");
}

// ========== Word-style: paragraph formatting ==========

void NoteWidget::applyBlockFormatOnSelection(const std::function<void(QTextBlockFormat &)> &mutator)
{
    QTextCursor cursor = m_editor->textCursor();
    QTextBlockFormat fmt = cursor.blockFormat();
    mutator(fmt);
    cursor.mergeBlockFormat(fmt);
}

void NoteWidget::indentIncrease()
{
    applyBlockFormatOnSelection([](QTextBlockFormat &fmt) {
        fmt.setLeftMargin(fmt.leftMargin() + 36.0);
    });
}

void NoteWidget::indentDecrease()
{
    applyBlockFormatOnSelection([](QTextBlockFormat &fmt) {
        fmt.setLeftMargin(qMax<qreal>(0.0, fmt.leftMargin() - 36.0));
    });
}

void NoteWidget::lineSpacingChanged(int index)
{
    static const qreal mults[] = {100.0, 115.0, 150.0, 200.0, 250.0, 300.0};
    const qreal h = mults[qBound(0, index, 5)];
    applyBlockFormatOnSelection([h](QTextBlockFormat &fmt) {
        fmt.setLineHeight(h, QTextBlockFormat::ProportionalHeight);
    });
}

void NoteWidget::toggleFirstLineIndent()
{
    const bool on = m_actionFirstLineIndent->isChecked();
    applyBlockFormatOnSelection([this, on](QTextBlockFormat &fmt) {
        if (on) {
            // 首行缩进 2 个汉字宽（Word 中文排版习惯）
            const qreal charW = QFontMetricsF(m_editor->font()).horizontalAdvance(QChar(0x6C49));
            fmt.setTextIndent(charW * 2.0);
        } else {
            fmt.setTextIndent(0.0);
        }
    });
}

// ========== Task list (☐/☑ in-text markers; QTextBlockFormat::marker is
// lost by QTextDocument::toHtml, so text markers are the durable choice) ====

QChar NoteWidget::taskMarkerChar(bool checked)
{
    return checked ? QChar(0x2611) : QChar(0x2610);
}

bool NoteWidget::isTaskMarkerChar(QChar c)
{
    return c == QChar(0x2610) || c == QChar(0x2611);
}

void NoteWidget::insertTaskList()
{
    QTextCursor cursor = m_editor->textCursor();
    // Capture the covered block range before createList(): it may not keep
    // the selection, and every covered block needs its ☐ marker.
    const int from = cursor.selectionStart();
    const int to = cursor.hasSelection() ? cursor.selectionEnd() - 1 : from;
    cursor.beginEditBlock();
    QTextListFormat listFmt; // ListStyleUndefined → indent without bullets
    listFmt.setIndent(1);
    cursor.createList(listFmt);

    QTextBlock b = m_editor->document()->findBlock(from);
    QTextBlock end = m_editor->document()->findBlock(qMax(from, to));
    for (; b.isValid(); b = b.next()) {
        if (!b.text().isEmpty() && isTaskMarkerChar(b.text().at(0))) {
            if (b == end) break;
            continue;
        }
        QTextCursor c(b);
        c.insertText(QString(taskMarkerChar(false)) + " ");
        if (b == end) break;
    }
    cursor.endEditBlock();
}

void NoteWidget::toggleTaskMarker(int blockNumber)
{
    QTextBlock b = m_editor->document()->findBlockByNumber(blockNumber);
    if (!b.isValid() || b.text().isEmpty() || !isTaskMarkerChar(b.text().at(0))) return;
    const int pos = b.position();
    QTextCursor c(m_editor->document());
    c.setPosition(pos);
    c.setPosition(pos + 1, QTextCursor::KeepAnchor);
    c.insertText(QString(taskMarkerChar(b.text().at(0) == QChar(0x2610))));
}

// ========== Tables ==========

QTextTable *NoteWidget::currentTable() const
{
    return m_editor->textCursor().currentTable();
}

void NoteWidget::insertTableDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("插入表格");
    QFormLayout *form = new QFormLayout(&dlg);
    QSpinBox *rows = new QSpinBox;
    rows->setRange(1, 50);
    rows->setValue(3);
    QSpinBox *cols = new QSpinBox;
    cols->setRange(1, 20);
    cols->setValue(3);
    QCheckBox *header = new QCheckBox("包含表头行");
    header->setChecked(true);
    form->addRow("行数", rows);
    form->addRow("列数", cols);
    form->addRow(QString(), header);
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);
    if (dlg.exec() != QDialog::Accepted) return;
    insertTableWithParams(rows->value(), cols->value(), header->isChecked());
}

void NoteWidget::insertTableWithParams(int rows, int cols, bool headerRow)
{
    QTextCursor cursor = m_editor->textCursor();
    QTextTableFormat fmt;
    fmt.setBorder(1);
    fmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    fmt.setBorderBrush(QColor(0xD1, 0xD5, 0xDB));
    fmt.setCellPadding(6);
    fmt.setCellSpacing(0);
    fmt.setBorderCollapse(true);
    fmt.setAlignment(Qt::AlignLeft);
    if (headerRow) fmt.setHeaderRowCount(1);
    QTextTable *table = cursor.insertTable(qMax(1, rows), qMax(1, cols), fmt);
    if (table && headerRow) styleTableRow(table, 0, true);
    m_editor->setTextCursor(cursor); // move into the first cell (Word behavior)
}

void NoteWidget::styleTableRow(QTextTable *table, int row, bool headerStyle)
{
    if (!table) return;
    for (int cIdx = 0; cIdx < table->columns(); ++cIdx) {
        QTextTableCell cell = table->cellAt(row, cIdx);
        if (!cell.isValid()) continue;
        QTextCursor cellCur = cell.firstCursorPosition();
        QTextBlockFormat bf = cellCur.blockFormat();
        bf.setBackground(headerStyle ? QColor(0xF3, 0xF4, 0xF6) : Qt::transparent);
        cellCur.setBlockFormat(bf);
        QTextCharFormat cf;
        cf.setFontWeight(headerStyle ? QFont::Bold : QFont::Normal);
        // Style existing content and text typed later alike.
        const QTextBlock cellBlock = cell.firstCursorPosition().block();
        const int endPos = qMax(cell.firstPosition() + 1,
                                cellBlock.position() + cellBlock.length() - 1);
        cellCur.setPosition(endPos, QTextCursor::KeepAnchor);
        cellCur.mergeCharFormat(cf);
        cellCur.setPosition(cell.firstPosition());
        cellCur.setCharFormat(cf);
    }
}

void NoteWidget::tableInsertRowAbove()
{
    if (QTextTable *t = currentTable())
        t->insertRows(t->cellAt(m_editor->textCursor()).row(), 1);
}

void NoteWidget::tableInsertRowBelow()
{
    if (QTextTable *t = currentTable())
        t->insertRows(t->cellAt(m_editor->textCursor()).row() + 1, 1);
}

void NoteWidget::tableInsertColumnLeft()
{
    if (QTextTable *t = currentTable())
        t->insertColumns(t->cellAt(m_editor->textCursor()).column(), 1);
}

void NoteWidget::tableInsertColumnRight()
{
    if (QTextTable *t = currentTable())
        t->insertColumns(t->cellAt(m_editor->textCursor()).column() + 1, 1);
}

void NoteWidget::tableDeleteRow()
{
    if (QTextTable *t = currentTable()) {
        if (t->rows() <= 1) { QMessageBox::information(this, "提示", "表格至少需要保留一行"); return; }
        t->removeRows(t->cellAt(m_editor->textCursor()).row(), 1);
    }
}

void NoteWidget::tableDeleteColumn()
{
    if (QTextTable *t = currentTable()) {
        if (t->columns() <= 1) { QMessageBox::information(this, "提示", "表格至少需要保留一列"); return; }
        t->removeColumns(t->cellAt(m_editor->textCursor()).column(), 1);
    }
}

void NoteWidget::tableMergeCells()
{
    if (QTextTable *t = currentTable())
        t->mergeCells(m_editor->textCursor());
}

void NoteWidget::tableSplitCell()
{
    if (QTextTable *t = currentTable()) {
        const QTextTableCell cell = t->cellAt(m_editor->textCursor());
        if (cell.isValid())
            t->splitCell(cell.row(), cell.column(), 1, 2);
    }
}

void NoteWidget::tableToggleHeaderShading()
{
    if (QTextTable *t = currentTable()) {
        const QTextTableCell cell = t->cellAt(0, 0);
        if (!cell.isValid()) return;
        const QBrush bg = cell.firstCursorPosition().blockFormat().background();
        const bool shaded = bg.style() != Qt::NoBrush && bg.color().alpha() > 0;
        styleTableRow(t, 0, !shaded);
    }
}

void NoteWidget::showEditorContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.setStyleSheet(kDarkMenuSS);
    if (currentTable()) {
        menu.addAction("上方插入行", this, &NoteWidget::tableInsertRowAbove);
        menu.addAction("下方插入行", this, &NoteWidget::tableInsertRowBelow);
        menu.addAction("左侧插入列", this, &NoteWidget::tableInsertColumnLeft);
        menu.addAction("右侧插入列", this, &NoteWidget::tableInsertColumnRight);
        menu.addSeparator();
        menu.addAction("删除行", this, &NoteWidget::tableDeleteRow);
        menu.addAction("删除列", this, &NoteWidget::tableDeleteColumn);
        menu.addSeparator();
        menu.addAction("合并单元格", this, &NoteWidget::tableMergeCells);
        menu.addAction("拆分单元格", this, &NoteWidget::tableSplitCell);
        menu.addAction("表头底纹 开/关", this, &NoteWidget::tableToggleHeaderShading);
        menu.addSeparator();
    }
    menu.addAction("撤销", m_editor, &QTextEdit::undo);
    menu.addAction("重做", m_editor, &QTextEdit::redo);
    menu.addSeparator();
    menu.addAction("剪切", m_editor, &QTextEdit::cut);
    menu.addAction("复制", m_editor, &QTextEdit::copy);
    menu.addAction("粘贴", m_editor, &QTextEdit::paste);
    menu.addAction("全选", m_editor, &QTextEdit::selectAll);
    menu.addSeparator();
    menu.addAction("插入图片", this, &NoteWidget::insertImage);
    menu.addAction("插入表格", this, &NoteWidget::insertTableDialog);
    menu.addAction("查找/替换", this, [this]() { openFindBar(false); });
    menu.exec(m_editor->viewport()->mapToGlobal(pos));
}

// ========== Insert date & time ==========

void NoteWidget::insertDateTime(int formatId)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate d = now.date();
    const QTime t = now.time();
    static const QLocale cn(QLocale::Chinese, QLocale::China);
    QString text;
    switch (formatId) {
    case 1: text = QString("%1年%2月%3日").arg(d.year()).arg(d.month()).arg(d.day()); break;
    case 2: text = t.toString("HH:mm"); break;
    case 3: text = now.toString("yyyy-MM-dd HH:mm:ss"); break;
    case 4: text = cn.toString(d, "dddd"); break;
    default: text = d.toString("yyyy-MM-dd"); break;
    }
    m_editor->textCursor().insertText(text);
}

// ========== PDF export ==========

bool NoteWidget::exportPdfTo(const QString &path)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    m_editor->document()->print(&printer);
    return QFileInfo(path).exists() && QFileInfo(path).size() > 0;
}

void NoteWidget::exportPdfDialog()
{
    saveCurrentNote();
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [this](const NoteData &n) { return n.id == m_currentNoteId; });
    if (it == m_notes.end()) { QMessageBox::information(this, "提示", "请先打开一篇笔记"); return; }
    const QString title = it->title.isEmpty() ? "无标题笔记" : it->title;
    const QString fileName = QFileDialog::getSaveFileName(this, "导出 PDF", title + ".pdf", "PDF (*.pdf)");
    if (fileName.isEmpty()) return;
    if (exportPdfTo(fileName))
        QMessageBox::information(this, "导出完成", QString("已导出到：\n%1").arg(fileName));
    else
        QMessageBox::warning(this, "导出失败", "无法生成 PDF 文件：" + fileName);
}

// ========== Find & replace ==========

void NoteWidget::setupFindBar()
{
    m_findBar = new QWidget;
    m_findBar->setObjectName("noteFindBar");
    m_findBar->setStyleSheet(
        "QWidget#noteFindBar { background: #16202d; border-bottom: 1px solid #2a475e; }"
        "QWidget#noteFindBar QLabel { color: #c7d5e0; background: transparent; font-size: 12px; }"
        "QWidget#noteFindBar QLineEdit { background: #0f1720; color: #e6eef5; border: 1px solid #2a475e; border-radius: 4px; padding: 4px 8px; font-size: 13px; }"
        "QWidget#noteFindBar QLineEdit:focus { border-color: #66c0f4; }"
        "QWidget#noteFindBar QCheckBox { color: #c7d5e0; font-size: 12px; background: transparent; }"
        "QWidget#noteFindBar QToolButton { background: rgba(255,255,255,0.06); color: #c7d5e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 4px; padding: 4px 10px; font-size: 12px; }"
        "QWidget#noteFindBar QToolButton:hover { background: rgba(42,71,94,0.8); color: #fff; }");
    QVBoxLayout *v = new QVBoxLayout(m_findBar);
    v->setContentsMargins(12, 6, 12, 6);
    v->setSpacing(4);

    QHBoxLayout *row1 = new QHBoxLayout;
    row1->setSpacing(6);
    row1->addWidget(new QLabel("查找"));
    m_findEdit = new QLineEdit;
    m_findEdit->setObjectName("noteFindEdit");
    m_findEdit->setPlaceholderText("输入查找内容（Enter 下一个，Shift+Enter 上一个）");
    connect(m_findEdit, &QLineEdit::textChanged, this, &NoteWidget::onFindQueryChanged);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &NoteWidget::findNext);
    row1->addWidget(m_findEdit, 1);
    m_findCountLabel = new QLabel("0/0");
    row1->addWidget(m_findCountLabel);
    auto mkBtn = [](const QString &txt, const QString &tip) {
        QToolButton *b = new QToolButton;
        b->setText(txt);
        b->setToolTip(tip);
        return b;
    };
    QToolButton *prevBtn = mkBtn("上一个", "上一个匹配（Shift+Enter）");
    connect(prevBtn, &QToolButton::clicked, this, &NoteWidget::findPrev);
    QToolButton *nextBtn = mkBtn("下一个", "下一个匹配（Enter）");
    connect(nextBtn, &QToolButton::clicked, this, &NoteWidget::findNext);
    QToolButton *closeBtn = mkBtn("✕", "关闭（Esc）");
    connect(closeBtn, &QToolButton::clicked, this, &NoteWidget::closeFindBar);
    row1->addWidget(prevBtn);
    row1->addWidget(nextBtn);
    row1->addWidget(closeBtn);
    v->addLayout(row1);

    QHBoxLayout *row2 = new QHBoxLayout;
    row2->setSpacing(6);
    row2->addWidget(new QLabel("替换"));
    m_replaceEdit = new QLineEdit;
    m_replaceEdit->setObjectName("noteReplaceEdit");
    m_replaceEdit->setPlaceholderText("替换为");
    row2->addWidget(m_replaceEdit, 1);
    m_findCaseCheck = new QCheckBox("区分大小写");
    connect(m_findCaseCheck, &QCheckBox::toggled, this, [this](bool) { recomputeFindMatches(); });
    row2->addWidget(m_findCaseCheck);
    QToolButton *repBtn = mkBtn("替换", "替换当前匹配");
    connect(repBtn, &QToolButton::clicked, this, &NoteWidget::findReplaceOne);
    QToolButton *repAllBtn = mkBtn("全部替换", "替换全部匹配");
    connect(repAllBtn, &QToolButton::clicked, this, &NoteWidget::findReplaceAll);
    row2->addWidget(repBtn);
    row2->addWidget(repAllBtn);
    v->addLayout(row2);

    // Keep matches in sync while the bar is open and the document changes.
    connect(m_editor->document(), &QTextDocument::contentsChange, this, [this](int, int, int) {
        if (m_findBar && m_findBar->isVisible())
            recomputeFindMatches();
    });
}

void NoteWidget::openFindBar(bool withReplace)
{
    if (!m_findBar) return;
    m_findBar->show();
    if (withReplace && m_replaceEdit)
        m_replaceEdit->setFocus();
    else if (m_findEdit)
        m_findEdit->setFocus();
    recomputeFindMatches();
}

void NoteWidget::closeFindBar()
{
    if (m_findBar) m_findBar->hide();
    m_findMatches.clear();
    m_findIndex = -1;
    m_editor->setExtraSelections({});
    m_editor->setFocus();
}

void NoteWidget::recomputeFindMatches()
{
    m_findMatches.clear();
    m_findIndex = -1;
    const QString doc = m_editor->toPlainText();
    const QString needle = m_findEdit ? m_findEdit->text() : QString();
    if (!needle.isEmpty() && m_findCaseCheck) {
        const Qt::CaseSensitivity cs = m_findCaseCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        int from = 0;
        while (true) {
            const int idx = doc.indexOf(needle, from, cs);
            if (idx < 0) break;
            m_findMatches.append({idx, needle.length()});
            from = idx + needle.length();
        }
    }
    updateFindHighlights();
    if (m_findCountLabel)
        m_findCountLabel->setText(m_findMatches.isEmpty() ? "0/0"
                                : QString("%1/%2").arg(m_findIndex + 1).arg(m_findMatches.size()));
}

void NoteWidget::updateFindHighlights()
{
    QList<QTextEdit::ExtraSelection> sels;
    const QColor allMatch(255, 236, 100, 140);
    for (int i = 0; i < m_findMatches.size(); ++i) {
        QTextEdit::ExtraSelection es;
        es.cursor = QTextCursor(m_editor->document());
        es.cursor.setPosition(m_findMatches[i].first);
        es.cursor.setPosition(m_findMatches[i].first + m_findMatches[i].second, QTextCursor::KeepAnchor);
        es.format.setBackground(i == m_findIndex ? QColor(255, 150, 40) : allMatch);
        sels.append(es);
    }
    m_editor->setExtraSelections(sels);
    if (m_findIndex >= 0 && m_findIndex < m_findMatches.size()) {
        QTextCursor c(m_editor->document());
        c.setPosition(m_findMatches[m_findIndex].first);
        c.setPosition(m_findMatches[m_findIndex].first + m_findMatches[m_findIndex].second, QTextCursor::KeepAnchor);
        m_editor->setTextCursor(c);
    }
    if (m_findCountLabel)
        m_findCountLabel->setText(m_findMatches.isEmpty() ? "0/0"
                                : QString("%1/%2").arg(m_findIndex + 1).arg(m_findMatches.size()));
}

void NoteWidget::findNext()
{
    if (m_findMatches.isEmpty()) {
        recomputeFindMatches();
        if (m_findMatches.isEmpty()) return;
    }
    m_findIndex = (m_findIndex + 1) % m_findMatches.size();
    updateFindHighlights();
}

void NoteWidget::findPrev()
{
    if (m_findMatches.isEmpty()) {
        recomputeFindMatches();
        if (m_findMatches.isEmpty()) return;
    }
    m_findIndex = (m_findIndex - 1 + m_findMatches.size()) % m_findMatches.size();
    updateFindHighlights();
}

void NoteWidget::findReplaceOne()
{
    if (m_findIndex < 0 || m_findIndex >= m_findMatches.size()) {
        findNext();
        if (m_findIndex < 0 || m_findIndex >= m_findMatches.size()) return;
    }
    const auto match = m_findMatches[m_findIndex];
    QTextCursor c(m_editor->document());
    c.setPosition(match.first);
    c.setPosition(match.first + match.second, QTextCursor::KeepAnchor);
    c.insertText(m_replaceEdit->text());
    recomputeFindMatches();
}

void NoteWidget::findReplaceAll()
{
    const QString needle = m_findEdit->text();
    if (needle.isEmpty() || m_findMatches.isEmpty()) return;
    const QString rep = m_replaceEdit->text();
    QTextCursor c(m_editor->document());
    c.beginEditBlock();
    // Back-to-front so earlier positions stay valid while replacing.
    for (int i = m_findMatches.size() - 1; i >= 0; --i) {
        c.setPosition(m_findMatches[i].first);
        c.setPosition(m_findMatches[i].first + m_findMatches[i].second, QTextCursor::KeepAnchor);
        c.insertText(rep);
    }
    c.endEditBlock();
    recomputeFindMatches();
}

void NoteWidget::onFindQueryChanged()
{
    recomputeFindMatches();
}

// ========== Wiki links ([[...]], Logseq-style) ==========

void NoteWidget::insertWikiLinkDialog()
{
    bool ok = false;
    const QString title = QInputDialog::getText(this, "插入双链", "目标笔记标题：", QLineEdit::Normal, QString(), &ok);
    if (!ok || title.trimmed().isEmpty()) return;
    const QString t = title.trimmed();
    QTextCursor cursor = m_editor->textCursor();
    QTextCharFormat fmt;
    fmt.setAnchor(true);
    fmt.setAnchorHref("wikilink://" + t);
    fmt.setForeground(QColor(0x2F, 0xA6, 0x6A));
    fmt.setFontUnderline(true);
    cursor.insertText("[[" + t + "]]", fmt);
}

// ========== Editor mouse / keyboard interception ==========

bool NoteWidget::handleEditorMousePress(const QPointF &pos)
{
    const QPoint p = pos.toPoint();
    const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();

    // Ctrl+click on a [[wiki-link]] opens (or creates) the target note.
    if (mods & Qt::ControlModifier) {
        const QTextCursor cur = m_editor->cursorForPosition(p);
        const QString href = cur.charFormat().anchorHref();
        if (href.startsWith("wikilink://")) {
            openOrCreateByTitle(href.mid(11));
            return true;
        }
        return false;
    }

    // Click on a task-list checkbox toggles it.
    const QTextCursor cur = m_editor->cursorForPosition(p);
    const QTextBlock b = cur.block();
    if (!b.isValid() || b.text().isEmpty() || !isTaskMarkerChar(b.text().at(0)))
        return false;
    const int bp = b.position();
    QTextCursor cL(m_editor->document());
    cL.setPosition(bp);
    QTextCursor cR(m_editor->document());
    cR.setPosition(bp + 1);
    const QRect rl = m_editor->cursorRect(cL);
    const QRect rr = m_editor->cursorRect(cR);
    const QRect area(qMin(rl.left(), rr.left()) - 4, rl.top(),
                     qAbs(rr.left() - rl.left()) + 10, rl.height());
    if (area.contains(p)) {
        toggleTaskMarker(b.blockNumber());
        return true;
    }
    return false;
}

bool NoteWidget::handleEditorKeyPress(QKeyEvent *event)
{
    const int key = event->key();
    const int mods = event->modifiers();

    if (key == Qt::Key_Escape) {
        if (m_findBar && m_findBar->isVisible()) {
            closeFindBar();
            return true;
        }
        if (m_painterSticky || m_actionFormatPainter->isChecked()) {
            deactivateFormatPainter();
            return true;
        }
    }
    // Ctrl+Enter toggles the checkbox of the current task item.
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && (mods & Qt::ControlModifier)) {
        const QTextBlock b = m_editor->textCursor().block();
        if (b.isValid() && !b.text().isEmpty() && isTaskMarkerChar(b.text().at(0))) {
            toggleTaskMarker(b.blockNumber());
            return true;
        }
    }
    return false;
}

// ========== Data persistence ==========

void NoteWidget::loadNotes()
{
    QDir().mkpath(m_libraryRoot);
    QString path = m_libraryRoot + "/notes.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // First run: seed default folders (工作/学习/生活, same as the old hardcoded set)
        m_folders = {"工作", "学习", "生活"};
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    // New format: { "notes": [...], "folders": [...] }
    if (doc.isObject()) {
        const QJsonArray foldersArr = doc.object()["folders"].toArray();
        m_folders.clear();
        for (const QJsonValue &v : foldersArr)
            m_folders.append(v.toString());
        if (m_folders.isEmpty()) m_folders = {"工作", "学习", "生活"};
        loadNotesFromArray(doc.object()["notes"].toArray());
        refreshFolderList();
        return;
    }
    // Legacy format: plain array of notes
    m_folders = {"工作", "学习", "生活"};
    loadNotesFromArray(doc.array());
    refreshFolderList();
}

void NoteWidget::loadNotesFromArray(const QJsonArray &arr)
{
    m_notes.clear();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        NoteData note;
        note.id = obj["id"].toString();
        note.title = obj["title"].toString();
        note.content = obj["content"].toString();
        note.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        note.updatedAt = QDateTime::fromString(obj["updatedAt"].toString(), Qt::ISODate);
        note.starred = obj["starred"].toBool(false);
        note.folder = obj["folder"].toString("");
        m_notes.append(note);
    }
}

void NoteWidget::saveNotes()
{
    QDir().mkpath(m_libraryRoot);
    QString path = m_libraryRoot + "/notes.json";
    QJsonArray arr;
    for (const NoteData &note : m_notes) {
        QJsonObject obj;
        obj["id"] = note.id;
        obj["title"] = note.title;
        obj["content"] = note.content;
        obj["createdAt"] = note.createdAt.toString(Qt::ISODate);
        obj["updatedAt"] = note.updatedAt.toString(Qt::ISODate);
        obj["starred"] = note.starred;
        obj["folder"] = note.folder;
        arr.append(obj);
    }
    QJsonObject root;
    root["notes"] = arr;
    QJsonArray foldersArr;
    for (const QString &f : m_folders) foldersArr.append(f);
    root["folders"] = foldersArr;
    // Atomic write (temp file + rename) so a crash can't truncate notes.json.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(root).toJson());
    file.commit();
}

void NoteWidget::saveCurrentNote()
{
    if (m_currentNoteId.isEmpty() || m_loading) return;
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [this](const NoteData &n) { return n.id == m_currentNoteId; });
    if (it != m_notes.end()) {
        it->title = m_titleEdit->text();
        it->content = m_editor->toHtml();
        it->updatedAt = QDateTime::currentDateTime();
    }
    saveNotes();
    m_modified = false;
}

void NoteWidget::updateStatusBar()
{
    QString text = m_editor->toPlainText();
    int chars = text.length();
    int words = text.isEmpty() ? 0 : text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    m_statusLabel->setText(QString("字数：%1  词数：%2").arg(chars).arg(words));
}

QString NoteWidget::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString NoteWidget::htmlToPreview(const QString &html, int maxLen) const
{
    QString text = QTextDocumentFragment::fromHtml(html).toPlainText();
    text = text.replace("\n", " ").trimmed();
    if (text.length() > maxLen) text = text.left(maxLen) + "...";
    return text;
}

QString NoteWidget::toHtml() const { return m_editor ? m_editor->toHtml() : ""; }
void NoteWidget::setHtml(const QString &html) { if (m_editor) m_editor->setHtml(html); }
QString NoteWidget::toPlainText() const { return m_editor ? m_editor->toPlainText() : ""; }
QString NoteWidget::title() const { return m_titleEdit ? m_titleEdit->text() : ""; }
void NoteWidget::setModified(bool modified) { m_modified = modified; }

void NoteWidget::setBgColor(const QColor &c)
{
    m_bgColor = c;
    if (!m_editor) return;
    m_editor->setStyleSheet(QString(R"(
        QTextEdit {
            background-color: %1;
            color: #333333;
            border: none;
            padding: 24px 60px;
            font-size: 14px;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            selection-background-color: #cfe8ff;
        }
    )").arg(c.name()));
}

void NoteWidget::setBackgroundImage(const QString &path)
{
    if (!m_stack) return;
    if (path == m_bgPath) return;
    m_bgPath = path;

    if (!path.isEmpty() && QFile::exists(path)) {
        // Pre-scale to reduce repaint cost during page switches
        QPixmap original(path);
        if (!original.isNull()) {
            m_bgPixmap = original.scaled(1600, 900, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QString tempPath = QDir::tempPath() + "/lyy_note_bg.png";
            m_bgPixmap.save(tempPath, "PNG");
            const QString imgUrl = QDir::toNativeSeparators(tempPath).replace("\\", "/");
            m_stack->setStyleSheet(QString(
                "QStackedWidget { border-image: url(%1) 0 0 0 0 stretch stretch; }"
            ).arg(imgUrl));
            // Also cover the whole page (sidebar area) so transparent panels reveal the image
            setStyleSheet(QString(
                "QWidget#noteRoot { border-image: url(%1) 0 0 0 0 stretch stretch; }"
            ).arg(imgUrl));
            setObjectName("noteRoot");
        }
    } else {
        m_bgPixmap = QPixmap();
        m_stack->setStyleSheet("background: transparent;");
        setStyleSheet("");
        setObjectName("noteRoot");
    }
}

void NoteWidget::setPanelsTransparent(bool transparent)
{
    m_panelsTransparent = transparent;

    // Left sidebar
    if (m_sidebar) {
        m_sidebar->setStyleSheet(transparent
            ? "background: transparent; border-right: 1px solid #333;"
            : "background: #1a1a1a; border-right: 1px solid #333;");
    }

    // Editor page background (keep custom editor bg image if user set one)
    if (m_editorPage && m_editorBgPath.isEmpty()) {
        m_editorPage->setStyleSheet(transparent
            ? "QWidget#noteEditorPage { background: transparent; }"
            : "QWidget#noteEditorPage { background: #1a1a1a; }");
    }

    // Editor top bar
    if (m_editorTopBar) {
        m_editorTopBar->setStyleSheet(transparent
            ? "background: transparent; border-bottom: 1px solid #2a475e;"
            : "background: #16202d; border-bottom: 1px solid #2a475e;");
    }

    // Format toolbar
    if (m_toolbar) {
        QString s = m_toolbar->styleSheet();
        if (transparent)
            s.replace("QToolBar { background: #1b2838;", "QToolBar { background: transparent;");
        else
            s.replace("QToolBar { background: transparent;", "QToolBar { background: #1b2838;");
        m_toolbar->setStyleSheet(s);
    }

    // Status label
    if (m_statusLabel) {
        m_statusLabel->setStyleSheet(transparent
            ? "background: transparent; color: #8f98a0; font-size: 12px; padding: 0 16px;"
            : "background: #16202d; color: #8f98a0; font-size: 12px; padding: 0 16px; border-top: 1px solid #2a475e;");
    }
}

void NoteWidget::changeEditorBackground()
{
    QString path = QFileDialog::getOpenFileName(this, "选择背景图片", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
    if (!path.isEmpty())
        setEditorBackground(path);
}

void NoteWidget::clearEditorBackground()
{
    setEditorBackground(QString());
}

void NoteWidget::setEditorBackground(const QString &path)
{
    if (!m_editorPage) return;
    m_editorBgPath = path;
    // Persist so it survives app restart
    QSettings settings("PureRefCanvas", "里与由");
    if (!path.isEmpty())
        settings.setValue("note/editorBg", path);
    else
        settings.remove("note/editorBg");

    // Decode-check (not just existence): an undecodable image (WebP without
    // the qwebp plugin) would set a background-image that never paints,
    // leaving the page transparent over the dark page-stack.
    if (!path.isEmpty() && QFile::exists(path) && !QImage(path).isNull()) {
        // background-size: cover keeps aspect ratio (no stretching), unlike border-image
        m_editorPage->setStyleSheet(QString(
            "QWidget#noteEditorPage {"
            " background-image: url(%1);"
            " background-position: center;"
            " background-repeat: no-repeat;"
            " background-size: cover;"
            " }"
        ).arg(QDir::toNativeSeparators(path).replace("\\", "/")));
        // Make editor semi-transparent so the background image shows through
        m_editor->setStyleSheet(m_editor->styleSheet().replace(
            "rgba(255,255,255,0.95)", "rgba(255,255,255,0.82)"));
    } else {
        m_editorPage->setStyleSheet("QWidget#noteEditorPage { background: #1a1a1a; }");
        // Restore opaque editor
        m_editor->setStyleSheet(m_editor->styleSheet().replace(
            "rgba(255,255,255,0.82)", "rgba(255,255,255,0.95)"));
    }
}

void NoteWidget::hideSidebar()
{
    if (m_sidebar) {
        if (m_sidebar->isVisible() && m_sidebar->width() > 0)
            m_sidebarWidth = m_sidebar->width();  // remember for toggleSidebar()
        m_sidebar->setMaximumWidth(0);
        m_sidebar->setMinimumWidth(0);
        m_sidebar->hide();    }
}

void NoteWidget::toggleSidebar()
{
    if (!m_sidebar) return;
    if (m_sidebar->isVisible()) {
        hideSidebar();
    } else {
        m_sidebar->setMinimumWidth(m_sidebarWidth);
        m_sidebar->setMaximumWidth(m_sidebarWidth);
        m_sidebar->show();
    }
}

bool NoteWidget::eventFilter(QObject *obj, QEvent *event)
{
    // Format painter double-click → sticky (continuous) mode
    if (obj == m_painterBtn && event->type() == QEvent::MouseButtonDblClick) {
        m_painterSticky = true;
        m_actionFormatPainter->setChecked(true);
        QTextCursor cursor = m_editor->textCursor();
        if (!cursor.hasSelection()) cursor.select(QTextCursor::WordUnderCursor);
        QTextCursor probe(m_editor->document());
        const int probePos = qMin(cursor.selectionStart() + 1, m_editor->document()->characterCount() - 1);
        probe.setPosition(qMax(0, probePos));
        m_painterCharFmt = probe.charFormat();
        m_painterBlockFmt = cursor.blockFormat();
        m_painterBtn->setToolTip("连续格式刷：选中文字即套用（Esc 退出）");
        return true;
    }

    // Ctrl+F inside the editor opens the find bar instead of canvas fit
    if (obj == m_editor && event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_F && ke->modifiers() == Qt::ControlModifier) {
            ke->accept();
            return true;
        }
    }
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_F && ke->modifiers() == Qt::ControlModifier) {
            openFindBar(false);
            return true;
        }
        if (handleEditorKeyPress(ke)) return true;
    }

    // Task checkbox clicks & ctrl-clicked wiki links
    if (obj == m_editor->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (handleEditorMousePress(me->position())) return true;
    }

    // Card click detection
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *w = qobject_cast<QWidget*>(obj);
        if (w && w->property("noteId").isValid()) {
            QString id = w->property("noteId").toString();
            if (!id.isEmpty()) {
                openNote(id);
                return true;
            }
        }
    }

    // Keep empty-state hint sized to the grid viewport
    if (obj == m_gridScroll->viewport() && event->type() == QEvent::Resize) {
        if (m_emptyLabel && m_emptyLabel->isVisible())
            m_emptyLabel->setGeometry(m_gridScroll->viewport()->rect());
    }

    // Image drag-drop into editor
    if (obj == m_editor && event->type() == QEvent::DragEnter) {
        QDragEnterEvent *e = static_cast<QDragEnterEvent*>(event);
        if (e->mimeData()->hasUrls()) { e->acceptProposedAction(); return true; }
    }
    if (obj == m_editor && event->type() == QEvent::Drop) {
        QDropEvent *e = static_cast<QDropEvent*>(event);
        if (e->mimeData()->hasUrls()) {
            for (const QUrl &url : e->mimeData()->urls()) {
                QString path = url.toLocalFile();
                QImage img(path);
                if (!img.isNull()) {
                    if (img.width() > 600) img = img.scaledToWidth(600, Qt::SmoothTransformation);
                    m_editor->document()->addResource(QTextDocument::ImageResource, QUrl(path), QVariant(img));
                    QTextCursor cursor = m_editor->cursorForPosition(e->position().toPoint());
                    cursor.insertImage(path);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void NoteWidget::resizeEvent(QResizeEvent *)
{
    if (m_stack->currentIndex() == 0) refreshCards();
}
