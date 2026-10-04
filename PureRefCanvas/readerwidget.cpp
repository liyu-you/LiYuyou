#include "readerwidget.h"
#include "booklibrary.h"

#include <QStackedWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QColorDialog>
#include <QMenu>
#include <QDialog>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QAbstractTextDocumentLayout>
#include <QScrollBar>
#include <QDir>
#include <QPalette>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPixmap>
#include <QImage>
#include <QIcon>
#include <QLinearGradient>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QFontMetricsF>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QAbstractItemView>
#include <QtMath>

// Custom text-format property id: carries an annotation id on highlighted text.
static const int kAnnProp = 640000001;

namespace {
constexpr int kShelfAuthorRole = Qt::UserRole + 1;
constexpr int kShelfProgressRole = Qt::UserRole + 2;
constexpr int kShelfAnnotationCountRole = Qt::UserRole + 3;
constexpr int kShelfNoteCountRole = Qt::UserRole + 4;
constexpr int kShelfTotalCharsRole = Qt::UserRole + 5;
constexpr int kShelfChapterRole = Qt::UserRole + 6;
constexpr int kShelfLastReadRole = Qt::UserRole + 7;
constexpr int kShelfCoverColorRole = Qt::UserRole + 8;
constexpr int kShelfCoverImageRole = Qt::UserRole + 9;

class ShelfBookDelegate : public QStyledItemDelegate
{
public:
    explicit ShelfBookDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(360, 156);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRect card = option.rect.adjusted(4, 4, -4, -8);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        painter->setPen(QPen(QColor(selected ? "#2FA66A" : "#E5E7EB"), selected ? 1.2 : 0.8));
        painter->setBrush(QColor(selected ? "#F0FBF5" : (hovered ? "#FAFBFC" : "#FFFFFF")));
        painter->drawRoundedRect(card, 16, 16);

        const QRect coverRect(card.left() + 20, card.top() + 22, 60, 92);
        {
            // Custom cover image if user picked one; otherwise gradient + vertical title.
            const QString coverImg = index.data(kShelfCoverImageRole).toString();
            if (!coverImg.isEmpty() && QFile::exists(coverImg)) {
                QPixmap cover(coverImg);
                if (!cover.isNull()) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(QColor("#E5E7EB"));
                    painter->drawRoundedRect(coverRect, 6, 6);
                    painter->drawPixmap(coverRect, cover.scaled(coverRect.size(),
                        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                }
            } else {
                QStringList colors = index.data(kShelfCoverColorRole).toString().split(",");
                QLinearGradient cg(coverRect.topLeft(), coverRect.bottomRight());
                if (colors.size() >= 2) {
                    cg.setColorAt(0, QColor(colors[0]));
                    cg.setColorAt(1, QColor(colors[1]));
                } else {
                    cg.setColorAt(0, QColor("#667eea"));
                    cg.setColorAt(1, QColor("#764ba2"));
                }
                painter->setPen(Qt::NoPen);
                painter->setBrush(cg);
                painter->drawRoundedRect(coverRect, 6, 6);

                QFont coverFont("Microsoft YaHei", 14, QFont::Bold);
                painter->setFont(coverFont);
                painter->setPen(QColor(255, 255, 255, 235));
                QStringList lines;
                for (int i = 0; i < index.data(Qt::DisplayRole).toString().size(); i += 2)
                    lines << index.data(Qt::DisplayRole).toString().mid(i, 2);
                painter->drawText(coverRect.adjusted(2, 8, -2, -8), Qt::AlignCenter | Qt::TextWordWrap,
                                  lines.join("\n"));
            }
        }

        const int textX = card.left() + 100;
        const int right = card.right() - 20;
        const int pillW = 80;
        const int titleRight = right - pillW - 16;

        QFont titleFont("Microsoft YaHei", 11, QFont::Medium);
        painter->setFont(titleFont);
        painter->setPen(QColor("#111827"));
        QFontMetrics titleFm(titleFont);
        painter->drawText(QRect(textX, card.top() + 16, qMax(80, titleRight - textX), 22),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          titleFm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, qMax(80, titleRight - textX)));

        QFont bodyFont("Microsoft YaHei", 9);
        painter->setFont(bodyFont);
        painter->setPen(QColor("#9CA3AF"));
        const QString author = index.data(kShelfAuthorRole).toString().isEmpty()
            ? QStringLiteral("未知作者") : index.data(kShelfAuthorRole).toString();
        QFontMetrics bodyFm(bodyFont);
        painter->drawText(QRect(textX, card.top() + 42, titleRight - textX, 18),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          bodyFm.elidedText(author + QStringLiteral(" · 古典文学"), Qt::ElideRight, qMax(80, titleRight - textX)));

        const QString chapter = index.data(kShelfChapterRole).toString();
        if (!chapter.isEmpty()) {
            painter->setPen(QColor("#6B7280"));
            painter->drawText(QRect(textX, card.top() + 62, titleRight - textX, 18),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              bodyFm.elidedText(chapter, Qt::ElideRight, qMax(80, titleRight - textX)));
        }

        const QString lastRead = index.data(kShelfLastReadRole).toString();
        if (!lastRead.isEmpty()) {
            painter->setPen(QColor("#9CA3AF"));
            QFont smallFont("Microsoft YaHei", 8);
            painter->setFont(smallFont);
            QFontMetrics smallFm(smallFont);
            painter->drawText(QRect(textX, card.top() + 82, titleRight - textX, 16),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              smallFm.elidedText(lastRead, Qt::ElideRight, qMax(80, titleRight - textX)));
        }

        painter->setFont(bodyFont);

        const int total = index.data(kShelfTotalCharsRole).toInt();
        const int progress = index.data(kShelfProgressRole).toInt();
        const int pct = total > 0 ? qBound(0, progress * 100 / total, 100) : 0;
        const int barW = qMin(184, qMax(80, titleRight - textX));
        const QRect barBg(textX, card.top() + 106, barW, 6);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#E5E7EB"));
        painter->drawRoundedRect(barBg, 3, 3);
        QRect barFill = barBg;
        barFill.setWidth(qMax(4, barBg.width() * pct / 100));
        painter->setBrush(QColor("#2FA66A"));
        painter->drawRoundedRect(barFill, 3, 3);

        painter->setPen(QColor("#9CA3AF"));
        painter->drawText(QRect(textX, card.top() + 122, 140, 18), Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("阅读进度 %1%").arg(pct));

        const int badgeCount = index.data(kShelfAnnotationCountRole).toInt() + index.data(kShelfNoteCountRole).toInt();
        if (badgeCount > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#FAEEDA"));
            QRect badge(textX + 110, card.top() + 122, 64, 18);
            painter->drawRoundedRect(badge, 9, 9);
            painter->setPen(QColor("#854F0B"));
            painter->drawText(badge, Qt::AlignCenter, QStringLiteral("笔记 %1").arg(badgeCount));
        }

        // Three-dot menu (top-right of card)
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#D1D5DB"));
        const int menuY = card.top() + 12;
        painter->drawEllipse(QPoint(right - 14, menuY), 2, 2);
        painter->drawEllipse(QPoint(right - 7, menuY), 2, 2);
        painter->drawEllipse(QPoint(right, menuY), 2, 2);

        // Continue pill (right side, vertically centered with text block)
        const QRect action(right - pillW, card.top() + 50, pillW, 32);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#EAF3DE"));
        painter->drawRoundedRect(action, 16, 16);
        QFont actionFont("Microsoft YaHei", 9, QFont::Medium);
        painter->setFont(actionFont);
        painter->setPen(QColor("#27500A"));
        painter->drawText(action, Qt::AlignCenter, QStringLiteral("继续阅读"));

        painter->restore();
    }
};
}

ReaderWidget::ReaderWidget(const QString &libraryRoot, QWidget *parent)
    : QWidget(parent), m_libraryRoot(libraryRoot)
{
    m_library = new BookLibrary(libraryRoot, this);

    setupUI();

    // 1s reading-time clock; started only when a book opens, stopped on back-to-shelf.
    m_readClock = new QTimer(this);
    m_readClock->setInterval(1000);
    connect(m_readClock, &QTimer::timeout, this, &ReaderWidget::onReadTick);

    connect(m_library, &BookLibrary::libraryChanged, this, &ReaderWidget::refreshShelf);

    // Defer heavy loading — staggered to avoid main window freeze on show
    QTimer::singleShot(250, this, [this]() {
        m_library->load();
        refreshShelf();
    });
}

void ReaderWidget::setupUI()
{
    m_stack = new QStackedWidget(this);
    setupShelfPage();
    setupReaderPage();
    m_stack->addWidget(m_shelfPage);
    m_stack->addWidget(m_readerPage);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_stack);
}

