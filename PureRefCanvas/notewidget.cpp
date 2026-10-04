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
#include <algorithm>

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
    m_sidebar->setStyleSheet("background: #1a1a1a; border-right: 1px solid #333;");
    setupSidebar();
    mainLayout->addWidget(m_sidebar);

    // Right stack: card grid or editor
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("background: transparent;");

    // Card grid page
    m_gridScroll = new QScrollArea;
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setFrameShape(QFrame::NoFrame);
    m_gridScroll->setStyleSheet("QScrollArea { background: transparent; } QScrollBar:vertical { background: transparent; width: 8px; } QScrollBar::handle:vertical { background: #555; border-radius: 4px; min-height: 30px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");
    m_gridWidget = new QWidget;
    m_gridWidget->setStyleSheet("background: transparent;");
    m_gridLayout = new QGridLayout(m_gridWidget);
    m_gridLayout->setContentsMargins(20, 20, 20, 20);
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
    m_exportBtn->setToolTip("导出当前笔记为 Markdown");
    m_exportBtn->setStyleSheet("QPushButton { background: rgba(255,255,255,0.06); color: #c7d5e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 15px; padding: 2px 12px; font-size: 13px; } QPushButton:hover { background: rgba(42,71,94,0.8); color: #fff; border-color: #66c0f4; }");
    connect(m_exportBtn, &QPushButton::clicked, this, &NoteWidget::exportCurrentNote);
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
    connect(m_editor, &QTextEdit::textChanged, this, &NoteWidget::onTextChanged);
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &NoteWidget::onCursorPositionChanged);

    // Toolbar
    setupToolbar();
    editorLayout->addWidget(m_toolbar);

    // Editor container with margins for card effect
    QWidget *editorContainer = new QWidget;
    editorContainer->setObjectName("noteEditorContainer");
    editorContainer->setStyleSheet("QWidget#noteEditorContainer { background: transparent; }");
    QHBoxLayout *containerLayout = new QHBoxLayout(editorContainer);
    containerLayout->setContentsMargins(40, 16, 40, 16);
    containerLayout->addWidget(m_editor);
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
    sideLayout->setContentsMargins(12, 16, 12, 12);
    sideLayout->setSpacing(10);

    // Search
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("搜索笔记");
    m_searchEdit->setFixedHeight(34);
    m_searchEdit->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.06); color: #e0e0e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 17px; padding: 4px 14px; font-size: 13px; } QLineEdit:focus { border-color: #66c0f4; background: rgba(42,71,94,0.4); }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &NoteWidget::onSearchChanged);
    sideLayout->addWidget(m_searchEdit);

    // New button
    m_newNoteBtn = new QPushButton("新建笔记");
    m_newNoteBtn->setFixedHeight(36);
    m_newNoteBtn->setCursor(Qt::PointingHandCursor);
    m_newNoteBtn->setStyleSheet("QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2a475e, stop:1 #3a576e); color: #eaf2f8; border: none; border-radius: 18px; font-size: 14px; font-weight: 600; } QPushButton:hover { background: #3a576e; color: #fff; }");
    connect(m_newNoteBtn, &QPushButton::clicked, this, &NoteWidget::newNote);
    sideLayout->addWidget(m_newNoteBtn);

    // Category list
    m_categoryList = new QListWidget;
    m_categoryList->setFixedHeight(120);
    m_categoryList->setStyleSheet("QListWidget { background: transparent; border: none; outline: none; } QListWidget::item { color: #aab4bd; padding: 8px 14px; border-radius: 8px; font-size: 13px; margin: 1px 2px; } QListWidget::item:selected { background: rgba(42,71,94,0.85); color: #66c0f4; font-weight: 600; } QListWidget::item:hover:!selected { background: rgba(255,255,255,0.06); color: #e0e0e0; }");
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
    folderLabel->setStyleSheet("color: #6b7885; font-size: 12px; font-weight: 600; padding: 8px 12px 4px 12px;");
    sideLayout->addWidget(folderLabel);

    // Folder list (自定义文件夹：右键新建/重命名/删除)
    m_folderList = new QListWidget;
    m_folderList->setStyleSheet("QListWidget { background: transparent; border: none; outline: none; } QListWidget::item { color: #aab4bd; padding: 7px 14px; border-radius: 8px; font-size: 13px; margin: 1px 2px; } QListWidget::item:selected { background: rgba(42,71,94,0.85); color: #66c0f4; font-weight: 600; } QListWidget::item:hover:!selected { background: rgba(255,255,255,0.06); color: #e0e0e0; }");
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

    // 新建文件夹 quick button (text-link style)
    QPushButton *newFolderBtn = new QPushButton("新建文件夹");
    newFolderBtn->setFixedHeight(28);
    newFolderBtn->setCursor(Qt::PointingHandCursor);
    newFolderBtn->setStyleSheet("QPushButton { background: transparent; color: #66c0f4; border: none; border-radius: 14px; font-size: 12px; } QPushButton:hover { background: rgba(42,71,94,0.7); color: #fff; }");
    connect(newFolderBtn, &QPushButton::clicked, this, &NoteWidget::newFolder);
    sideLayout->addWidget(newFolderBtn);

    // Sort combo (主流平台标准：自定义排序)
    QHBoxLayout *sortRow = new QHBoxLayout;
    sortRow->setContentsMargins(4, 4, 4, 0);
    QLabel *sortLabel = new QLabel("排序");
    sortLabel->setStyleSheet("color: #6b7885; font-size: 12px;");
    m_sortCombo = new QComboBox;
    m_sortCombo->addItems({"最近更新", "最近创建", "标题 A-Z", "收藏优先"});
    m_sortCombo->setFixedHeight(28);
    m_sortCombo->setStyleSheet("QComboBox { background: rgba(255,255,255,0.06); color: #e0e0e0; border: 1px solid rgba(255,255,255,0.14); border-radius: 14px; padding: 2px 10px; font-size: 12px; } QComboBox:hover { border-color: #66c0f4; } QComboBox QAbstractItemView { background: #1b2838; color: #e0e0e0; selection-background-color: #2a475e; selection-color: #66c0f4; outline: none; border: 1px solid #2a475e; }");
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::onSortChanged);
    sortRow->addWidget(sortLabel);
    sortRow->addWidget(m_sortCombo, 1);
    sideLayout->addLayout(sortRow);

    // Export all
    m_exportAllBtn = new QPushButton("导出全部笔记");
    m_exportAllBtn->setFixedHeight(34);
    m_exportAllBtn->setCursor(Qt::PointingHandCursor);
    m_exportAllBtn->setToolTip("将所有笔记导出为一份 Markdown 文件（按文件夹分组）");
    m_exportAllBtn->setStyleSheet("QPushButton { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2a475e, stop:1 #3a576e); color: #eaf2f8; border: none; border-radius: 17px; font-size: 13px; font-weight: 600; } QPushButton:hover { background: #3a576e; color: #fff; }");
    connect(m_exportAllBtn, &QPushButton::clicked, this, &NoteWidget::exportAllNotes);
    sideLayout->addWidget(m_exportAllBtn);
}