void ReaderWidget::setupShelfPage()
{
    m_shelfPage = new QWidget;
    m_shelfPage->setObjectName("readerShelfPage");
    m_shelfPage->setStyleSheet(R"(
        QWidget#readerShelfPage { background: #F5F7FA; }
        QLabel { background: transparent; }
    )");

    // Horizontal: [reader sidebar | content area]
    m_shelfLayout = new QHBoxLayout(m_shelfPage);
    m_shelfLayout->setContentsMargins(0, 0, 0, 0);
    m_shelfLayout->setSpacing(0);

    setupReadingSidebar();

    // --- Content area ---
    m_shelfContentArea = new QWidget;
    m_shelfContentArea->setObjectName("readerShelfContent");
    m_shelfContentArea->setStyleSheet("background: transparent;");
    QVBoxLayout *layout = new QVBoxLayout(m_shelfContentArea);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(16);

    // Header: title/count on the left, import button on the right.
    QHBoxLayout *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(12);

    QVBoxLayout *titleBlock = new QVBoxLayout;
    titleBlock->setContentsMargins(0, 0, 0, 0);
    titleBlock->setSpacing(2);
    m_shelfTitle = new QLabel("书架");
    m_shelfTitle->setStyleSheet("font-size: 22px; font-weight: 500; color: #111827;");
    m_shelfCountLabel = new QLabel("0 本书");
    m_shelfCountLabel->setStyleSheet("font-size: 13px; color: #6B7280;");
    titleBlock->addWidget(m_shelfTitle);
    titleBlock->addWidget(m_shelfCountLabel);

    m_importBtn = new QPushButton("导入书籍");
    m_importBtn->setCursor(Qt::PointingHandCursor);
    m_importBtn->setFixedSize(112, 36);
    m_importBtn->setStyleSheet(R"(
        QPushButton {
            background: #2FA66A;
            color: #FFFFFF;
            border: none;
            border-radius: 18px;
            padding: 0 18px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover { background: #27945D; }
        QPushButton:pressed { background: #1F7A4B; }
    )");
    connect(m_importBtn, &QPushButton::clicked, this, &ReaderWidget::importBook);

    header->addLayout(titleBlock);
    header->addStretch();
    header->addWidget(m_importBtn, 0, Qt::AlignTop);
    layout->addLayout(header);

    // Shelf list: each book is drawn as a full-width modern card by ShelfBookDelegate.
    m_shelfGrid = new QListWidget;
    m_shelfGrid->setObjectName("readerShelfGrid");
    m_shelfGrid->setViewMode(QListView::ListMode);
    m_shelfGrid->setResizeMode(QListView::Adjust);
    m_shelfGrid->setMovement(QListView::Static);
    m_shelfGrid->setSpacing(0);
    m_shelfGrid->setUniformItemSizes(true);
    m_shelfGrid->setContextMenuPolicy(Qt::CustomContextMenu);
    m_shelfGrid->setItemDelegate(new ShelfBookDelegate(m_shelfGrid));
    m_shelfGrid->setStyleSheet(R"(
        QListWidget#readerShelfGrid {
            background: transparent;
            border: none;
            outline: none;
            padding: 0;
        }
        QListWidget#readerShelfGrid::item {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #D1D5DB;
            border-radius: 5px;
            min-height: 32px;
            margin: 2px;
        }
        QScrollBar::handle:vertical:hover { background: #9CA3AF; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
    connect(m_shelfGrid, &QListWidget::itemDoubleClicked, this, &ReaderWidget::openBook);
    connect(m_shelfGrid, &QListWidget::customContextMenuRequested, this, &ReaderWidget::onShelfItemMenu);
    layout->addWidget(m_shelfGrid, 1);

    // --- Bottom: quick-import card + reading-record card ---
    QHBoxLayout *quickRow = new QHBoxLayout;
    quickRow->setContentsMargins(0, 0, 0, 0);
    quickRow->setSpacing(16);

    // Reading-timer card — today / cumulative reading stats
    QWidget *timerCard = new QWidget;
    timerCard->setFixedHeight(132);
    timerCard->setStyleSheet("background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 16px;");
    {
        QHBoxLayout *row = new QHBoxLayout(timerCard);
        row->setContentsMargins(20, 18, 20, 18);
        row->setSpacing(16);

        QLabel *icon = new QLabel("时");
        icon->setFixedSize(36, 36);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("background: #EAF3DE; color: #3F7A2E; border-radius: 18px; font-size: 14px; font-weight: 500;");
        row->addWidget(icon, 0, Qt::AlignTop);

        QVBoxLayout *texts = new QVBoxLayout;
        texts->setContentsMargins(0, 0, 0, 0);
        texts->setSpacing(6);
        QLabel *t = new QLabel("阅读计时");
        t->setStyleSheet("font-size: 14px; font-weight: 500; color: #111827;");
        m_timerTodayLabel = new QLabel("今日阅读 0 分钟");
        m_timerTodayLabel->setStyleSheet("font-size: 12px; color: #6B7280;");
        m_timerTotalLabel = new QLabel("累计阅读 0 小时");
        m_timerTotalLabel->setStyleSheet("font-size: 12px; color: #6B7280;");
        QLabel *badge = new QLabel("继续阅读");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(86, 24);
        badge->setStyleSheet("background: #EAF3DE; color: #3F7A2E; border-radius: 12px; font-size: 12px; font-weight: 500;");
        texts->addWidget(t);
        texts->addWidget(m_timerTodayLabel);
        texts->addWidget(m_timerTotalLabel);
        texts->addWidget(badge, 0, Qt::AlignLeft);
        row->addLayout(texts, 1);
    }
    timerCard->installEventFilter(this);
    timerCard->setProperty("readerTimerCard", true);
    quickRow->addWidget(timerCard, 1);

    // Reading record card — book / chapter + segmented progress pill badge
    QWidget *recordCard = new QWidget;
    recordCard->setFixedHeight(132);
    recordCard->setStyleSheet("background: #FFFFFF; border: 1px solid #E5E7EB; border-radius: 16px;");
    {
        QHBoxLayout *row = new QHBoxLayout(recordCard);
        row->setContentsMargins(20, 18, 20, 18);
        row->setSpacing(16);

        QLabel *icon = new QLabel("≡");
        icon->setFixedSize(36, 36);
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("background: #FAEEDA; color: #854F0B; border-radius: 18px; font-size: 20px; font-weight: 500;");
        row->addWidget(icon, 0, Qt::AlignTop);

        QVBoxLayout *texts = new QVBoxLayout;
        texts->setContentsMargins(0, 0, 0, 0);
        texts->setSpacing(6);
        QLabel *t = new QLabel("阅读记录");
        t->setStyleSheet("font-size: 14px; font-weight: 500; color: #111827;");
        m_recordBookLabel = new QLabel("暂无最近书目");
        m_recordBookLabel->setStyleSheet("font-size: 12px; color: #111827; font-weight: 500;");
        m_recordChapterLabel = new QLabel("导入书籍后开始阅读");
        m_recordChapterLabel->setStyleSheet("font-size: 12px; color: #6B7280;");

        // Progress pill: amber background, dark fill segment, centered text overlay.
        m_recordBadge = new QLabel("暂无记录");
        m_recordBadge->setAlignment(Qt::AlignCenter);
        m_recordBadge->setFixedSize(96, 24);
        m_recordBadge->setStyleSheet("background: #FAEEDA; color: #854F0B; border-radius: 12px; font-size: 12px; font-weight: 500;");

        texts->addWidget(t);
        texts->addWidget(m_recordBookLabel);
        texts->addWidget(m_recordChapterLabel);
        texts->addWidget(m_recordBadge, 0, Qt::AlignLeft);
        row->addLayout(texts, 1);
    }
    quickRow->addWidget(recordCard, 1);
    layout->addLayout(quickRow);

    m_shelfLayout->addWidget(m_readerSidebar, 0);
    m_shelfLayout->addWidget(m_shelfContentArea, 1);
}

void ReaderWidget::setupReadingSidebar()
{
    m_readerSidebar = new QWidget;
    m_readerSidebar->setObjectName("readerSidebar");
    m_readerSidebar->setFixedWidth(220);
    m_readerSidebar->setStyleSheet(R"(
        QWidget#readerSidebar { background: #FFFFFF; border-right: 1px solid #E5E7EB; }
        QLabel { background: transparent; }
    )");

    QVBoxLayout *sb = new QVBoxLayout(m_readerSidebar);
    sb->setContentsMargins(16, 20, 16, 20);
    sb->setSpacing(18);

    // Section: content browser header
    QLabel *browserLabel = new QLabel("内容浏览器");
    browserLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #6B7280;");
    sb->addWidget(browserLabel);

    // Active "书架" row
    QWidget *activeRow = new QWidget;
    activeRow->setStyleSheet("background: #EAF3DE; border-radius: 10px;");
    QHBoxLayout *activeLayout = new QHBoxLayout(activeRow);
    activeLayout->setContentsMargins(12, 8, 8, 8);
    activeLayout->setSpacing(8);
    QLabel *shelfIcon = new QLabel("▮");
    shelfIcon->setStyleSheet("color: #27500A; font-size: 14px;");
    QLabel *shelfText = new QLabel("书架");
    shelfText->setStyleSheet("color: #27500A; font-size: 14px; font-weight: 500;");
    activeLayout->addWidget(shelfIcon);
    activeLayout->addWidget(shelfText);
    activeLayout->addStretch();
    QLabel *chevron = new QLabel("›");
    chevron->setStyleSheet("color: #27500A; font-size: 16px;");
    activeLayout->addWidget(chevron);
    sb->addWidget(activeRow);

    // Stats row: book count + note count (single-line, compact)
    QHBoxLayout *statsRow = new QHBoxLayout;
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(12);
    {
        QWidget *bookStat = new QWidget;
        bookStat->setStyleSheet("background: #F3F4F6; border-radius: 10px;");
        QVBoxLayout *bl = new QVBoxLayout(bookStat);
        bl->setContentsMargins(12, 12, 12, 12);
        bl->setSpacing(0);
        m_sidebarBookCount = new QLabel("0 本书");
        m_sidebarBookCount->setAlignment(Qt::AlignCenter);
        m_sidebarBookCount->setStyleSheet("font-size: 14px; font-weight: 600; color: #111827;");
        bl->addWidget(m_sidebarBookCount);
        statsRow->addWidget(bookStat, 1);

        QWidget *noteStat = new QWidget;
        noteStat->setStyleSheet("background: #F3F4F6; border-radius: 10px;");
        QVBoxLayout *nl = new QVBoxLayout(noteStat);
        nl->setContentsMargins(12, 12, 12, 12);
        nl->setSpacing(0);
        m_sidebarNoteCount = new QLabel("0 条记录");
        m_sidebarNoteCount->setAlignment(Qt::AlignCenter);
        m_sidebarNoteCount->setStyleSheet("font-size: 14px; font-weight: 600; color: #111827;");
        nl->addWidget(m_sidebarNoteCount);
        statsRow->addWidget(noteStat, 1);
    }
    sb->addLayout(statsRow);

    // Section: recent import
    QLabel *recentLabel = new QLabel("最近导入");
    recentLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #6B7280;");
    sb->addWidget(recentLabel);

    QWidget *recentItem = new QWidget;
    recentItem->setStyleSheet("background: #F9FAFB; border-radius: 10px;");
    QVBoxLayout *rl = new QVBoxLayout(recentItem);
    rl->setContentsMargins(12, 10, 12, 10);
    rl->setSpacing(2);
    m_sidebarRecentImportTitle = new QLabel("暂无导入");
    m_sidebarRecentImportTitle->setStyleSheet("font-size: 13px; font-weight: 500; color: #111827;");
    m_sidebarRecentImportTime = new QLabel("导入书籍以查看");
    m_sidebarRecentImportTime->setStyleSheet("font-size: 11px; color: #9CA3AF;");
    rl->addWidget(m_sidebarRecentImportTitle);
    rl->addWidget(m_sidebarRecentImportTime);
    sb->addWidget(recentItem);

    sb->addStretch();

    // New group button
    m_sidebarNewGroupBtn = new QPushButton("+ 新建分组");
    m_sidebarNewGroupBtn->setCursor(Qt::PointingHandCursor);
    m_sidebarNewGroupBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #6B7280;
            border: 1px dashed #D1D5DB;
            border-radius: 10px;
            padding: 10px;
            font-size: 13px;
        }
        QPushButton:hover { border-color: #2FA66A; color: #2FA66A; }
    )");
    sb->addWidget(m_sidebarNewGroupBtn);
}

void ReaderWidget::setupReaderPage()
{
    m_readerPage = new QWidget;

    m_readerLayout = new QVBoxLayout(m_readerPage);
    m_readerLayout->setContentsMargins(0, 0, 0, 0);
    m_readerLayout->setSpacing(0);

    // Top bar
    m_topBar = new QWidget;
    m_topBar->setFixedHeight(40);
    QHBoxLayout *topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(12, 0, 12, 0);

    m_backBtn = new QPushButton("← 书架");
    m_backBtn->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 14px; padding: 6px 10px; border-radius: 4px; } QPushButton:hover { background: rgba(0,0,0,0.08); }");
    connect(m_backBtn, &QPushButton::clicked, this, &ReaderWidget::backToShelf);

    m_bookTitleLabel = new QLabel;
    m_bookTitleLabel->setStyleSheet("font-size: 15px; font-weight: 500;");
    m_bookTitleLabel->setAlignment(Qt::AlignCenter);

    m_tocBtn = new QPushButton("目录");
    m_tocBtn->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 14px; padding: 6px 10px; border-radius: 4px; } QPushButton:hover { background: rgba(0,0,0,0.08); }");
    connect(m_tocBtn, &QPushButton::clicked, this, &ReaderWidget::showToc);

    topLayout->addWidget(m_backBtn);
    topLayout->addWidget(m_bookTitleLabel, 1);
    topLayout->addWidget(m_tocBtn);

    // Reading view — selectable text so highlights/notes are possible
    m_readingView = new QTextEdit;
    m_readingView->setReadOnly(true);
    m_readingView->setFrameShape(QFrame::NoFrame);
    m_readingView->setLineWrapMode(QTextEdit::WidgetWidth);
    m_readingView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_readingView->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_readingView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_readingView->document()->setDocumentMargin(12);
    m_readingView->document()->setDefaultFont(readerFont());
    m_readingView->setCursor(Qt::IBeamCursor);
    m_readingView->setUndoRedoEnabled(false); // whole book lives in one doc — no undo history
    connect(m_readingView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { updateChrome(); });
    connect(m_readingView, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showSelectionMenu(m_readingView->viewport()->mapToGlobal(pos));
    });

    m_chapterLabel = new QLabel;
    m_chapterLabel->setStyleSheet("font-size: 13px; color: #999; padding: 0 40px;");
    m_chapterLabel->setAlignment(Qt::AlignCenter);

    // Wrap the text view in a container that provides the page side margins
    // (QTextEdit::setViewportMargins is protected, so margins live in the layout).
    // The container doubles as a translucent "paper card" when a custom background
    // image is active, so text stays readable over any image.
    QWidget *textContainer = new QWidget;
    m_textCard = textContainer;
    m_textLayout = new QVBoxLayout(textContainer);
    m_textLayout->setContentsMargins(m_sideMargin, 4, m_sideMargin, 8);
    m_textLayout->setSpacing(0);
    m_textLayout->addWidget(m_readingView);

    QWidget *readingContainer = new QWidget;
    QVBoxLayout *readingLayout = new QVBoxLayout(readingContainer);
    readingLayout->setContentsMargins(0, 10, 0, 10);
    readingLayout->setSpacing(8);
    readingLayout->addWidget(m_chapterLabel);
    readingLayout->addWidget(textContainer, 1);

    // Annotation / notes side panel
    m_annoPanel = new QWidget;
    m_annoPanel->setFixedWidth(300);
    m_annoPanel->setStyleSheet("background: #faf6ea; border-left: 1px solid rgba(0,0,0,0.08);");
    QVBoxLayout *panelLayout = new QVBoxLayout(m_annoPanel);
    panelLayout->setContentsMargins(10, 12, 10, 12);
    panelLayout->setSpacing(8);
    m_annoHeader = new QLabel("标注");
    m_annoHeader->setStyleSheet("font-size: 15px; font-weight: bold; color: #5b4636;");
    panelLayout->addWidget(m_annoHeader);
    m_annoList = new QListWidget;
    m_annoList->setObjectName("annoList");
    m_annoList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { background: white; border-radius: 6px; padding: 8px; margin-bottom: 6px; font-size: 12px; color: #444; border: 1px solid rgba(0,0,0,0.06); }
        QListWidget::item:hover { border-color: #2ea043; }
    )");
    m_annoList->setWordWrap(true);
    m_annoList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_annoList, &QListWidget::itemDoubleClicked, this, &ReaderWidget::onAnnotationActivated);
    connect(m_annoList, &QListWidget::customContextMenuRequested, this, &ReaderWidget::onAnnotationListMenu);
    panelLayout->addWidget(m_annoList, 1);
    QLabel *tip = new QLabel("划线后在文字上点右键或松开鼠标即可标注");
    tip->setWordWrap(true);
    tip->setStyleSheet("font-size: 11px; color: #998a75;");
    panelLayout->addWidget(tip);
    m_annoPanel->hide();

    // Reading notes side panel (free-form notes anchored to positions in the text)
    m_notePanel = new QWidget;
    m_notePanel->setObjectName("notePanel");
    m_notePanel->setFixedWidth(300);
    m_notePanel->setStyleSheet("background: #faf6ea; border-left: 1px solid rgba(0,0,0,0.08);");
    QVBoxLayout *noteLayout = new QVBoxLayout(m_notePanel);
    noteLayout->setContentsMargins(10, 12, 10, 12);
    noteLayout->setSpacing(8);
    m_noteHeader = new QLabel("笔记");
    m_noteHeader->setStyleSheet("font-size: 15px; font-weight: bold; color: #5b4636;");
    noteLayout->addWidget(m_noteHeader);
    m_noteList = new QListWidget;
    m_noteList->setObjectName("noteList");
    m_noteList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; }
        QListWidget::item { background: white; border-radius: 6px; padding: 8px; margin-bottom: 6px; font-size: 12px; color: #444; border: 1px solid rgba(0,0,0,0.06); }
        QListWidget::item:hover { border-color: #2ea043; }
    )");
    m_noteList->setWordWrap(true);
    m_noteList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_noteList, &QListWidget::itemDoubleClicked, this, &ReaderWidget::onNoteActivated);
    connect(m_noteList, &QListWidget::customContextMenuRequested, this, &ReaderWidget::onNoteListMenu);
    noteLayout->addWidget(m_noteList, 1);
    m_writeNoteBtn = new QPushButton("＋ 写笔记");
    m_writeNoteBtn->setStyleSheet("QPushButton { background: #2ea043; color: white; border: none; border-radius: 6px; padding: 8px 0; font-size: 13px; } QPushButton:hover { background: #3cb353; }");
    connect(m_writeNoteBtn, &QPushButton::clicked, this, &ReaderWidget::writeNote);
    noteLayout->addWidget(m_writeNoteBtn);
    QLabel *noteTip = new QLabel("记录阅读时的想法，笔记按位置收藏，双击条目可跳回原文");
    noteTip->setWordWrap(true);
    noteTip->setStyleSheet("font-size: 11px; color: #998a75;");
    noteLayout->addWidget(noteTip);
    m_notePanel->hide();

    QHBoxLayout *contentRow = new QHBoxLayout;
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(0);
    contentRow->addWidget(readingContainer, 1);
    contentRow->addWidget(m_annoPanel);
    contentRow->addWidget(m_notePanel);

    // Bottom bar
    m_bottomBar = new QWidget;
    m_bottomBar->setFixedHeight(60);
    QHBoxLayout *bottomLayout = new QHBoxLayout(m_bottomBar);
    bottomLayout->setContentsMargins(16, 6, 16, 6);
    bottomLayout->setSpacing(8);

    m_prevBtn = new QPushButton("‹ 上一章");
    m_nextBtn = new QPushButton("下一章 ›");
    QString navStyle = "QPushButton { background: transparent; border: 1px solid rgba(0,0,0,0.15); border-radius: 4px; padding: 4px 10px; font-size: 12px; } QPushButton:hover { background: rgba(0,0,0,0.06); }";
    m_prevBtn->setStyleSheet(navStyle);
    m_nextBtn->setStyleSheet(navStyle);
    connect(m_prevBtn, &QPushButton::clicked, this, &ReaderWidget::prevChapter);
    connect(m_nextBtn, &QPushButton::clicked, this, &ReaderWidget::nextChapter);

    m_progressSlider = new QSlider(Qt::Horizontal);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setFixedWidth(180);   // fixed: keeps the button row from being squeezed out at high DPI
    m_progressSlider->setStyleSheet(R"(
        QSlider::groove:horizontal { height: 4px; background: rgba(0,0,0,0.1); border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #2ea043; border-radius: 7px; }
        QSlider::sub-page:horizontal { background: #2ea043; border-radius: 2px; }
    )");
    connect(m_progressSlider, &QSlider::sliderPressed, this, &ReaderWidget::onSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &ReaderWidget::onSliderReleased);

    m_pageLabel = new QPushButton("0%");
    m_pageLabel->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 12px; color: #888; min-width: 44px; padding: 2px 4px; } QPushButton:hover { color: #2ea043; }");
    m_pageLabel->setCursor(Qt::PointingHandCursor);
    m_pageLabel->setToolTip("点击跳转到指定进度");
    connect(m_pageLabel, &QPushButton::clicked, this, &ReaderWidget::jumpToPercent);

    m_fontBtn = new QPushButton("字号");
    m_fontBtn->setStyleSheet(navStyle);
    connect(m_fontBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu;
        menu.setStyleSheet("QMenu { background: white; border: 1px solid #ddd; } QMenu::item { padding: 8px 24px; } QMenu::item:selected { background: #2ea043; color: white; }");
        menu.addAction("增大字体", this, [this]() { changeFontSize(2); });
        menu.addAction("减小字体", this, [this]() { changeFontSize(-2); });
        menu.exec(m_fontBtn->mapToGlobal(QPoint(0, m_fontBtn->height())));
    });

    m_themeBtn = new QPushButton("主题");
    m_themeBtn->setStyleSheet(navStyle);
    connect(m_themeBtn, &QPushButton::clicked, this, &ReaderWidget::showThemeMenu);

    m_bookmarkBtn = new QPushButton("书签");
    m_bookmarkBtn->setStyleSheet(navStyle);
    connect(m_bookmarkBtn, &QPushButton::clicked, this, &ReaderWidget::toggleBookmark);

    m_annoBtn = new QPushButton("标注");
    m_annoBtn->setStyleSheet(navStyle);
    connect(m_annoBtn, &QPushButton::clicked, this, &ReaderWidget::toggleAnnotationPanel);

    m_noteBtn = new QPushButton("笔记");
    m_noteBtn->setStyleSheet(navStyle);
    m_noteBtn->setIconSize(QSize(16, 16));
    connect(m_noteBtn, &QPushButton::clicked, this, &ReaderWidget::toggleNotePanel);

    // 字体选择（微信读书同款：宋体/黑体/楷体/仿宋/微软雅黑）
    m_familyBtn = new QPushButton("字体");
    m_familyBtn->setStyleSheet(navStyle);
    connect(m_familyBtn, &QPushButton::clicked, this, &ReaderWidget::showFontMenu);

    // 行间距调节（微信读书同款：1.2 / 1.5 / 1.75 / 2.0 / 2.5）
    m_spacingBtn = new QPushButton("行距");
    m_spacingBtn->setStyleSheet(navStyle);
    connect(m_spacingBtn, &QPushButton::clicked, this, &ReaderWidget::showSpacingMenu);

    // 全文搜索
    m_searchBtn = new QPushButton("搜索");
    m_searchBtn->setStyleSheet(navStyle);
    connect(m_searchBtn, &QPushButton::clicked, this, &ReaderWidget::openSearch);

    bottomLayout->addWidget(m_prevBtn);
    bottomLayout->addWidget(m_progressSlider);
    bottomLayout->addWidget(m_nextBtn);
    bottomLayout->addWidget(m_pageLabel);
    bottomLayout->addStretch(1);   // pushes the function-button row to the right, airy like the reference
    bottomLayout->addWidget(m_fontBtn);
    bottomLayout->addWidget(m_familyBtn);
    bottomLayout->addWidget(m_spacingBtn);
    bottomLayout->addWidget(m_themeBtn);
    bottomLayout->addWidget(m_bookmarkBtn);
    bottomLayout->addWidget(m_annoBtn);
    bottomLayout->addWidget(m_noteBtn);
    bottomLayout->addWidget(m_searchBtn);

    m_readerLayout->addWidget(m_topBar);
    m_readerLayout->addLayout(contentRow, 1);
    m_readerLayout->addWidget(m_bottomBar);

    // Click handling on the text viewport: drag-release with selection → annotate menu;
    // plain click on an existing highlight → annotation menu.
    m_readingView->viewport()->installEventFilter(this);
    // Also watch the view itself: when the reader page is hidden the view keeps a
    // stale geometry, so the document must be re-pinned to the real viewport width
    // on every resize and on becoming visible (see syncDocWidth).
    m_readingView->installEventFilter(this);

    applyTheme();
}

// ========== Font / text helpers ==========

QFont ReaderWidget::readerFont() const
{
    QFont f(m_fontFamily.isEmpty() ? QStringLiteral("Microsoft YaHei") : m_fontFamily);
    f.setPixelSize(m_fontPx);
    return f;
}

QString ReaderWidget::marginFamilyToFont(const QString &displayName) const
{
    if (displayName == "宋体") return "SimSun";
    if (displayName == "黑体") return "SimHei";
    if (displayName == "楷体") return "KaiTi";
    if (displayName == "仿宋") return "FangSong";
    if (displayName == "微软雅黑") return "Microsoft YaHei";
    return QString(); // 系统默认 / unknown → fall back to Microsoft YaHei
}

QString ReaderWidget::currentThemeKey() const
{
    switch (m_theme) {
    case Light:  return "light";
    case Dark:   return "dark";
    case Green:  return "eye";
    case Sepia:  return "parchment";
    case Custom: return "custom";
    }
    return "light";
}

QString ReaderWidget::currentMarginKey() const
{
    if (m_sideMargin <= 28) return "narrow";
    if (m_sideMargin >= 96) return "wide";
    return "medium";
}

QString ReaderWidget::barTextColor() const
{
    switch (m_theme) {
    case Light: return "#555";
    case Dark: return "#aaa";
    case Sepia: return "#7a6555";
    case Green: return "#4a6a4a";
    case Custom: return m_customText.name();
    }
    return "#555";
}

QString ReaderWidget::ctrlButtonStyle() const
{
    const QString barText = barTextColor();
    const QColor c(barText);
    return QString(
        "QPushButton { background: rgba(%1,%2,%3,0.12); border: 1px solid rgba(%1,%2,%3,0.35);"
        " border-radius: 13px; padding: 3px 10px; font-size: 12px; color: %4; }"
        "QPushButton:hover { background: rgba(%1,%2,%3,0.20); border-color: %4; }")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(barText);
}