void NoteWidget::setupToolbar()
{
    m_toolbar = new QToolBar;
    m_toolbar->setFixedHeight(42);
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet("QToolBar { background: #1b2838; border-bottom: 1px solid #2a475e; spacing: 2px; padding: 3px 8px; } QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 7px; font-size: 13px; } QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; } QToolButton:checked { background: #2a475e; color: #66c0f4; } QComboBox { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 4px; padding: 2px 8px; font-size: 12px; min-width: 60px; } QComboBox:hover { border-color: #66c0f4; } QComboBox QAbstractItemView { background: #16202d; color: #c7d5e0; selection-background-color: #2a475e; selection-color: #66c0f4; outline: none; }");

    m_actionUndo = m_toolbar->addAction("撤销");
    m_actionUndo->setToolTip("撤销");
    connect(m_actionUndo, &QAction::triggered, m_editor, &QTextEdit::undo);

    m_actionRedo = m_toolbar->addAction("重做");
    m_actionRedo->setToolTip("重做");
    connect(m_actionRedo, &QAction::triggered, m_editor, &QTextEdit::redo);

    m_toolbar->addSeparator();

    m_styleCombo = new QComboBox;
    m_styleCombo->addItems({"正文", "标题1", "标题2", "标题3"});
    m_styleCombo->setFixedHeight(26);
    connect(m_styleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::textStyle);
    m_toolbar->addWidget(m_styleCombo);

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setFixedHeight(26);
    m_fontCombo->setFixedWidth(120);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &NoteWidget::fontChanged);
    m_toolbar->addWidget(m_fontCombo);

    m_sizeCombo = new QComboBox;
    m_sizeCombo->addItems({"10", "11", "12", "13", "14", "15", "16", "18", "20", "24", "28", "32"});
    m_sizeCombo->setCurrentText("14");
    m_sizeCombo->setFixedHeight(26);
    m_sizeCombo->setFixedWidth(56);
    connect(m_sizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NoteWidget::fontSizeChanged);
    m_toolbar->addWidget(m_sizeCombo);

    m_toolbar->addSeparator();

    m_actionBold = m_toolbar->addAction("B");
    m_actionBold->setToolTip("加粗");
    QFont boldFont = m_actionBold->font(); boldFont.setBold(true);
    m_actionBold->setFont(boldFont);
    connect(m_actionBold, &QAction::triggered, this, &NoteWidget::textBold);

    m_actionItalic = m_toolbar->addAction("I");
    m_actionItalic->setToolTip("斜体");
    QFont italicFont = m_actionItalic->font(); italicFont.setItalic(true);
    m_actionItalic->setFont(italicFont);
    connect(m_actionItalic, &QAction::triggered, this, &NoteWidget::textItalic);

    m_actionUnderline = m_toolbar->addAction("U");
    m_actionUnderline->setToolTip("下划线");
    QFont ulFont = m_actionUnderline->font(); ulFont.setUnderline(true);
    m_actionUnderline->setFont(ulFont);
    connect(m_actionUnderline, &QAction::triggered, this, &NoteWidget::textUnderline);

    m_actionStrike = m_toolbar->addAction("S");
    m_actionStrike->setToolTip("删除线");
    QFont strikeFont = m_actionStrike->font(); strikeFont.setStrikeOut(true);
    m_actionStrike->setFont(strikeFont);
    connect(m_actionStrike, &QAction::triggered, this, &NoteWidget::textStrike);

    m_toolbar->addSeparator();

    m_actionColor = m_toolbar->addAction("A");
    m_actionColor->setToolTip("文字颜色");
    connect(m_actionColor, &QAction::triggered, this, &NoteWidget::textColor);

    m_actionClearFormat = m_toolbar->addAction("清除格式");
    m_actionClearFormat->setToolTip("清除格式");
    connect(m_actionClearFormat, &QAction::triggered, this, &NoteWidget::clearFormat);

    m_toolbar->addSeparator();

    m_actionAlignLeft = m_toolbar->addAction("左对齐");
    m_actionAlignLeft->setToolTip("左对齐");
    m_actionAlignLeft->setCheckable(true);
    connect(m_actionAlignLeft, &QAction::triggered, this, [this]() { textAlign(m_actionAlignLeft); });

    m_actionAlignCenter = m_toolbar->addAction("居中");
    m_actionAlignCenter->setToolTip("居中");
    m_actionAlignCenter->setCheckable(true);
    connect(m_actionAlignCenter, &QAction::triggered, this, [this]() { textAlign(m_actionAlignCenter); });

    m_actionAlignRight = m_toolbar->addAction("右对齐");
    m_actionAlignRight->setToolTip("右对齐");
    m_actionAlignRight->setCheckable(true);
    connect(m_actionAlignRight, &QAction::triggered, this, [this]() { textAlign(m_actionAlignRight); });

    m_toolbar->addSeparator();

    m_actionListBullet = m_toolbar->addAction("无序列表");
    m_actionListBullet->setToolTip("无序列表");
    connect(m_actionListBullet, &QAction::triggered, this, [this]() { insertList(m_actionListBullet); });

    m_actionListNumber = m_toolbar->addAction("有序列表");
    m_actionListNumber->setToolTip("有序列表");
    connect(m_actionListNumber, &QAction::triggered, this, [this]() { insertList(m_actionListNumber); });

    m_actionQuote = m_toolbar->addAction("引用");
    connect(m_actionQuote, &QAction::triggered, this, &NoteWidget::insertQuote);

    m_actionCode = m_toolbar->addAction("代码");
    connect(m_actionCode, &QAction::triggered, this, &NoteWidget::insertCodeBlock);

    m_actionHR = m_toolbar->addAction("分割线");
    connect(m_actionHR, &QAction::triggered, this, &NoteWidget::insertHR);

    m_toolbar->addSeparator();

    m_actionImage = m_toolbar->addAction("图片");
    connect(m_actionImage, &QAction::triggered, this, &NoteWidget::insertImage);

    m_actionLink = m_toolbar->addAction("链接");
    connect(m_actionLink, &QAction::triggered, this, &NoteWidget::insertLink);

    m_toolbar->addSeparator();

    // Background image button with menu
    QToolButton *bgBtn = new QToolButton;
    bgBtn->setText("背景");
    bgBtn->setToolTip("设置编辑器背景");
    bgBtn->setPopupMode(QToolButton::InstantPopup);
    bgBtn->setStyleSheet("QToolButton { background: transparent; color: #c7d5e0; border: none; border-radius: 4px; padding: 4px 8px; font-size: 13px; } QToolButton:hover { background: rgba(42,71,94,0.7); color: #fff; } QToolButton::menu-indicator { image: none; }");
    QMenu *bgMenu = new QMenu(bgBtn);
    bgMenu->setStyleSheet("QMenu { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; } QMenu::item { padding: 8px 20px; } QMenu::item:selected { background: #2a475e; color: #66c0f4; }");
    bgMenu->addAction("选择背景图片...", this, &NoteWidget::changeEditorBackground);
    bgMenu->addAction("恢复默认背景", this, &NoteWidget::clearEditorBackground);
    bgBtn->setMenu(bgMenu);
    m_toolbar->addWidget(bgBtn);
}