// Feather icon copied from the user's design, tinted to the theme bar color so it
// stays visible on both light and dark chrome.
QIcon ReaderWidget::noteIcon() const
{
    QPixmap src(QStringLiteral(":/icons/note_feather.png"));
    if (src.isNull()) return QIcon();
    if (src.width() < 40)
        src = src.scaled(src.width() * 2, src.height() * 2, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    // The asset is a white glyph with variable alpha; tint every non-transparent
    // pixel to the theme's bar color, preserving the original alpha.
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    const QColor tint(barTextColor());
    const int tr = tint.red(), tg = tint.green(), tb = tint.blue();
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int a = qAlpha(line[x]);
            if (a > 0) line[x] = qRgba(tr, tg, tb, a);
        }
    }
    return QIcon(QPixmap::fromImage(img));
}

QColor ReaderWidget::highlightColor(const QString &key) const
{
    if (key == "green") return QColor(120, 224, 130, 170);
    if (key == "blue")  return QColor(140, 200, 255, 170);
    if (key == "pink")  return QColor(255, 170, 205, 170);
    return QColor(255, 224, 102, 190); // yellow
}

QString ReaderWidget::themeTextColor() const
{
    switch (m_theme) {
    case Light: return "#333333";
    case Dark:  return "#cccccc";
    case Sepia: return "#5b4636";
    case Green: return "#3a5a3a";
    case Custom: return m_customText.name(QColor::HexRgb);
    }
    return "#333333";
}

void ReaderWidget::clearTextSelection()
{
    QTextCursor c = m_readingView->textCursor();
    c.clearSelection();
    m_readingView->setTextCursor(c);
}

void ReaderWidget::relayoutPage()
{
    if (m_currentBookId.isEmpty()) return;
    const int pos = topCharPos();
    buildWholeDocument();
    restorePosition(pos);
}

// ========== Book loading ==========

void ReaderWidget::loadBook(const QString &id)
{
    BookInfo book = m_library->bookById(id);
    if (book.id.isEmpty()) return;

    m_currentBookId = id;
    m_bookText = m_library->loadBookText(id);
    m_chapters = m_library->extractChapters(m_bookText);
    m_bookTitleLabel->setText(book.title);

    // Build the paragraph index: trim each line, remember its source position.
    m_paras.clear();
    QSet<int> headingStarts;
    for (const ChapterInfo &ch : m_chapters)
        headingStarts.insert(ch.startPos);

    int ls = 0;
    const int len = m_bookText.length();
    while (ls <= len) {
        int le = m_bookText.indexOf('\n', ls);
        if (le < 0) le = len;
        int a = ls, bnd = le - 1;
        while (a < le && m_bookText[a].isSpace()) a++;
        while (bnd >= a && m_bookText[bnd].isSpace()) bnd--;
        if (bnd >= a) {
            Para p;
            p.src = a;
            p.len = bnd - a + 1;
            p.heading = headingStarts.contains(ls);
            m_paras << p;
        }
        ls = le + 1;
    }

    buildWholeDocument();
    // Position is restored once the reader page is actually shown (viewport
    // geometry settles only then); store it so openBook can re-apply cheaply.
    m_pendingRestorePos = book.progress;
}

void ReaderWidget::openBook(QListWidgetItem *item)
{
    if (!item) return;
    loadBook(item->data(Qt::UserRole).toString());
    m_stack->setCurrentIndex(1);
    applyTheme();
    m_readFlushCount = 0;
    m_readClock->start();
    // The document is width-independent; once the page is shown, just re-apply
    // the saved position (no document rebuild needed).
    QTimer::singleShot(0, this, [this]() {
        if (!m_currentBookId.isEmpty()) restorePosition(m_pendingRestorePos);
    });
}

void ReaderWidget::openBookById(const QString &id)
{
    loadBook(id);
    m_stack->setCurrentIndex(1);
    applyTheme();
    m_readFlushCount = 0;
    m_readClock->start();
    QTimer::singleShot(0, this, [this]() {
        if (!m_currentBookId.isEmpty()) restorePosition(m_pendingRestorePos);
    });
}

void ReaderWidget::backToShelf()
{
    // Stop the reading clock and flush the accumulated seconds to disk.
    m_readClock->stop();
    // Save progress
    if (!m_currentBookId.isEmpty()) {
        int pos = topCharPos();
        m_library->updateProgress(m_currentBookId, pos);  // also persists time counters
    }
    m_currentBookId.clear();
    m_bookText.clear();
    m_paras.clear();
    m_stack->setCurrentIndex(0);
    refreshShelf();
}

void ReaderWidget::onReadTick()
{
    if (m_currentBookId.isEmpty()) { m_readClock->stop(); return; }
    m_library->addReadingSeconds(m_currentBookId, 1);
    // Flush to disk periodically so a crash loses at most ~15s.
    if (++m_readFlushCount >= 15) {
        m_readFlushCount = 0;
        m_library->save();
    }
}

void ReaderWidget::backToShelfPublic()
{
    backToShelf();
}

// ========== Whole-book document building ==========
//
// The document is built programmatically (no HTML import) so every visible
// character maps 1:1 to the source text: block = ["　　"? for paragraphs] + trimmed
// paragraph. That mapping is what makes highlight positions survive layout and
// font changes.

void ReaderWidget::buildDocument(QTextDocument *doc, int begin, int end) const
{
    doc->clear();
    const QFont base = readerFont();
    doc->setDefaultFont(base);

    // 1) Assemble every visible paragraph into ONE string (no per-paragraph
    //    QString allocations) and insert it with a single setPlainText call.
    //    A whole book previously cost ~17.5k formatted insertText() calls, each
    //    allocating and font-resolving a run — that dominated the open delay.
    QString all;
    all.reserve(m_bookText.length() + (end - begin) * 3);
    const QStringView bookView(m_bookText);
    for (int i = begin; i < end; ++i) {
        const Para &p = m_paras[i];
        if (!p.heading) {
            all.append(QChar(0x3000)); // 首行缩进两个全角空格
            all.append(QChar(0x3000));
        }
        all.append(bookView.mid(p.src, p.len));
        all.append(QLatin1Char('\n'));
    }
    doc->setPlainText(all);

    applyDocumentFormats(doc, base);
}

// Paragraph spacing (all blocks, batched) + chapter-heading styles (few blocks).
// Body text keeps the document default font, so font/size changes can re-apply
// just these formats without re-inserting any text.
void ReaderWidget::applyDocumentFormats(QTextDocument *doc, const QFont &base) const
{
    const QFontMetricsF fm(base);
    const qreal extra = fm.height() * qMax(0.0, m_lineSpacing - 1.0);
    QTextBlockFormat bf;
    bf.setBottomMargin(extra);
    QTextCursor sel(doc);
    sel.select(QTextCursor::Document);
    sel.setBlockFormat(bf);

    for (int i = 0; i < m_paras.size(); ++i) {
        if (!m_paras[i].heading) continue;
        QTextBlock blk = doc->findBlockByNumber(i);
        QTextCursor hc(blk);
        QTextBlockFormat hf = bf;
        hf.setAlignment(Qt::AlignHCenter);
        hf.setTopMargin(fm.height() * 1.6);
        hf.setBottomMargin(extra + fm.height() * 1.0);
        hc.setBlockFormat(hf);
        QFont runFont = base;
        runFont.setBold(true);
        runFont.setPixelSize(qMin(48, m_fontPx + 6));
        QTextCharFormat cf;
        cf.setFont(runFont);
        hc.select(QTextCursor::LineUnderCursor);
        hc.mergeCharFormat(cf);
    }
}

// ========== Whole-book document ==========
//
// The reader shows the entire book as one continuously scrollable document.
// Chapter headings act as visual separators (centered, larger, with spacing).
// The block mapping below covers the whole document, so selection → source
// positions and highlight rendering work regardless of the scroll offset.

void ReaderWidget::buildWholeDocument()
{
    m_readingView->setUpdatesEnabled(false);

    m_paraSrcStart.clear();
    m_docBlockStart.clear();
    m_blockPrefix.clear();
    m_blockSrcLen.clear();

    if (m_paras.isEmpty()) {
        m_readingView->clear();
        updateChrome();
        m_readingView->setUpdatesEnabled(true);
        return;
    }

    // Build + format the whole book in a DETACHED QTextDocument. A standalone
    // doc has no QTextEdit attached, so per-block format changes don't trigger
    // the view's reactions (each reaction re-walks all ~17.5k blocks and
    // re-lays them out). The finished doc is swapped in with one layout pass.
    QTextDocument *doc = new QTextDocument;
    doc->setUndoRedoEnabled(false);
    buildDocument(doc, 0, m_paras.size());

    // Record the mapping for every block (one block per paragraph).
    QTextBlock blk = doc->firstBlock();
    for (int i = 0; i < m_paras.size() && blk.isValid(); ++i, blk = blk.next()) {
        const Para &p = m_paras[i];
        m_paraSrcStart << p.src;
        m_docBlockStart << blk.position();
        m_blockPrefix << (p.heading ? 0 : 2);
        m_blockSrcLen << p.len;
    }

    // Highlights: background + annotation id, across the whole book.
    const QList<Annotation> anns = m_library->annotations(m_currentBookId);
    QTextCursor c(doc);
    for (const Annotation &a : anns) {
        const int d1 = docFromSource(a.start);
        const int d2 = docFromSource(qMin(a.start + a.length, m_bookText.length()));
        if (d2 <= d1) continue;
        c.setPosition(d1);
        c.setPosition(d2, QTextCursor::KeepAnchor);
        QTextCharFormat cf;
        cf.setBackground(highlightColor(a.color));
        cf.setProperty(kAnnProp, a.id);
        c.mergeCharFormat(cf);
    }

    m_readingView->setDocument(doc);
    // Clear any temporary search-highlight extras so they never survive a rebuild.
    m_readingView->setExtraSelections({});
    // Pin the wrap width to the viewport right away. QTextEdit normally re-wraps
    // the document when the view resizes, but that only happens while the view is
    // VISIBLE — a reader page that was hidden (shelf shown, or another main-window
    // page active) keeps its stale geometry, so a document laid out there would
    // stay frozen at the wrong width and never re-wrap. Setting an explicit text
    // width now (and again on every Show/Resize in eventFilter) fixes it.
    syncDocWidth();

    updateChrome();
    if (m_annoPanel->isVisible())
        refreshAnnotationList();
    if (m_notePanel->isVisible())
        refreshNoteList();
    m_readingView->setUpdatesEnabled(true);
}

// Character position of the text at the top of the visible area.
int ReaderWidget::topCharPos() const
{
    QTextCursor c = m_readingView->cursorForPosition(QPoint(4, 4));
    return sourceFromDoc(c.position());
}

// Scroll so the given source position sits near the top of the viewport.
void ReaderWidget::restorePosition(int charPos)
{
    if (m_paras.isEmpty()) return;
    QTextDocument *doc = m_readingView->document();
    const int docPos = docFromSource(charPos);
    QTextBlock targetBlock = doc->findBlock(docPos);
    if (!targetBlock.isValid()) return;

    // Document-space math instead of cursorRect: right after a document swap
    // the view's layout state can be stale, but blockBoundingRect always lays
    // the block out on demand and yields a reliable top.
    const qreal blockTop = doc->documentLayout()->blockBoundingRect(targetBlock).top();
    QTextCursor c(doc);
    c.setPosition(docPos);
    m_readingView->setTextCursor(c);
    QScrollBar *sb = m_readingView->verticalScrollBar();
    sb->setValue(qMax(0, qRound(blockTop) - 12));
}

// Scroll by one viewport height (dir: +1 down / -1 up).
void ReaderWidget::scrollPage(int dir)
{
    QScrollBar *sb = m_readingView->verticalScrollBar();
    sb->setValue(sb->value() + dir * qMax(1, sb->pageStep()));
}

// Force the document to wrap at the view's ACTUAL viewport width. Called after
// every document swap and on every view Resize/Show. When the reader page is
// hidden (shelf shown, or the reader not the active main-window page) the view
// is never resized while visible, so QTextEdit never re-runs its document
// layout — the blocks keep the wrap width they got at open time and the text
// stays frozen in a narrow left column even after the window grows. Nudging the
// view's own size makes QTextEdit's resize handling re-layout at the real
// width (the same path that works for a visible standalone reader).
void ReaderWidget::syncDocWidth()
{
    QTextDocument *doc = m_readingView->document();
    if (!doc) return;
    const int w = m_readingView->viewport()->width();
    if (w <= 0) return;
    if (qRound(doc->documentLayout()->documentSize().width()) != w) {
        const QSize s = m_readingView->size();
        m_readingView->resize(s.width() + 1, s.height());
        m_readingView->resize(s.width(), s.height());
    }
}

// Source text position <-> rendered document position mapping for the current page.
int ReaderWidget::docFromSource(int srcPos) const
{
    if (m_paraSrcStart.isEmpty()) return 0;
    int idx = 0;
    for (int i = 0; i < m_paraSrcStart.size(); ++i) {
        if (m_paraSrcStart[i] <= srcPos) idx = i;
        else break;
    }
    int blockEnd = m_paraSrcStart[idx] + m_blockSrcLen[idx];
    srcPos = qBound(m_paraSrcStart[idx], srcPos, blockEnd);
    return m_docBlockStart.value(idx, 0) + m_blockPrefix.value(idx, 0)
           + (srcPos - m_paraSrcStart[idx]);
}

int ReaderWidget::sourceFromDoc(int docPos) const
{
    if (m_docBlockStart.isEmpty()) return 0;
    for (int i = 0; i < m_docBlockStart.size(); ++i) {
        const int start = m_docBlockStart[i];
        const int shownLen = m_blockPrefix[i] + m_blockSrcLen[i];
        if (docPos < start + shownLen) {
            int rel = docPos - start - m_blockPrefix[i];
            rel = qBound(0, rel, m_blockSrcLen[i]);
            return m_paraSrcStart[i] + rel;
        }
    }
    const int last = m_docBlockStart.size() - 1;
    return m_paraSrcStart.value(last, 0) + m_blockSrcLen.value(last, 0);
}

bool ReaderWidget::selectionSourceRange(int &start, int &end) const
{
    QTextCursor c = m_readingView->textCursor();
    if (!c.hasSelection()) return false;
    start = sourceFromDoc(c.selectionStart());
    end = sourceFromDoc(c.selectionEnd());
    // Re-check against the real selection end so trailing paragraph breaks don't
    // pull in the next paragraph.
    const int lastSelDocPos = c.selectionEnd() - 1;
    end = qMin(end, sourceFromDoc(lastSelDocPos) + 1);
    return end > start;
}

QString ReaderWidget::annotationAtPoint(const QPoint &viewPos) const
{
    QTextCursor c = m_readingView->cursorForPosition(viewPos);
    const QVariant v = c.charFormat().property(kAnnProp);
    return v.canConvert<QString>() ? v.toString() : QString();
}

QString ReaderWidget::findChapterCached(int pos) const
{
    QString current = "正文";
    for (const ChapterInfo &ch : m_chapters) {
        if (ch.startPos <= pos) current = ch.title;
        else break;
    }
    return current;
}

void ReaderWidget::updateChrome()
{
    if (m_paras.isEmpty() || m_bookText.isEmpty()) {
        m_pageLabel->setText("0%");
        m_chapterLabel->setText("");
        return;
    }
    const int top = topCharPos();
    m_chapterLabel->setText(findChapterCached(top));

    QScrollBar *sb = m_readingView->verticalScrollBar();
    const int max = sb->maximum();
    const int pct = max > 0 ? qBound(0, qRound(100.0 * sb->value() / max), 100) : 0;
    m_pageLabel->setText(QString("%1%").arg(pct));

    if (!m_sliderPressed) {
        m_progressSlider->setValue(max > 0 ? qRound(1000.0 * sb->value() / max) : 0);
    }

    const bool isBM = !m_currentBookId.isEmpty() && m_library->hasBookmark(m_currentBookId, top);
    const QString activeBtnStyle =
        "QPushButton { background: #2ea043; color: white; border: none; border-radius: 13px;"
        " padding: 4px 14px; font-size: 12px; }";
    m_bookmarkBtn->setStyleSheet(isBM ? activeBtnStyle : ctrlButtonStyle());

    const int annCount = m_currentBookId.isEmpty() ? 0 : int(m_library->annotations(m_currentBookId).size());
    m_annoBtn->setText(annCount > 0 ? QString("标注 %1").arg(annCount) : "标注");
    m_annoBtn->setStyleSheet(m_annoPanel->isVisible() ? activeBtnStyle : ctrlButtonStyle());

    const int noteCount = m_currentBookId.isEmpty() ? 0 : int(m_library->notes(m_currentBookId).size());
    m_noteBtn->setText(noteCount > 0 ? QString("笔记 %1").arg(noteCount) : "笔记");
    m_noteBtn->setStyleSheet(m_notePanel->isVisible() ? activeBtnStyle : ctrlButtonStyle());
}

// ========== Navigation ==========

void ReaderWidget::prevChapter()
{
    const int top = topCharPos();
    int best = -1;
    for (const ChapterInfo &ch : m_chapters) {
        if (ch.startPos < top - 2) best = ch.startPos;
        else break;
    }
    if (best >= 0) restorePosition(best);
}

void ReaderWidget::nextChapter()
{
    const int top = topCharPos();
    for (const ChapterInfo &ch : m_chapters) {
        if (ch.startPos > top + 2) { restorePosition(ch.startPos); return; }
    }
}

void ReaderWidget::toggleToolbar()
{
    m_toolbarVisible = !m_toolbarVisible;
    m_topBar->setVisible(m_toolbarVisible);
    m_bottomBar->setVisible(m_toolbarVisible);
}

void ReaderWidget::changeFontSize(int delta)
{
    m_fontPx = qBound(16, m_fontPx + delta * 2, 44);
    int pos = topCharPos();
    buildWholeDocument();
    restorePosition(pos);
}

void ReaderWidget::showThemeMenu()
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #ffffff; border: 1px solid rgba(0,0,0,0.12); border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 7px 24px 7px 34px; border-radius: 6px; font-size: 13px; color: #333; }"
        "QMenu::item:selected { background: #e8f5e9; color: #1a7f37; }"
        "QMenu::item:checked { color: #1a7f37; font-weight: 600; }"
        "QMenu::separator { height: 1px; background: #eee; margin: 5px 10px; }");

    struct ThemeItem { const char *label; Theme theme; const char *key; const char *color; };
    const ThemeItem items[] = {
        { "护眼绿", Green,     "eye",       "#cce8cf" },
        { "羊皮纸", Sepia,     "parchment", "#f4ecd8" },
        { "浅色",   Light,     "light",     "#ffffff" },
        { "深色",   Dark,      "dark",      "#1a1a1a" },
    };
    for (const auto &it : items) {
        QPixmap pm(14, 14);
        pm.fill(QColor(QString::fromLatin1(it.color)));
        QAction *a = menu.addAction(QIcon(pm), QString::fromUtf8(it.label));
        a->setData(QString::fromLatin1(it.key));
        a->setCheckable(true);
        a->setChecked(m_theme == it.theme);
    }
    menu.addSeparator();
    QAction *customAct = menu.addAction("自定义颜色…");
    menu.addSeparator();
    QAction *bgAct = menu.addAction(m_bgPath.isEmpty() ? "自定义背景图片…" : "更换背景图片…");
    QAction *clearAct = menu.addAction("清除背景图片");
    clearAct->setEnabled(!m_bgPath.isEmpty());

    QAction *chosen = menu.exec(m_themeBtn->mapToGlobal(QPoint(0, m_themeBtn->height())));
    if (!chosen) return;

    if (chosen == customAct) {
        QColor bg = QColorDialog::getColor(m_customBg, this, "选择阅读背景色");
        if (!bg.isValid()) return;
        QColor text = QColorDialog::getColor(m_customText, this, "选择文字颜色");
        if (!text.isValid()) return;
        m_customBg = bg;
        m_customText = text;
        m_theme = Custom;
        applyTheme();
        emit customColorsChanged(bg, text);
        return;
    }
    if (chosen == bgAct) { pickBackgroundImage(); return; }
    if (chosen == clearAct) { clearBackgroundImage(); return; }

    const QString key = chosen->data().toString();
    if (key == "eye") m_theme = Green;
    else if (key == "parchment") m_theme = Sepia;
    else if (key == "light") m_theme = Light;
    else if (key == "dark") m_theme = Dark;
    applyTheme();
    emit themeChanged(key);
}

void ReaderWidget::showFontMenu()
{
    if (m_currentBookId.isEmpty()) return;
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #ffffff; border: 1px solid rgba(0,0,0,0.12); border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 7px 24px 7px 34px; border-radius: 6px; font-size: 13px; color: #333; }"
        "QMenu::item:selected { background: #e8f5e9; color: #1a7f37; }"
        "QMenu::item:checked { color: #1a7f37; font-weight: 600; }");

    const QStringList fonts = { "微软雅黑", "宋体", "黑体", "楷体", "仿宋" };
    for (const QString &name : fonts) {
        QAction *a = menu.addAction(name);
        a->setCheckable(true);
        a->setChecked(m_fontFamily == marginFamilyToFont(name));
    }
    QAction *chosen = menu.exec(m_familyBtn->mapToGlobal(QPoint(0, m_familyBtn->height())));
    if (!chosen) return;
    const QString name = chosen->text();
    if (marginFamilyToFont(name).isEmpty()) return;
    const int pos = m_currentBookId.isEmpty() ? 0 : topCharPos();
    m_fontFamily = marginFamilyToFont(name);
    if (!m_currentBookId.isEmpty()) {
        buildWholeDocument();
        restorePosition(pos);
    }
    applyTheme();
    emit fontFamilyChanged(name);
}

void ReaderWidget::showSpacingMenu()
{
    if (m_currentBookId.isEmpty()) return;
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #ffffff; border: 1px solid rgba(0,0,0,0.12); border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 7px 24px 7px 34px; border-radius: 6px; font-size: 13px; color: #333; }"
        "QMenu::item:selected { background: #e8f5e9; color: #1a7f37; }"
        "QMenu::item:checked { color: #1a7f37; font-weight: 600; }");

    const QList<qreal> spacings = { 1.2, 1.5, 1.75, 2.0, 2.5 };
    for (qreal s : spacings) {
        QAction *a = menu.addAction(QString("行距 %1").arg(s));
        a->setCheckable(true);
        a->setChecked(qAbs(m_lineSpacing - s) < 0.01);
    }
    QAction *chosen = menu.exec(m_spacingBtn->mapToGlobal(QPoint(0, m_spacingBtn->height())));
    if (!chosen) return;
    const qreal s = chosen->text().remove("行距 ").toDouble();
    const int pos = m_currentBookId.isEmpty() ? 0 : topCharPos();
    m_lineSpacing = s;
    if (!m_currentBookId.isEmpty()) {
        buildWholeDocument();
        restorePosition(pos);
    }
    applyTheme();
    emit lineSpacingChanged(s);
}