QWidget *NoteWidget::createCard(const NoteData &note)
{
    QWidget *card = new QWidget;
    card->setFixedWidth(240);
    card->setMinimumHeight(160);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(R"(
        QWidget { background: rgba(40,40,40,0.9); border-radius: 10px; border: 1px solid rgba(255,255,255,0.05); }
        QWidget:hover { background: rgba(55,55,55,0.95); border: 1px solid #66c0f4; }
    )");

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 12);
    cardLayout->setSpacing(8);

    // Title
    QLabel *titleLabel = new QLabel(note.title.isEmpty() ? "无标题" : note.title);
    titleLabel->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: 600; background: transparent;");
    titleLabel->setWordWrap(true);
    titleLabel->setMaximumHeight(40);
    cardLayout->addWidget(titleLabel);

    // Preview
    QString preview = htmlToPreview(note.content);
    QLabel *previewLabel = new QLabel(preview);
    previewLabel->setStyleSheet("color: #999; font-size: 12px; background: transparent;");
    previewLabel->setWordWrap(true);
    previewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    cardLayout->addWidget(previewLabel, 1);

    // Bottom: starred tag + date
    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);
    if (note.starred) {
        QLabel *starTag = new QLabel("已收藏");
        starTag->setStyleSheet("background: rgba(255,193,7,0.15); color: #ffc107; font-size: 11px; padding: 2px 8px; border-radius: 8px;");
        bottomLayout->addWidget(starTag);
    }
    bottomLayout->addStretch();
    QLabel *dateLabel = new QLabel(note.updatedAt.toString("MM-dd"));
    dateLabel->setStyleSheet("color: #6b7885; font-size: 11px; background: transparent;");
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
            "<div style='color:#8f98a0; font-size:14px; font-weight:600; margin-bottom:6px;'>还没有笔记</div>"
            "<div style='color:#6b7885; font-size:12px;'>点击「新建笔记」开始记录，<br>或把文字、图片拖进编辑器</div>");
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
        QString line;
        const int h = blk.blockFormat().headingLevel();
        if (h > 0) line = QString(h, '#') + " ";
        if (blk.textList()) {
            const QTextListFormat lf = blk.textList()->format();
            line = (lf.style() == QTextListFormat::ListDecimal) ? "1. " : "- ";
        }
        for (QTextBlock::iterator it = blk.begin(); !it.atEnd(); ++it) {
            const QTextFragment frag = it.fragment();
            if (frag.text().isEmpty()) continue;
            QString t = frag.text();
            const QTextCharFormat fmt = frag.charFormat();
            const QString href = fmt.anchorHref();
            if (!href.isEmpty()) t = QString("[%1](%2)").arg(t, href);
            if (fmt.fontWeight() == QFont::Bold) t = "**" + t + "**";
            if (fmt.fontItalic()) t = "*" + t + "*";
            if (fmt.fontStrikeOut()) t = "~~" + t + "~~";
            line += t;
        }
        if (!line.trimmed().isEmpty())
            md += line + "\n";
    }
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
}