void ReaderWidget::openSearch()
{
    if (m_currentBookId.isEmpty() || m_paras.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("搜索本书");
    dlg.resize(480, 520);
    dlg.setStyleSheet("background: #fafafa;");
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(12, 12, 12, 12);

    QLineEdit *edit = new QLineEdit;
    edit->setPlaceholderText("输入关键词，搜索本书内容");
    edit->setClearButtonEnabled(true);
    QLabel *hint = new QLabel("输入关键词后回车搜索");
    hint->setStyleSheet("color: #888; font-size: 12px;");
    QListWidget *list = new QListWidget;
    list->setStyleSheet(
        "QListWidget { background: white; border: 1px solid #e0e0e0; border-radius: 6px; font-size: 13px; }"
        "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #f0f0f0; }"
        "QListWidget::item:selected { background: #e8f5e9; color: #1a7f37; }");
    lay->addWidget(edit);
    lay->addWidget(hint);
    lay->addWidget(list, 1);

    auto runSearch = [this, edit, hint, list]() {
        const QString q = edit->text().trimmed();
        list->clear();
        if (q.isEmpty()) { hint->setText("请输入关键词"); return; }
        if (m_paras.isEmpty()) { hint->setText("未找到匹配内容"); return; }
        const QString lower = q.toLower();
        int hits = 0;
        const int maxHits = 200;
        for (int i = 0; i < m_paras.size(); ++i) {
            if (hits >= maxHits) break;
            const Para &p = m_paras[i];
            if (m_bookText.mid(p.src, p.len).toLower().contains(lower)) {
                ++hits;
                QString snip = m_bookText.mid(p.src, p.len);
                snip = snip.simplified();
                if (snip.length() > 40) snip = snip.left(40) + "…";
                QString chap = findChapterCached(p.src);
                QListWidgetItem *it = new QListWidgetItem(
                    chap.isEmpty() ? snip : QString("%1 · %2").arg(chap, snip));
                it->setData(Qt::UserRole, i);
                list->addItem(it);
            }
        }
        hint->setText(hits > 0 ? QString("找到 %1 处，双击结果跳转").arg(hits) : "未找到匹配内容");
    };
    connect(edit, &QLineEdit::returnPressed, this, runSearch);
    connect(list, &QListWidget::itemActivated, this, [this, &dlg](QListWidgetItem *it) {
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_paras.size()) { dlg.accept(); jumpToPara(idx); }
    });
    connect(list, &QListWidget::itemDoubleClicked, this, [this, &dlg](QListWidgetItem *it) {
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_paras.size()) { dlg.accept(); jumpToPara(idx); }
    });

    dlg.exec();
}

void ReaderWidget::jumpToPercent()
{
    if (m_currentBookId.isEmpty()) return;
    QScrollBar *sb = m_readingView->verticalScrollBar();
    const int cur = sb->maximum() > 0 ? qRound(100.0 * sb->value() / sb->maximum()) : 0;
    bool ok = false;
    const int pct = QInputDialog::getInt(this, "跳转进度",
                                         "跳转到全书进度（0-100%）", cur, 0, 100, 1, &ok);
    if (!ok) return;
    sb->setValue(sb->maximum() > 0 ? qRound(sb->maximum() * pct / 100.0) : 0);
    updateChrome();
}

void ReaderWidget::jumpToPara(int idx)
{
    if (idx < 0 || idx >= m_paras.size()) return;
    const Para &p = m_paras[idx];
    restorePosition(p.src);

    // Temporary highlight of the matched paragraph so the reader can see where
    // it landed (cleared on the next document rebuild or after a short delay).
    QTextDocument *doc = m_readingView->document();
    const int docPos = docFromSource(p.src);
    const int selStart = docPos;
    const int selEnd = qMin(docPos + p.len, doc->characterCount());
    QTextEdit::ExtraSelection sel;
    sel.cursor = QTextCursor(doc);
    sel.cursor.setPosition(selStart);
    sel.cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
    sel.format.setBackground(QColor(255, 224, 102, 150));
    m_readingView->setExtraSelections({ sel });
    QTimer::singleShot(2500, m_readingView, [this]() {
        if (m_readingView) m_readingView->setExtraSelections({});
    });
}

void ReaderWidget::pickBackgroundImage()
{
    QString file = QFileDialog::getOpenFileName(this, "选择阅读背景图片", QString(),
                                                "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
    if (file.isEmpty()) return;
    setBackgroundImage(file);
    emit backgroundImageChanged(file);
}

void ReaderWidget::clearBackgroundImage()
{
    if (m_bgPath.isEmpty()) return;
    m_bgPath.clear();
    applyTheme();
    emit backgroundImageChanged(QString());
}

void ReaderWidget::applyReaderSettings(const QString &themeKey, const QString &fontFamily,
                                       int fontSizePx, qreal lineSpacing, const QString &margin)
{
    if (themeKey == "light") m_theme = Light;
    else if (themeKey == "dark") m_theme = Dark;
    else if (themeKey == "eye") m_theme = Green;
    else if (themeKey == "parchment") m_theme = Sepia;

    m_fontFamily = marginFamilyToFont(fontFamily);
    if (fontSizePx > 0) m_fontPx = qBound(16, qRound(fontSizePx * 1.35), 44);
    if (lineSpacing >= 1.0 && lineSpacing <= 3.0) m_lineSpacing = lineSpacing;
    m_sideMargin = margin == "narrow" ? 28 : (margin == "wide" ? 96 : 56);

    m_textLayout->setContentsMargins(m_sideMargin, 4, m_sideMargin, 8);

    if (!m_currentBookId.isEmpty()) {
        int pos = topCharPos();
        buildWholeDocument();
        restorePosition(pos);
    }
    applyTheme();
}

void ReaderWidget::setCustomColors(const QColor &bg, const QColor &text)
{
    m_customBg = bg;
    m_customText = text;
    m_theme = Custom;
    applyTheme();
}

void ReaderWidget::toggleBookmark()
{
    if (m_currentBookId.isEmpty()) return;
    int charPos = topCharPos();
    m_library->toggleBookmark(m_currentBookId, charPos);
    updateChrome();
}

void ReaderWidget::onSliderPressed()
{
    m_sliderPressed = true;
}

void ReaderWidget::onSliderReleased()
{
    m_sliderPressed = false;
    QScrollBar *sb = m_readingView->verticalScrollBar();
    const int max = sb->maximum();
    sb->setValue(max > 0 ? qRound(max * m_progressSlider->value() / 1000.0) : 0);
}

void ReaderWidget::showToc()
{
    if (m_chapters.isEmpty()) {
        QMessageBox::information(this, "目录", "未检测到章节。");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("目录");
    dlg.resize(400, 500);
    dlg.setStyleSheet("background: #f5f5f5;");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(12, 12, 12, 12);

    QListWidget *list = new QListWidget;
    list->setStyleSheet(R"(
        QListWidget { background: white; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }
        QListWidget::item { padding: 10px 12px; border-bottom: 1px solid #f0f0f0; }
        QListWidget::item:selected { background: #2ea043; color: white; }
        QListWidget::item:hover:!selected { background: #f0f0f0; }
    )");
    for (const auto &ch : m_chapters) {
        QListWidgetItem *item = new QListWidgetItem(ch.title, list);
        item->setData(Qt::UserRole, ch.startPos);
    }
    layout->addWidget(list);

    QList<int> bms = m_library->bookmarks(m_currentBookId);
    if (!bms.isEmpty()) {
        QLabel *bmLabel = new QLabel("书签");
        bmLabel->setStyleSheet("font-weight: bold; color: #666; padding: 8px 0 4px;");
        layout->addWidget(bmLabel);

        QListWidget *bmList = new QListWidget;
        bmList->setMaximumHeight(150);
        bmList->setStyleSheet(list->styleSheet());
        for (int pos : bms) {
            QString chTitle = findChapterCached(pos);
            QListWidgetItem *item = new QListWidgetItem(chTitle, bmList);
            item->setData(Qt::UserRole, pos);
        }
        layout->addWidget(bmList);
        connect(bmList, &QListWidget::itemDoubleClicked, this, [this, &dlg](QListWidgetItem *item) {
            int pos = item->data(Qt::UserRole).toInt();
            restorePosition(pos);
            dlg.accept();
        });
    }

    connect(list, &QListWidget::itemDoubleClicked, this, [this, &dlg](QListWidgetItem *item) {
        int pos = item->data(Qt::UserRole).toInt();
        restorePosition(pos);
        dlg.accept();
    });

    dlg.exec();
}

void ReaderWidget::importBook()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "导入书籍", QString(), "文本文件 (*.txt);;所有文件 (*.*)");
    for (const QString &f : files) {
        m_library->importBook(f);
    }
}

void ReaderWidget::onShelfItemMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_shelfGrid->itemAt(pos);
    QMenu menu;
    menu.setStyleSheet("QMenu { background: white; border: 1px solid #ddd; } QMenu::item { padding: 8px 24px; } QMenu::item:selected { background: #2ea043; color: white; }");

    if (item) {
        QString id = item->data(Qt::UserRole).toString();
        menu.addAction("打开阅读", this, [this, item]() { openBook(item); });
        menu.addAction("更改封面图片", this, [this, id]() { changeBookCover(id); });
        menu.addAction("删除书籍", this, [this, id]() {
            if (QMessageBox::question(this, "确认删除", "确定从书架删除这本书？") == QMessageBox::Yes)
                m_library->removeBook(id);
        });
    } else {
        menu.addAction("导入书籍", this, &ReaderWidget::importBook);
    }
    menu.exec(m_shelfGrid->mapToGlobal(pos));
}

void ReaderWidget::continueReadingLatest()
{
    if (!m_lastReadBookId.isEmpty())
        openBookById(m_lastReadBookId);
}

void ReaderWidget::changeBookCover(const QString &bookId)
{
    QString path = QFileDialog::getOpenFileName(
        this, "选择封面图片", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;

    // Persist custom cover as books/covers/<bookId>.png
    const QString dir = m_library->rootPath() + "/covers";
    QDir().mkpath(dir);
    const QString dest = dir + "/" + bookId + ".png";
    QPixmap pm(path);
    if (pm.isNull()) {
        QMessageBox::warning(this, "封面", "无法读取所选图片");
        return;
    }
    pm.save(dest, "PNG");
    refreshShelf();
}

// ========== Annotations ==========

static QIcon makeSwatchIcon(const QColor &c)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(1, 1, 12, 12, 3, 3);
    p.end();
    return QIcon(pm);
}

QMenu *ReaderWidget::buildColorMenu(QWidget *parent)
{
    QMenu *colors = new QMenu("划线", parent);
    struct { const char *key; const char *label; } items[] = {
        { "yellow", "黄色 · 划线" },
        { "green",  "绿色 · 重点" },
        { "blue",   "蓝色 · 好词好句" },
        { "pink",   "粉色 · 存疑" },
    };
    for (const auto &it : items) {
        QColor c = highlightColor(it.key).darker(115);
        QAction *act = colors->addAction(makeSwatchIcon(c), QString::fromUtf8(it.label));
        act->setData(QString::fromUtf8(it.key));
    }
    return colors;
}

void ReaderWidget::showSelectionMenu(const QPoint &globalPos)
{
    int s = 0, e = 0;
    if (!selectionSourceRange(s, e) || m_currentBookId.isEmpty())
        return;

    const QString excerpt = m_bookText.mid(s, e - s);
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: white; border: 1px solid #ddd; } QMenu::item { padding: 7px 22px; } QMenu::item:selected { background: #2ea043; color: white; }");

    QMenu *colors = buildColorMenu(&menu);
    menu.addMenu(colors);
    connect(colors, &QMenu::triggered, this, [this, s, e](QAction *act) {
        m_library->addAnnotation(m_currentBookId, s, e - s, act->data().toString());
        clearTextSelection();
        relayoutPage();
    });

    menu.addAction("✍  划线并写笔记", this, [this, s, e, excerpt]() {
        bool ok = false;
        QString note = QInputDialog::getMultiLineText(
            this, "写笔记",
            QString("原文：%1\n\n写下你的笔记：").arg(excerpt.left(60)),
            QString(), &ok);
        if (!ok || note.trimmed().isEmpty()) return;
        m_library->addAnnotation(m_currentBookId, s, e - s, "yellow", note.trimmed());
        clearTextSelection();
        relayoutPage();
    });

    menu.addSeparator();
    menu.addAction("复制原文", this, [this, s, e]() {
        QApplication::clipboard()->setText(m_bookText.mid(s, e - s));
        clearTextSelection();
    });
    menu.addAction("清除这段高亮", this, [this, s, e]() {
        const auto anns = m_library->annotations(m_currentBookId);
        bool changed = false;
        for (const Annotation &a : anns) {
            if (a.start < e && a.start + a.length > s) {
                m_library->removeAnnotation(m_currentBookId, a.id);
                changed = true;
            }
        }
        clearTextSelection();
        if (changed) relayoutPage();
    });

    menu.exec(globalPos);
}

Annotation ReaderWidget::annotationById(const QString &annId) const
{
    const auto anns = m_library->annotations(m_currentBookId);
    for (const Annotation &a : anns)
        if (a.id == annId) return a;
    return Annotation();
}

void ReaderWidget::showAnnotationMenu(const QString &annId, const QPoint &globalPos)
{
    const Annotation ann = annotationById(annId);
    if (ann.id.isEmpty()) return;

    const QString excerpt = m_bookText.mid(ann.start, ann.length);
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: white; border: 1px solid #ddd; } QMenu::item { padding: 7px 22px; } QMenu::item:selected { background: #2ea043; color: white; }");

    menu.addAction("✎  写 / 改笔记", this, [this, annId, excerpt]() {
        const Annotation a = annotationById(annId);
        if (a.id.isEmpty()) return;
        bool ok = false;
        QString note = QInputDialog::getMultiLineText(
            this, "笔记",
            QString("原文：%1").arg(excerpt.left(60)),
            a.note, &ok);
        if (!ok) return;
        m_library->updateAnnotation(m_currentBookId, annId, QString(), note.trimmed());
        relayoutPage();
    });

    QMenu *colors = buildColorMenu(&menu);
    colors->setTitle("换颜色");
    menu.addMenu(colors);
    connect(colors, &QMenu::triggered, this, [this, annId](QAction *act) {
        m_library->updateAnnotation(m_currentBookId, annId, act->data().toString(), QString());
        relayoutPage();
    });

    menu.addSeparator();
    menu.addAction("删除这条划线", this, [this, annId]() {
        m_library->removeAnnotation(m_currentBookId, annId);
        relayoutPage();
    });

    menu.exec(globalPos);
}

void ReaderWidget::toggleAnnotationPanel()
{
    const bool show = !m_annoPanel->isVisible();
    const int pos = topCharPos();
    m_annoPanel->setVisible(show);
    if (show) {
        m_notePanel->hide();
        refreshAnnotationList();
    }
    updateChrome();
    // Viewport width changed → QTextEdit re-wraps natively; just keep the same
    // text at the top once layout settles (no document rebuild).
    QTimer::singleShot(0, this, [this, pos]() {
        if (!m_currentBookId.isEmpty()) restorePosition(pos);
    });
}

void ReaderWidget::toggleNotePanel()
{
    const bool show = !m_notePanel->isVisible();
    const int pos = topCharPos();
    m_notePanel->setVisible(show);
    if (show) {
        m_annoPanel->hide();
        refreshNoteList();
    }
    updateChrome();
    QTimer::singleShot(0, this, [this, pos]() {
        if (!m_currentBookId.isEmpty()) restorePosition(pos);
    });
}

void ReaderWidget::writeNote()
{
    if (m_currentBookId.isEmpty()) return;
    const int pos = topCharPos();
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(
        this, "写笔记",
        QString("在「%1」记一条笔记：").arg(findChapterCached(pos)),
        QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) return;
    m_library->addNote(m_currentBookId, pos, text);
    if (m_notePanel->isVisible()) refreshNoteList();
    updateChrome();
}

void ReaderWidget::refreshNoteList()
{
    m_noteList->clear();
    if (m_currentBookId.isEmpty()) {
        m_noteHeader->setText("笔记");
        return;
    }
    const auto notes = m_library->notes(m_currentBookId);
    m_noteHeader->setText(QString("笔记 · %1 条").arg(notes.size()));

    for (const BookNote &n : notes) {
        QString preview = n.text;
        preview.replace('\n', ' ');
        const QString first = preview.left(46) + (preview.size() > 46 ? "…" : "");
        const QString second = QString("%1 · %2")
            .arg(findChapterCached(n.startPos), n.createdAt.toString("MM-dd HH:mm"));
        QListWidgetItem *item = new QListWidgetItem(m_noteList);
        item->setData(Qt::UserRole, n.id);
        item->setText(first + "\n" + second);
        item->setToolTip(n.text);
    }
}

void ReaderWidget::onNoteActivated(QListWidgetItem *item)
{
    if (!item || m_currentBookId.isEmpty()) return;
    const QString id = item->data(Qt::UserRole).toString();
    for (const BookNote &n : m_library->notes(m_currentBookId)) {
        if (n.id == id) { restorePosition(n.startPos); return; }
    }
}

void ReaderWidget::onNoteListMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_noteList->itemAt(pos);
    if (!item || m_currentBookId.isEmpty()) return;
    const QString id = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    menu.setStyleSheet("QMenu { background: white; border: 1px solid #ddd; } QMenu::item { padding: 8px 24px; } QMenu::item:selected { background: #2ea043; color: white; }");
    menu.addAction("删除这条笔记", this, [this, id]() {
        m_library->removeNote(m_currentBookId, id);
        refreshNoteList();
        updateChrome();
    });
    menu.exec(m_noteList->viewport()->mapToGlobal(pos));
}

void ReaderWidget::refreshAnnotationList()
{
    m_annoList->clear();
    if (m_currentBookId.isEmpty()) {
        m_annoHeader->setText("标注");
        return;
    }
    const auto anns = m_library->annotations(m_currentBookId);
    m_annoHeader->setText(QString("标注 · %1 条").arg(anns.size()));

    for (const Annotation &a : anns) {
        QString excerpt = m_bookText.mid(a.start, a.length).trimmed();
        excerpt.replace('\n', ' ');
        QString text = excerpt.left(42) + (excerpt.size() > 42 ? "…" : "");
        if (!a.note.isEmpty())
            text += QString("\n📝 %1").arg(a.note.left(36).replace('\n', ' ') + (a.note.size() > 36 ? "…" : ""));

        QListWidgetItem *item = new QListWidgetItem(m_annoList);
        item->setData(Qt::UserRole, a.id);
        item->setText(text);
        item->setIcon(makeSwatchIcon(highlightColor(a.color)));
        item->setToolTip(a.note.isEmpty() ? excerpt : QString("%1\n\n笔记：%2").arg(excerpt, a.note));
    }
}

void ReaderWidget::onAnnotationActivated(QListWidgetItem *item)
{
    if (!item) return;
    jumpToAnnotation(item->data(Qt::UserRole).toString());
}

void ReaderWidget::onAnnotationListMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_annoList->itemAt(pos);
    if (!item) return;
    showAnnotationMenu(item->data(Qt::UserRole).toString(), m_annoList->viewport()->mapToGlobal(pos));
}