void NoteWidget::onCursorPositionChanged()
{
    QTextCharFormat fmt = m_editor->currentCharFormat();
    m_actionBold->setChecked(fmt.fontWeight() == QFont::Bold);
    m_actionItalic->setChecked(fmt.fontItalic());
    m_actionUnderline->setChecked(fmt.fontUnderline());
    m_actionStrike->setChecked(fmt.fontStrikeOut());
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
    m_actionAlignLeft->setChecked(action == m_actionAlignLeft);
    m_actionAlignCenter->setChecked(action == m_actionAlignCenter);
    m_actionAlignRight->setChecked(action == m_actionAlignRight);
    if (action == m_actionAlignLeft) m_editor->setAlignment(Qt::AlignLeft);
    else if (action == m_actionAlignCenter) m_editor->setAlignment(Qt::AlignCenter);
    else if (action == m_actionAlignRight) m_editor->setAlignment(Qt::AlignRight);
}

void NoteWidget::textStyle(int index)
{
    QTextCursor cursor = m_editor->textCursor();
    QTextBlockFormat blockFmt = cursor.blockFormat();
    if (index == 0) blockFmt.setHeadingLevel(-1);
    else blockFmt.setHeadingLevel(index);
    cursor.mergeBlockFormat(blockFmt);
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
    blockFmt.setBackground(Qt::transparent);
    cursor.mergeBlockFormat(blockFmt);
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
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
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
    file.write(QJsonDocument(root).toJson());
    file.close();
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

    if (!path.isEmpty() && QFile::exists(path)) {
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
        m_sidebar->setMaximumWidth(0);
        m_sidebar->setMinimumWidth(0);
        m_sidebar->hide();    }
}

bool NoteWidget::eventFilter(QObject *obj, QEvent *event)
{
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