void ReaderWidget::jumpToAnnotation(const QString &annId)
{
    const Annotation ann = annotationById(annId);
    if (ann.id.isEmpty()) return;

    restorePosition(ann.start);

    const int d1 = docFromSource(ann.start);
    const int d2 = docFromSource(qMin(ann.start + ann.length, m_bookText.length()));
    if (d2 > d1) {
        QTextCursor c(m_readingView->document());
        c.setPosition(d1);
        c.setPosition(d2, QTextCursor::KeepAnchor);
        m_readingView->setTextCursor(c);
    }
    m_readingView->ensureCursorVisible();
}

// ========== Shelf background / chrome ==========

QPixmap ReaderWidget::generateCover(const BookInfo &book) const
{
    QPixmap pix(120, 160);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    QStringList colors = book.coverColor.split(",");
    QLinearGradient grad(0, 0, 120, 160);
    if (colors.size() >= 2) {
        grad.setColorAt(0, QColor(colors[0]));
        grad.setColorAt(1, QColor(colors[1]));
    } else {
        grad.setColorAt(0, QColor("#667eea"));
        grad.setColorAt(1, QColor("#764ba2"));
    }
    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 120, 160, 6, 6);

    // Vertical book title: up to two characters per line, large and centered.
    painter.setPen(QColor(255, 255, 255, 235));
    QFont titleFont("Microsoft YaHei", 16, QFont::Bold);
    painter.setFont(titleFont);
    QStringList lines;
    for (int i = 0; i < book.title.size(); i += 2)
        lines << book.title.mid(i, 2);
    painter.drawText(QRect(8, 20, 104, 120), Qt::AlignCenter | Qt::TextWordWrap,
                     lines.join("\n"));

    painter.end();
    return pix;
}

void ReaderWidget::refreshShelf()
{
    m_shelfGrid->clear();
    auto books = m_library->books();

    if (m_shelfTitle) m_shelfTitle->setText("书架");
    if (m_shelfCountLabel) m_shelfCountLabel->setText(QString("%1 本书").arg(books.size()));

    int latestPct = 0;
    QDateTime latestRead;
    QString latestBookId;

    for (const auto &book : books) {
        QListWidgetItem *item = new QListWidgetItem(m_shelfGrid);
        item->setText(book.title);
        item->setData(Qt::UserRole, book.id);
        item->setData(kShelfAuthorRole, book.author);
        item->setData(kShelfProgressRole, book.progress);
        item->setData(kShelfTotalCharsRole, book.totalChars);
        item->setData(kShelfAnnotationCountRole, m_library->annotations(book.id).size());
        item->setData(kShelfNoteCountRole, m_library->notes(book.id).size());
        item->setData(kShelfCoverColorRole, book.coverColor);
        const QString coverImgPath = m_library->rootPath() + "/covers/" + book.id + ".png";
        item->setData(kShelfCoverImageRole, QFile::exists(coverImgPath) ? coverImgPath : QString());

        // Current chapter from loaded text (only if reading has started).
        QString chapter;
        if (book.progress > 0 && book.totalChars > 0) {
            const QString text = m_library->loadBookText(book.id);
            chapter = m_library->findChapterTitle(text, book.progress);
        }
        item->setData(kShelfChapterRole, chapter);

        // Relative last-read time, e.g. "2 小时前".
        QString lastReadStr;
        if (book.lastRead.isValid()) {
            const qint64 secs = book.lastRead.secsTo(QDateTime::currentDateTime());
            if (secs < 0) {
                lastReadStr = QStringLiteral("刚刚阅读");
            } else if (secs < 60) {
                lastReadStr = QStringLiteral("刚刚阅读");
            } else if (secs < 3600) {
                lastReadStr = QStringLiteral("%1 分钟前阅读").arg(secs / 60);
            } else if (secs < 86400) {
                lastReadStr = QStringLiteral("%1 小时前阅读").arg(secs / 3600);
            } else if (secs < 86400 * 7) {
                lastReadStr = QStringLiteral("%1 天前阅读").arg(secs / 86400);
            } else {
                lastReadStr = QStringLiteral("上次阅读 %1").arg(book.lastRead.toString("MM-dd"));
            }
        } else {
            lastReadStr = QStringLiteral("尚未开始阅读");
        }
        item->setData(kShelfLastReadRole, lastReadStr);

        item->setIcon(QIcon(generateCover(book)));
        item->setSizeHint(QSize(360, 156));

        const int pct = book.totalChars > 0 ? qBound(0, book.progress * 100 / book.totalChars, 100) : 0;
        if (!latestRead.isValid() || book.lastRead > latestRead) {
            latestRead = book.lastRead;
            latestPct = pct;
            latestBookId = book.id;
            m_lastReadBookId = book.id;
            if (m_recordBookLabel) m_recordBookLabel->setText(QStringLiteral("《%1》").arg(book.title));
            if (m_recordChapterLabel) {
                const QString full =
                    chapter.isEmpty()
                            ? lastReadStr
                            : QStringLiteral("%1 · %3").arg(lastReadStr).arg(chapter);
                m_recordChapterLabel->setText(full);
                m_recordChapterLabel->setToolTip(full);
                // Elide long chapter text so it stays on one tidy line.
                const int avail = qMax(120, m_recordChapterLabel->width() - 4);
                QFontMetrics fm(m_recordChapterLabel->font());
                m_recordChapterLabel->setText(fm.elidedText(full, Qt::ElideRight, avail));
            }
        }
        if (m_sidebarRecentImportTitle) m_sidebarRecentImportTitle->setText(book.title);
        if (m_sidebarRecentImportTime) {
            if (book.addedAt.isValid()) {
                const qint64 secs = book.addedAt.secsTo(QDateTime::currentDateTime());
                if (secs < 60) m_sidebarRecentImportTime->setText(QStringLiteral("刚刚导入"));
                else if (secs < 3600) m_sidebarRecentImportTime->setText(QStringLiteral("%1 分钟前导入").arg(secs/60));
                else if (secs < 86400) m_sidebarRecentImportTime->setText(QStringLiteral("%1 小时前导入").arg(secs/3600));
                else m_sidebarRecentImportTime->setText(QStringLiteral("%1 天前导入").arg(secs/86400));
            } else {
                m_sidebarRecentImportTime->setText(QStringLiteral("已导入"));
            }
        }

        QString tip = QString("%1\n作者: %2\n进度: %3%").arg(book.title, book.author).arg(pct);
        const int annCount = m_library->annotations(book.id).size();
        const int noteCount = m_library->notes(book.id).size();
        if (annCount > 0) tip += QString("\n标注: %1 条").arg(annCount);
        if (noteCount > 0) tip += QString("\n笔记: %1 条").arg(noteCount);
        if (!chapter.isEmpty()) tip += QString("\n章节: %1").arg(chapter);
        item->setToolTip(tip);
    }

    if (m_sidebarBookCount) {
        m_sidebarBookCount->setText(QStringLiteral("%1 本书").arg(books.size()));
    }
    if (m_sidebarNoteCount) {
        int totalNotes = 0;
        for (const auto &book : books) totalNotes += m_library->notes(book.id).size();
        m_sidebarNoteCount->setText(QStringLiteral("%1 条记录").arg(totalNotes));
    }
    if (books.isEmpty()) {
        if (m_recordBookLabel) m_recordBookLabel->setText(QStringLiteral("暂无最近书目"));
        if (m_recordChapterLabel) m_recordChapterLabel->setText(QStringLiteral("导入书籍后开始阅读"));
        if (m_sidebarRecentImportTitle) m_sidebarRecentImportTitle->setText(QStringLiteral("暂无导入"));
        if (m_sidebarRecentImportTime) m_sidebarRecentImportTime->setText(QStringLiteral("导入书籍以查看"));
    }

    if (m_recordBadge) {
        m_recordBadge->setText(books.isEmpty() ? QStringLiteral("暂无记录") : QStringLiteral("已读 %1%").arg(latestPct));
    }

    // Reading-timer card: real accumulated reading time (today / all-time).
    if (m_timerTodayLabel || m_timerTotalLabel) {
        const qint64 todaySec = m_library->totalTodayReadSec();
        const qint64 allSec = m_library->totalAllReadSec();
        auto fmtDur = [](qint64 s) -> QString {
            if (s < 60) return QStringLiteral("%1 秒").arg(s);
            if (s < 3600) return QStringLiteral("%1 分钟").arg(s / 60);
            const qint64 h = s / 3600, m = (s % 3600) / 60;
            return m > 0 ? QStringLiteral("%1 小时 %2 分").arg(h).arg(m)
                         : QStringLiteral("%1 小时").arg(h);
        };
        if (m_timerTodayLabel)
            m_timerTodayLabel->setText(todaySec > 0
                ? QStringLiteral("今日阅读 %1").arg(fmtDur(todaySec))
                : QStringLiteral("今日尚未阅读"));
        if (m_timerTotalLabel)
            m_timerTotalLabel->setText(allSec > 0
                ? QStringLiteral("累计阅读 %1").arg(fmtDur(allSec))
                : QStringLiteral("累计阅读 0 分钟"));
    }
}

void ReaderWidget::applyTheme()
{
    QString bg, text, barBg, barText;
    switch (m_theme) {
    case Light: bg = "#ffffff"; text = "#333333"; barBg = "#f5f5f5"; barText = "#555"; break;
    case Dark: bg = "#1a1a1a"; text = "#cccccc"; barBg = "#222222"; barText = "#aaa"; break;
    case Sepia: bg = "#f4ecd8"; text = "#5b4636"; barBg = "#e8dcc0"; barText = "#7a6555"; break;
    case Green: bg = "#cce8cf"; text = "#3a5a3a"; barBg = "#b8d8bc"; barText = "#4a6a4a"; break;
    case Custom:
        bg = m_customBg.name();
        text = m_customText.name();
        barBg = m_customBg.lighter(110).name();
        barText = m_customText.name();
        break;
    }

    const bool hasBg = !m_bgPath.isEmpty() && QFile::exists(m_bgPath);
    if (hasBg) {
        const QString imgUrl = QDir::toNativeSeparators(m_bgPath).replace("\\", "/");
        m_readerPage->setStyleSheet(QString(
            "background: transparent; border-image: url(%1) 0 0 0 0 stretch stretch;").arg(imgUrl));
    } else {
        m_readerPage->setStyleSheet(QString("background: %1;").arg(bg));
    }
    // Translucent "paper card" behind the text when a custom image is active,
    // so body text stays readable regardless of the image brightness.
    if (m_textCard) {
        if (hasBg && m_panelsTransparent) {
            QColor paper(bg);
            paper.setAlpha(172);
            m_textCard->setStyleSheet(QString(
                "background: %1; border-radius: 10px;").arg(paper.name(QColor::HexArgb)));
        } else {
            m_textCard->setStyleSheet(QString());
        }
    }
    // font-family must be pinned here: the app-wide theme stylesheet also targets
    // QTextEdit, and any font property it applies would otherwise reset the
    // document default font to a family without CJK glyphs (→ tofu boxes).
    m_readingView->setStyleSheet(QString(
        "QTextEdit { background: transparent; border: none; padding: 0px;"
        " font-family: \"Microsoft YaHei\", \"SimSun\"; }"
        "QTextEdit::selection { background: rgba(46,160,67,0.25); }"));
    QPalette pal = m_readingView->palette();
    pal.setColor(QPalette::Text, QColor(text));
    m_readingView->setPalette(pal);
    m_chapterLabel->setStyleSheet(QString("font-size: 13px; color: %1; padding: 0 40px;").arg(barText));
    if (hasBg && m_panelsTransparent) {
        m_topBar->setStyleSheet(QString("background: transparent; color: %1;").arg(barText));
        m_bottomBar->setStyleSheet(QString("background: transparent; color: %1;").arg(barText));
    } else {
        m_topBar->setStyleSheet(QString("background: %1; color: %2;").arg(barBg).arg(barText));
        m_bottomBar->setStyleSheet(QString("background: %1; color: %2;").arg(barBg).arg(barText));
    }
    m_bookTitleLabel->setStyleSheet(QString("font-size: 15px; font-weight: 500; color: %1;").arg(barText));
    m_pageLabel->setStyleSheet(QString("font-size: 12px; color: %1; min-width: 44px;").arg(barText));

    QString btnStyle = QString("QPushButton { background: transparent; border: none; font-size: 14px; color: %1; padding: 6px 10px; border-radius: 4px; } QPushButton:hover { background: rgba(0,0,0,0.08); }").arg(barText);
    m_backBtn->setStyleSheet(btnStyle);
    m_tocBtn->setStyleSheet(btnStyle);

    QString ctrlBtnStyle = ctrlButtonStyle();
    QString activeBtnStyle = QString(
        "QPushButton { background: #2ea043; color: white; border: none; border-radius: 13px;"
        " padding: 3px 10px; font-size: 12px; }");
    m_fontBtn->setStyleSheet(ctrlBtnStyle);
    m_familyBtn->setStyleSheet(ctrlBtnStyle);
    m_spacingBtn->setStyleSheet(ctrlBtnStyle);
    m_themeBtn->setStyleSheet(ctrlBtnStyle);
    m_searchBtn->setStyleSheet(ctrlBtnStyle);
    m_prevBtn->setStyleSheet(ctrlBtnStyle);
    m_nextBtn->setStyleSheet(ctrlBtnStyle);

    if (!m_currentBookId.isEmpty()) {
        int charPos = topCharPos();
        bool isBM = m_library->hasBookmark(m_currentBookId, charPos);
        m_bookmarkBtn->setStyleSheet(isBM ? activeBtnStyle : ctrlBtnStyle);
    }
    m_annoBtn->setStyleSheet(m_annoPanel->isVisible() ? activeBtnStyle : ctrlBtnStyle);
    m_noteBtn->setIcon(noteIcon());
    m_noteBtn->setStyleSheet(m_notePanel->isVisible() ? activeBtnStyle : ctrlBtnStyle);

    m_progressSlider->setStyleSheet(QString(R"(
        QSlider::groove:horizontal { height: 4px; background: %1; border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #2ea043; border-radius: 7px; }
        QSlider::sub-page:horizontal { background: #2ea043; border-radius: 2px; }
    )").arg(QColor(barText).lighter(150).name()));
    // Text color is palette-driven (no per-run color in the document), so theme
    // switches never need a document rebuild.
}

void ReaderWidget::setShelfBackground(const QString &imagePath)
{
    m_shelfBgPath = imagePath;
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        m_shelfPage->setStyleSheet(QString(
            "QWidget#shelfPage { border-image: url(%1) 0 0 0 0 stretch stretch; }"
        ).arg(imagePath));
        m_shelfPage->setObjectName("shelfPage");
    } else {
        m_shelfPage->setStyleSheet("background: #f5f5f5;");
        m_shelfPage->setObjectName("");
    }
    m_shelfPage->update();
}

void ReaderWidget::setBackgroundImage(const QString &path)
{
    m_bgPath = path;
    applyTheme();
}

void ReaderWidget::setPanelsTransparent(bool transparent)
{
    m_panelsTransparent = transparent;
    applyTheme();
}

// ========== Input handling ==========

bool ReaderWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj && obj->property("readerTimerCard").toBool()
        && event->type() == QEvent::MouseButtonPress) {
        continueReadingLatest();
        return true;
    }

    if (m_readingView && obj == m_readingView) {
        // Re-pin the document wrap width whenever the view is (re)sized or
        // becomes visible — hidden reader pages miss resize events entirely.
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            syncDocWidth();
    } else if (m_readingView && obj == m_readingView->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            m_pressPos = static_cast<QMouseEvent*>(event)->pos();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !m_currentBookId.isEmpty()) {
                const bool dragged = (me->pos() - m_pressPos).manhattanLength() > 4;
                if (dragged && m_readingView->textCursor().hasSelection()) {
                    showSelectionMenu(m_readingView->viewport()->mapToGlobal(me->pos()));
                    return QWidget::eventFilter(obj, event);
                }
                if (!dragged && !m_readingView->textCursor().hasSelection()) {
                    const QString annId = annotationAtPoint(me->pos());
                    if (!annId.isEmpty()) {
                        showAnnotationMenu(annId, m_readingView->viewport()->mapToGlobal(me->pos()));
                        return QWidget::eventFilter(obj, event);
                    }
                }
            }
        } else if (event->type() == QEvent::KeyPress && m_stack->currentIndex() == ReaderPage) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_PageDown) {
                scrollPage(1); return true;
            }
            if (ke->key() == Qt::Key_PageUp) {
                scrollPage(-1); return true;
            }
            // Left / Right are left to QTextEdit's native cursor movement.
            if (ke->key() == Qt::Key_Escape) {
                if (m_readingView->textCursor().hasSelection()) {
                    clearTextSelection();
                    return true;
                }
                backToShelf();
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ReaderWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_stack->currentIndex() == ReaderPage && !m_currentBookId.isEmpty()) {
        // Width changes re-wrap natively; no document rebuild. Debounce a cheap
        // scroll correction so the same text stays at the top of the viewport.
        if (!m_relayoutTimer) {
            m_relayoutTimer = new QTimer(this);
            m_relayoutTimer->setSingleShot(true);
            m_relayoutTimer->setInterval(120);
            connect(m_relayoutTimer, &QTimer::timeout, this, [this]() {
                if (!m_currentBookId.isEmpty()) restorePosition(m_lastTopPos);
            });
        }
        m_lastTopPos = topCharPos();
        m_relayoutTimer->start();
    }
}

void ReaderWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_stack->currentIndex() == ReaderPage) {
        if (event->key() == Qt::Key_PageUp) {
            scrollPage(-1); event->accept(); return;
        }
        if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_Space) {
            scrollPage(1); event->accept(); return;
        }
        if (event->key() == Qt::Key_Escape) {
            backToShelf(); event->accept(); return;
        }
    }
    QWidget::keyPressEvent(event);
}
