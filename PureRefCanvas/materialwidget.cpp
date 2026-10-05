#include "materialwidget.h"
#include "materiallibrary.h"
#include "thumbcache.h"

#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QGraphicsView>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTextEdit>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QImageReader>
#include <QIcon>
#include <QDateTime>
#include <QSet>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScrollBar>
#include <QHeaderView>
#include <QTextStream>
#include <QtMath>
#include <QWheelEvent>
#include <QFile>
#include <QPainter>
#include <QPolygon>
#include <QMap>
#include <QScrollArea>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDrag>
#include <QMimeData>
#include <QToolButton>
#include <QPainter>
#include <cmath>

// Masonry card geometry (shared by grid creation and async thumbnail updates)
static constexpr int kCardWidth = 134;
static constexpr int kCardBorder = 2;  // per side
static constexpr int kImgWidth = kCardWidth - kCardBorder * 2;

// ===== ColorWheel (UE-style HSV picker) =====
ColorWheel::ColorWheel(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(120, 120);
    setCursor(Qt::CrossCursor);
}

void ColorWheel::setColor(const QColor &c)
{
    if (m_color != c) {
        m_color = c;
        update();
    }
}

void ColorWheel::renderWheel()
{
    int size = qMin(width(), height());
    if (size < 10) { m_wheelImage = QImage(); return; }
    // Keep it square and even-sized for clean centering
    size = (size / 2) * 2;
    m_wheelImage = QImage(size, size, QImage::Format_ARGB32);
    m_wheelImage.fill(Qt::transparent);
    const int radius = size / 2 - 1;
    const QPointF center(size / 2.0, size / 2.0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const qreal dx = x - center.x();
            const qreal dy = y - center.y();
            const qreal dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) continue;
            qreal hue = std::atan2(dy, dx) * 180.0 / M_PI;
            if (hue < 0) hue += 360.0;
            const qreal sat = dist / radius;
            m_wheelImage.setPixelColor(x, y, QColor::fromHsvF(hue / 360.0, sat, 1.0));
        }
    }
}

QColor ColorWheel::colorAtPos(const QPoint &pos) const
{
    if (m_wheelImage.isNull()) return m_color;
    const int size = m_wheelImage.width();
    const QPointF center(size / 2.0, size / 2.0);
    // Map widget-relative pos to image-relative pos
    const int ox = (width() - size) / 2;
    const int oy = (height() - size) / 2;
    const qreal dx = (pos.x() - ox) - center.x();
    const qreal dy = (pos.y() - oy) - center.y();
    const int radius = size / 2 - 1;
    qreal dist = std::sqrt(dx * dx + dy * dy);
    if (dist > radius) dist = radius;
    qreal hue = std::atan2(dy, dx) * 180.0 / M_PI;
    if (hue < 0) hue += 360.0;
    const qreal sat = dist / radius;
    return QColor::fromHsvF(hue / 360.0, sat, 1.0);
}

QPoint ColorWheel::posForColor(const QColor &c) const
{
    if (m_wheelImage.isNull()) return QPoint();
    const int size = m_wheelImage.width();
    const QPointF center(size / 2.0, size / 2.0);
    const int radius = size / 2 - 1;
    const qreal hue = c.hsvHueF() * 360.0;
    const qreal sat = c.hsvSaturationF();
    const qreal dist = sat * radius;
    const qreal rad = hue * M_PI / 180.0;
    const int ox = (width() - size) / 2;
    const int oy = (height() - size) / 2;
    return QPoint(qRound(ox + center.x() + dist * std::cos(rad)),
                  qRound(oy + center.y() + dist * std::sin(rad)));
}

void ColorWheel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!m_wheelImage.isNull()) {
        const int ox = (width() - m_wheelImage.width()) / 2;
        const int oy = (height() - m_wheelImage.height()) / 2;
        // Soft circular clip
        p.setClipRegion(QRegion(ox, oy, m_wheelImage.width(), m_wheelImage.height(), QRegion::Ellipse));
        p.drawImage(ox, oy, m_wheelImage);
        p.setClipRect(rect());
        // Outer ring
        p.setPen(QPen(QColor(255,255,255,60), 1));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(ox, oy, m_wheelImage.width(), m_wheelImage.height());
    }
    // Selection indicator (white ring with black outline)
    const QPoint ip = posForColor(m_color);
    if (!ip.isNull()) {
        p.setPen(QPen(Qt::black, 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(ip, 7, 7);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(ip, 7, 7);
    }
}

void ColorWheel::mousePressEvent(QMouseEvent *e)
{
    m_pressed = true;
    m_color = colorAtPos(e->pos());
    emit colorChanged(m_color);
    update();
}

void ColorWheel::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_pressed) return;
    m_color = colorAtPos(e->pos());
    emit colorChanged(m_color);
    update();
}

void ColorWheel::mouseReleaseEvent(QMouseEvent *)
{
    m_pressed = false;
}

void ColorWheel::resizeEvent(QResizeEvent *)
{
    renderWheel();
}

// ===== MasonryContainer =====
MasonryContainer::MasonryContainer(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
}

void MasonryContainer::clear()
{
    qDeleteAll(m_cards);
    m_cards.clear();
    m_selectedCard = nullptr;
    relayout();
}

void MasonryContainer::addCard(QWidget *card)
{
    card->setParent(this);
    card->installEventFilter(this);
    card->show();
    m_cards.append(card);
    relayout();
}

void MasonryContainer::relayout()
{
    if (m_inRelayout) return;
    m_inRelayout = true;
    const int spacing = m_spacing;
    const int colWidth = m_columnWidth;
    const int w = width();
    if (w <= 0) { m_inRelayout = false; return; }
    int cols = qMax(1, (w + spacing) / (colWidth + spacing));
    int totalW = cols * colWidth + (cols - 1) * spacing;
    int leftOffset = qMax(0, (w - totalW) / 2);
    QList<int> colHeights;
    colHeights.fill(0, cols);
    for (QWidget *card : m_cards) {
        int col = 0;
        for (int i = 1; i < cols; ++i)
            if (colHeights[i] < colHeights[col]) col = i;
        int x = leftOffset + col * (colWidth + spacing);
        int y = colHeights[col];
        card->setFixedWidth(colWidth);
        card->move(x, y);
        card->adjustSize();
        colHeights[col] = y + card->height() + spacing;
    }
    int maxH = 0;
    for (int h : colHeights) maxH = qMax(maxH, h);
    m_contentHeight = qMax(0, maxH - spacing);
    m_inRelayout = false;
    updateGeometry();
}

QSize MasonryContainer::sizeHint() const
{
    return QSize(width(), m_contentHeight);
}

void MasonryContainer::setColumnWidth(int width)
{
    m_columnWidth = qMax(60, width);
    relayout();
}

void MasonryContainer::setSpacing(int s)
{
    m_spacing = qMax(0, s);
    relayout();
}

QWidget* MasonryContainer::cardAt(const QPoint &pos) const
{
    for (QWidget *card : m_cards)
        if (card->geometry().contains(pos)) return card;
    return nullptr;
}

void MasonryContainer::setSelectedCard(QWidget *card)
{
    if (m_selectedCard == card) return;
    if (m_selectedCard) applyCardSelection(m_selectedCard, false);
    m_selectedCard = card;
    if (m_selectedCard) applyCardSelection(m_selectedCard, true);
}

QList<QWidget*> MasonryContainer::selectedCards() const
{
    QList<QWidget*> list;
    if (m_selectedCard) list.append(m_selectedCard);
    return list;
}

void MasonryContainer::applyCardSelection(QWidget *card, bool selected)
{
    if (!card) return;
    card->setStyleSheet(selected
        ? "#masonryCard { background: rgba(60,130,200,50); border: 2px solid #3c82c8; border-radius: 6px; }"
        : "#masonryCard { background: transparent; border: 2px solid transparent; border-radius: 6px; }");
}

void MasonryContainer::setCardInfoVisible(QWidget *card, bool visible)
{
    const QString nameColor = visible ? "#d8d8d8" : "transparent";
    const QString dimColor = visible ? "#888" : "transparent";
    if (QLabel *n = card->findChild<QLabel*>("cardName"))
        n->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;").arg(nameColor));
    if (QLabel *d = card->findChild<QLabel*>("cardDim"))
        d->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(dimColor));
}

void MasonryContainer::resizeEvent(QResizeEvent *)
{
    relayout();
}

bool MasonryContainer::eventFilter(QObject *obj, QEvent *event)
{
    QWidget *card = qobject_cast<QWidget*>(obj);
    if (!card || !m_cards.contains(card))
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_pressed = true;
            m_pressPos = me->globalPos();
            setSelectedCard(card);
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_pressed = false;
    } else if (event->type() == QEvent::MouseMove) {
        if (m_pressed && m_selectedCard == card) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if ((me->globalPos() - m_pressPos).manhattanLength() > QApplication::startDragDistance()) {
                m_pressed = false;
                QString path = card->property("filePath").toString();
                if (!path.isEmpty()) {
                    QDrag *drag = new QDrag(this);
                    QMimeData *mime = new QMimeData;
                    mime->setUrls(QList<QUrl>() << QUrl::fromLocalFile(path));
                    drag->setMimeData(mime);
                    drag->exec(Qt::CopyAction);
                }
            }
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        emit cardDoubleClicked(card);
        return true;
    } else if (event->type() == QEvent::ContextMenu) {
        QContextMenuEvent *ce = static_cast<QContextMenuEvent*>(event);
        setSelectedCard(card);
        emit cardContextMenuRequested(ce->globalPos(), card);
        return true;
    } else if (event->type() == QEvent::Enter) {
        setCardInfoVisible(card, true);
    } else if (event->type() == QEvent::Leave) {
        setCardInfoVisible(card, false);
    }
    return QWidget::eventFilter(obj, event);
}

MaterialWidget::MaterialWidget(const QString &libraryRoot, QWidget *parent)
    : QWidget(parent), m_libraryRoot(libraryRoot)
{
    m_library = new MaterialLibrary(libraryRoot, this);
    connect(m_library, &MaterialLibrary::libraryChanged, this, &MaterialWidget::refreshAll);
    setupUI();
    setAcceptDrops(true);
    // Defer heavy refresh — staggered to avoid main window freeze on show
    QTimer::singleShot(50, this, &MaterialWidget::refreshAll);
}

void MaterialWidget::setupUI()
{
    m_stack = new QStackedWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_stack);

    // ===== Browser Page =====
    m_browserPage = new QWidget;
    m_browserPage->setStyleSheet("background: transparent;");
    QVBoxLayout *browserLayout = new QVBoxLayout(m_browserPage);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setStyleSheet("QSplitter::handle { background: #333; }");

    // Left: folder tree
    QWidget *leftPanel = new QWidget;
    leftPanel->setFixedWidth(220);
    leftPanel->setStyleSheet("background: #1a1a1a; border-right: 1px solid #333;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("🔍 搜索资产");
    m_searchEdit->setFixedHeight(30);
    m_searchEdit->setStyleSheet("QLineEdit { background: #2a2a2a; color: #e0e0e0; border: 1px solid #444; border-radius: 4px; padding: 2px 10px; font-size: 13px; } QLineEdit:focus { border-color: #66c0f4; }");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MaterialWidget::onSearchChanged);
    leftLayout->addWidget(m_searchEdit);

    QLabel *sourcesLabel = new QLabel("  内容浏览器");
    sourcesLabel->setStyleSheet("color: #888; font-size: 12px; font-weight: 600; padding: 4px;");
    leftLayout->addWidget(sourcesLabel);

    m_folderTree = new QTreeWidget;
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setStyleSheet(R"(
        QTreeWidget { background: transparent; border: none; outline: none; color: #ccc; font-size: 13px; }
        QTreeWidget::item { padding: 4px 6px; border-radius: 3px; }
        QTreeWidget::item:selected { background: #2a475e; color: #66c0f4; }
        QTreeWidget::item:hover:!selected { background: #2a2a2a; }
    )");
    connect(m_folderTree, &QTreeWidget::itemClicked, this, &MaterialWidget::onFolderTreeClicked);
    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_folderTree, &QTreeWidget::customContextMenuRequested, this, &MaterialWidget::onTreeContextMenu);
    leftLayout->addWidget(m_folderTree, 1);

    m_splitter->addWidget(leftPanel);

    // Right panel
    m_rightPanel = new QWidget;
    m_rightPanel->setStyleSheet("background: transparent;");
    QVBoxLayout *rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Asset grid — masonry waterfall layout inside a scroll area
    m_gridScroll = new QScrollArea;
    m_gridScroll->setWidgetResizable(true);
    m_gridScroll->setFrameShape(QFrame::NoFrame);
    m_gridScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_gridScroll->setStyleSheet("background: transparent;");

    // Fixed host stays as the scroll area widget forever; masonry/list are
    // switched by show/hide — avoids setWidget reparent crashes.
    m_gridHost = new QWidget;
    m_gridHost->setStyleSheet("background: transparent;");
    QVBoxLayout *hostLay = new QVBoxLayout(m_gridHost);
    hostLay->setContentsMargins(0, 0, 0, 0);
    hostLay->setSpacing(0);

    m_masonry = new MasonryContainer;
    m_masonry->setColumnWidth(134);
    m_masonry->setSpacing(2);
    connect(m_masonry, &MasonryContainer::cardDoubleClicked, this, &MaterialWidget::onAssetDoubleClicked);
    connect(m_masonry, &MasonryContainer::cardContextMenuRequested, this,
            [this](const QPoint &globalPos, QWidget *) { onGridContextMenu(m_gridScroll->mapFromGlobal(globalPos)); });
    hostLay->addWidget(m_masonry);

    m_listContainer = new QWidget;
    m_listContainer->setStyleSheet("background: transparent;");
    QVBoxLayout *listLay = new QVBoxLayout(m_listContainer);
    listLay->setContentsMargins(8, 8, 8, 8);
    listLay->setSpacing(2);
    m_listContainer->hide();
    hostLay->addWidget(m_listContainer);

    m_gridScroll->setWidget(m_gridHost);
    setGridTransparent(true);

    // Color filter bar (above the grid)
    createColorFilterBar();
    rightLayout->addWidget(m_colorBar);

    // Color wheel popup panel (UE-style), hidden by default
    m_colorWheelPanel = new QWidget;
    m_colorWheelPanel->setFixedHeight(150);
    m_colorWheelPanel->setStyleSheet("background: rgba(18,28,40,0.92); border-bottom: 1px solid #2a475e;");
    QHBoxLayout *wheelLay = new QHBoxLayout(m_colorWheelPanel);
    wheelLay->setContentsMargins(12, 8, 12, 8);
    wheelLay->setSpacing(12);
    m_colorWheel = new ColorWheel;
    m_colorWheel->setFixedSize(134, 134);
    connect(m_colorWheel, &ColorWheel::colorChanged, this, &MaterialWidget::setActiveColor);
    wheelLay->addWidget(m_colorWheel);
    QLabel *wheelHint = new QLabel("拖动选择色相与饱和度\n点击色板按钮关闭");
    wheelHint->setStyleSheet("color: #8f98a0; font-size: 11px; background: transparent;");
    wheelLay->addWidget(wheelHint);
    wheelLay->addStretch();
    m_colorWheelPanel->hide();
    rightLayout->addWidget(m_colorWheelPanel);

    rightLayout->addWidget(m_gridScroll, 1);
    createLibraryZoomBar();

    // Status bar (fully transparent so the grid content shows through)
    m_statusLabel = new QLabel("0 项");
    m_statusLabel->setFixedHeight(24);
    m_statusLabel->setStyleSheet("background: transparent; color: #9aa0a6; font-size: 12px; padding: 0 16px;");
    rightLayout->addWidget(m_statusLabel);

    m_splitter->addWidget(m_rightPanel);
    m_splitter->setStretchFactor(1, 1);
    browserLayout->addWidget(m_splitter, 1);

    // Route all key events (Ctrl+V paste, etc.) through our event filter.
    m_browserPage->installEventFilter(this);

    m_stack->addWidget(m_browserPage);

    // ===== Preview Page =====
    setupPreviewPage();
}

void MaterialWidget::setupPreviewPage()
{
    QWidget *previewPage = new QWidget;
    previewPage->setObjectName("materialPreviewPage");
    previewPage->setStyleSheet("QWidget#materialPreviewPage { background: #0f1923; }");
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPage);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    // Top bar
    QWidget *topBar = new QWidget;
    topBar->setFixedHeight(44);
    topBar->setStyleSheet("background: #16202d; border-bottom: 1px solid #2a475e;");
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 6, 12, 6);
    topLayout->setSpacing(8);

    m_previewBackBtn = new QPushButton("← 返回库");
    m_previewBackBtn->setFixedHeight(32);
    m_previewBackBtn->setCursor(Qt::PointingHandCursor);
    m_previewBackBtn->setStyleSheet("QPushButton { background: rgba(42,71,94,0.5); color: #c7d5e0; border: 1px solid #2a475e; border-radius: 6px; padding: 4px 14px; font-size: 13px; } QPushButton:hover { background: #2a475e; color: #fff; border-color: #66c0f4; }");
    connect(m_previewBackBtn, &QPushButton::clicked, this, &MaterialWidget::closePreview);
    topLayout->addWidget(m_previewBackBtn);

    m_previewInfoLabel = new QLabel;
    m_previewInfoLabel->setStyleSheet("color: #c7d5e0; font-size: 13px; font-weight: 500;");
    topLayout->addWidget(m_previewInfoLabel, 1);

    previewLayout->addWidget(topBar);

    // Content area: graphics view for images, text edit for text
    m_previewScene = new QGraphicsScene(this);
    m_previewView = new QGraphicsView(m_previewScene);
    m_previewView->setStyleSheet("QGraphicsView { background: #0a0f15; border: none; } QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; } QScrollBar::handle:vertical { background: #2a475e; border-radius: 5px; min-height: 30px; } QScrollBar::handle:vertical:hover { background: #3a576e; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; } QScrollBar:horizontal { background: transparent; height: 10px; margin: 4px; } QScrollBar::handle:horizontal { background: #2a475e; border-radius: 5px; min-width: 30px; } QScrollBar::handle:horizontal:hover { background: #3a576e; } QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
    m_previewView->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    m_previewView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_previewView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_previewView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_previewView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    // PureRef-style: no scrollbars, pan by dragging, zoom by wheel
    m_previewView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previewView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_previewView->installEventFilter(this);
    m_previewItem = nullptr;
    m_previewZoom = 1.0;
    m_previewFitMode = true;

    // PureRef-style transient zoom badge (bottom-left, fades after a moment)
    m_previewZoomBadge = new QLabel(m_previewView->viewport());
    m_previewZoomBadge->setText("100%");
    m_previewZoomBadge->setStyleSheet(
        "background: rgba(18,28,40,0.85); color: #66c0f4; font-size: 12px; font-weight: 700;"
        "border: 1px solid rgba(102,192,244,0.4); border-radius: 6px; padding: 4px 10px;");
    m_previewZoomBadge->adjustSize();
    m_previewZoomBadge->move(12, m_previewView->viewport()->height() - m_previewZoomBadge->height() - 12);
    m_previewZoomBadge->hide();
    m_zoomBadgeTimer = new QTimer(this);
    m_zoomBadgeTimer->setSingleShot(true);
    m_zoomBadgeTimer->setInterval(1600);
    connect(m_zoomBadgeTimer, &QTimer::timeout, m_previewZoomBadge, &QWidget::hide);

    m_textPreview = new QTextEdit;
    m_textPreview->setReadOnly(true);
    m_textPreview->setStyleSheet("QTextEdit { background: #0f1923; color: #c7d5e0; border: none; padding: 24px; font-family: Consolas, monospace; font-size: 13px; }");

    QStackedWidget *previewContent = new QStackedWidget;
    previewContent->addWidget(m_previewView);
    previewContent->addWidget(m_textPreview);
    previewContent->setObjectName("previewContent");
    previewLayout->addWidget(previewContent, 1);

    m_stack->addWidget(previewPage);
}

void MaterialWidget::refreshAll()
{
    refreshFolderTree();
    refreshAssetGrid();
}

void MaterialWidget::refreshFolderTree()
{
    m_folderTree->clear();
    QTreeWidgetItem *rootItem = new QTreeWidgetItem({"📁  全部"});
    rootItem->setData(0, Qt::UserRole, QString());
    rootItem->setExpanded(true);
    m_folderTree->addTopLevelItem(rootItem);

    // Build tree from folder paths
    QMap<QString, QTreeWidgetItem*> itemMap;
    itemMap[""] = rootItem;

    QStringList allPaths = m_library->allFolderPaths();
    // Sort by depth to ensure parents created first
    std::sort(allPaths.begin(), allPaths.end(), [](const QString &a, const QString &b) {
        return a.count('/') < b.count('/');
    });

    for (const QString &path : allPaths) {
        int idx = path.lastIndexOf('/');
        QString parentPath = idx >= 0 ? path.left(idx) : QString();
        QString name = idx >= 0 ? path.mid(idx + 1) : path;
        QTreeWidgetItem *parent = itemMap.value(parentPath, rootItem);
        QTreeWidgetItem *item = new QTreeWidgetItem(parent, {"📁  " + name});
        item->setData(0, Qt::UserRole, path);
        item->setExpanded(true);
        itemMap[path] = item;
    }

    // Select current path
    if (m_currentPath.isEmpty()) {
        m_folderTree->setCurrentItem(rootItem);
    } else {
        QTreeWidgetItem *found = itemMap.value(m_currentPath, nullptr);
        if (found) m_folderTree->setCurrentItem(found);
    }
}

void MaterialWidget::refreshAssetGrid()
{
    const bool listMode = (m_viewMode == ViewMode::List);

    // Switch view by show/hide inside the fixed host (no setWidget reparent)
    if (listMode) {
        m_masonry->hide();
        m_listContainer->show();
        QLayout *lay = m_listContainer->layout();
        while (lay && lay->count()) {
            QLayoutItem *item = lay->takeAt(0);
            if (item->widget()) delete item->widget();
            delete item;
        }
    } else {
        m_listContainer->hide();
        m_masonry->show();
        m_masonry->setUpdatesEnabled(false);
        m_masonry->clear();
    }

    // Cancel any in-flight async thumbnail generation for the previous view
    m_pendingThumbs.clear();
    m_pendingPaths.clear();
    if (m_thumbTimer) m_thumbTimer->stop();

    QList<MaterialItem> items;
    if (!m_searchEdit->text().isEmpty()) {
        items = m_library->searchItems(m_searchEdit->text());
    } else {
        items = m_library->itemsInFolder(m_currentPath);
    }

    // Color filter: sync-extract dominant colors, then keep only matches
    if (m_activeColor.isValid()) {
        QList<MaterialItem> filtered;
        for (const MaterialItem &mat : items) {
            const QString fp = mat.fullPath(m_libraryRoot);
            if (!m_colorCache.contains(fp))
                thumbnailForItem(mat); // decode fills color cache
            if (colorMatches(m_colorCache.value(fp), m_activeColor))
                filtered.append(mat);
        }
        items = filtered;
    }

    // Size filter
    if (m_sizeFilter && m_sizeFilter->currentIndex() > 0) {
        QList<MaterialItem> filtered;
        const int idx = m_sizeFilter->currentIndex();
        for (const MaterialItem &mat : items) {
            const int maxDim = qMax(mat.width, mat.height);
            if (idx == 1 && maxDim < 500) filtered.append(mat);
            else if (idx == 2 && maxDim >= 500 && maxDim <= 2000) filtered.append(mat);
            else if (idx == 3 && maxDim > 2000) filtered.append(mat);
        }
        items = filtered;
    }

    // Format filter
    if (m_formatFilter && m_formatFilter->currentIndex() > 0) {
        QList<MaterialItem> filtered;
        const int idx = m_formatFilter->currentIndex();
        for (const MaterialItem &mat : items) {
            if (idx == 1 && mat.type == AssetImage) filtered.append(mat);
            else if (idx == 2 && mat.type == AssetVideo) filtered.append(mat);
            else if (idx == 3 && mat.type == AssetText) filtered.append(mat);
        }
        items = filtered;
    }

    // Rating filter: index 0 = all, 1 = >=1, ..., 4 = >=4, 5 = ==5
    if (m_ratingFilter && m_ratingFilter->currentIndex() > 0) {
        QList<MaterialItem> filtered;
        const int idx = m_ratingFilter->currentIndex();
        for (const MaterialItem &mat : items) {
            if (idx == 5) { if (mat.rating == 5) filtered.append(mat); }
            else          { if (mat.rating >= idx) filtered.append(mat); }
        }
        items = filtered;
    }

    const int cardWidth = m_masonry->columnWidth();
    const int imgWidth = cardWidth - kCardBorder * 2;

    for (const MaterialItem &mat : items) {
        const QString fullPath = mat.fullPath(m_libraryRoot);

        // ===== List view mode: one row per item =====
        if (listMode) {
            QWidget *row = new QWidget;
            row->setFixedHeight(64);
            row->setProperty("filePath", fullPath);
            row->setProperty("assetId", mat.id);
            row->setStyleSheet("QWidget { background: transparent; border-radius: 4px; } QWidget:hover { background: rgba(102,192,244,0.1); }");
            QHBoxLayout *rowLay = new QHBoxLayout(row);
            rowLay->setContentsMargins(6, 4, 6, 4);
            rowLay->setSpacing(10);

            // Thumbnail (60x60)
            QLabel *thumb = new QLabel;
            thumb->setFixedSize(60, 60);
            thumb->setAlignment(Qt::AlignCenter);
            thumb->setStyleSheet("background: #1a1a1a; border-radius: 4px;");
            if (mat.type == AssetImage) {
                QPixmap t = cachedThumb(mat);
                if (!t.isNull())
                    thumb->setPixmap(t.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                else {
                    thumb->setText("🖼");
                    thumb->setStyleSheet("color: #666; font-size: 24px; background: #1a1a1a; border-radius: 4px;");
                    enqueueAsyncThumb(mat);
                }
            } else if (mat.type == AssetVideo) {
                thumb->setText("🎬");
                thumb->setStyleSheet("color: #666; font-size: 24px; background: #1a1a1a; border-radius: 4px;");
            } else {
                thumb->setText("📄");
                thumb->setStyleSheet("color: #666; font-size: 24px; background: #1a1a1a; border-radius: 4px;");
            }
            rowLay->addWidget(thumb);

            // Info: filename + dimensions
            QVBoxLayout *infoLay = new QVBoxLayout;
            infoLay->setSpacing(2);
            QLabel *nameLbl = new QLabel(mat.baseName());
            nameLbl->setStyleSheet("color: #e0e0e0; font-size: 13px; background: transparent;");
            nameLbl->setWordWrap(false);
            QFontMetrics fm(nameLbl->font());
            nameLbl->setText(fm.elidedText(mat.baseName(), Qt::ElideMiddle, 400));
            infoLay->addWidget(nameLbl);

            QString dimText;
            if (mat.type == AssetImage && mat.width > 0)
                dimText = QString("%1 × %2  ·  图片").arg(mat.width).arg(mat.height);
            else if (mat.type == AssetVideo)
                dimText = "视频";
            else
                dimText = "文档";
            QLabel *dimLbl = new QLabel(dimText);
            dimLbl->setStyleSheet("color: #888; font-size: 11px; background: transparent;");
            infoLay->addWidget(dimLbl);
            rowLay->addLayout(infoLay);
            rowLay->addStretch();

            row->setProperty("isListRow", true);
            row->installEventFilter(this);
            if (QLayout *lay = m_listContainer->layout())
                lay->addWidget(row);
            continue;
        }

        // Create masonry card
        QWidget *card = new QWidget;
        card->setObjectName("masonryCard");
        card->setProperty("filePath", fullPath);
        card->setProperty("assetId", mat.id);
        card->setStyleSheet("#masonryCard { background: transparent; border: 2px solid transparent; border-radius: 6px; }");
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 2);
        cardLayout->setSpacing(1);

        // Image label — width fixed, height follows aspect (masonry) or square (grid)
        QLabel *imgLabel = new QLabel;
        imgLabel->setObjectName("thumbLabel");
        imgLabel->setAlignment(Qt::AlignCenter);
        const bool gridMode = (m_viewMode == ViewMode::Grid);
        int imgHeight = gridMode ? imgWidth : 104;
        if (mat.type == AssetImage) {
            QPixmap thumb = cachedThumb(mat);
            if (!thumb.isNull() && thumb.width() > 0) {
                if (!gridMode) {
                    qreal aspect = (qreal)thumb.height() / thumb.width();
                    imgHeight = qBound(60, qRound(imgWidth * aspect), 320);
                }
                imgLabel->setPixmap(thumb.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                imgLabel->setText("🖼");
                imgLabel->setStyleSheet("color: #666; font-size: 32px; background: transparent;");
                enqueueAsyncThumb(mat);
            }
        } else if (mat.type == AssetVideo) {
            imgLabel->setText("🎬");
            imgLabel->setStyleSheet("color: #666; font-size: 32px; background: transparent;");
        } else {
            imgLabel->setText("📄");
            imgLabel->setStyleSheet("color: #666; font-size: 32px; background: transparent;");
        }
        imgLabel->setFixedSize(imgWidth, imgHeight);
        cardLayout->addWidget(imgLabel);

        // Rating row (only shown when rated): ★★★☆☆ — Eagle-style corner badge.
        // We lay it out as a thin row directly under the thumbnail so we don't
        // have to fight absolute positioning inside the masonry card.
        if (mat.rating > 0) {
            QLabel *starLabel = new QLabel;
            starLabel->setObjectName("cardStars");
            starLabel->setFixedHeight(12);
            QString stars;
            for (int s = 1; s <= 5; ++s) stars += (s <= mat.rating ? QChar(0x2605) : QChar(0x2606));
            starLabel->setText(stars);
            starLabel->setStyleSheet(
                "color: #f2c94c; font-size: 10px; background: transparent;");
            starLabel->setAlignment(Qt::AlignCenter);
            cardLayout->addWidget(starLabel);
        }

        // Filename label (hidden by default, shown on hover via eventFilter)
        QLabel *nameLabel = new QLabel(mat.baseName());
        nameLabel->setObjectName("cardName");
        nameLabel->setFixedHeight(14);
        nameLabel->setStyleSheet("color: transparent; font-size: 11px; background: transparent;");
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(false);
        QFontMetrics fm(nameLabel->font());
        nameLabel->setText(fm.elidedText(mat.baseName(), Qt::ElideMiddle, imgWidth));
        cardLayout->addWidget(nameLabel);

        // Dimensions label (hidden by default, shown on hover)
        QString dimText;
        if (mat.type == AssetImage && mat.width > 0 && mat.height > 0)
            dimText = QString("%1 × %2").arg(mat.width).arg(mat.height);
        else if (mat.type == AssetVideo)
            dimText = "视频";
        else
            dimText = "文档";
        QLabel *dimLabel = new QLabel(dimText);
        dimLabel->setObjectName("cardDim");
        dimLabel->setFixedHeight(13);
        dimLabel->setStyleSheet("color: transparent; font-size: 10px; background: transparent;");
        dimLabel->setAlignment(Qt::AlignCenter);
        cardLayout->addWidget(dimLabel);

        m_masonry->addCard(card);
    }

    if (listMode) {
        if (QLayout *lay = m_listContainer->layout())
            lay->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
        m_statusLabel->setText(QString("%1 项").arg(items.size()));
    } else {
        m_masonry->setUpdatesEnabled(true);
        m_masonry->relayout();
        m_statusLabel->setText(QString("%1 项").arg(m_masonry->count()));
    }
    if (m_breadcrumbLabel) updateBreadcrumb();

    // Empty-state hint (use items count for list mode, masonry count otherwise)
    const bool isEmpty = listMode ? items.isEmpty() : (m_masonry->count() == 0);
    if (isEmpty) {
        if (!m_emptyLabel) {
            m_emptyLabel = new QLabel(m_gridScroll->viewport());
            m_emptyLabel->setAlignment(Qt::AlignCenter);
            m_emptyLabel->setWordWrap(true);
            m_emptyLabel->setStyleSheet("background: transparent; color: #7a8794; font-size: 13px;");
            m_gridScroll->viewport()->installEventFilter(this);
        }
        m_emptyLabel->setText(
            "<div style='font-size:46px; margin-bottom:10px;'>🗂</div>"
            "<div style='color:#8f98a0; font-size:14px; font-weight:600; margin-bottom:4px;'>这里还没有素材</div>"
            "<div style='color:#6b7885; font-size:12px;'>点击上方「导入」按钮，<br>或把图片/视频直接拖进当前文件夹</div>");
        m_emptyLabel->setGeometry(m_gridScroll->viewport()->rect());
        m_emptyLabel->show();
        m_emptyLabel->raise();
    } else if (m_emptyLabel) {
        m_emptyLabel->hide();
    }

    // Start background thumbnail generation if anything is pending
    if (!m_pendingThumbs.isEmpty()) {
        if (!m_thumbTimer) {
            m_thumbTimer = new QTimer(this);
            m_thumbTimer->setInterval(30);
            connect(m_thumbTimer, &QTimer::timeout, this, &MaterialWidget::processAsyncThumbs);
        }
        m_thumbTimer->start();
    }
}

QPixmap MaterialWidget::cachedThumb(const MaterialItem &item) const
{
    QString path = item.fullPath(m_libraryRoot);
    auto it = m_thumbCache.constFind(path);
    if (it != m_thumbCache.constEnd())
        return it.value();
    QPixmap disk = ThumbCache::load(path);
    if (!disk.isNull()) {
        if (m_thumbCache.size() >= 1024)
            m_thumbCache.clear();
        m_thumbCache.insert(path, disk);
        return disk;
    }
    return QPixmap();
}

void MaterialWidget::enqueueAsyncThumb(const MaterialItem &item)
{
    const QString path = item.fullPath(m_libraryRoot);
    if (m_pendingPaths.contains(path))
        return;
    m_pendingPaths.insert(path);
    m_pendingThumbs.append(item);
}

void MaterialWidget::processAsyncThumbs()
{
    const int batch = 6; // decode a handful per tick to keep the UI responsive
    for (int n = 0; n < batch && !m_pendingThumbs.isEmpty(); ++n) {
        MaterialItem mat = m_pendingThumbs.takeFirst();
        const QString path = mat.fullPath(m_libraryRoot);
        m_pendingPaths.remove(path);
        QPixmap thumb = thumbnailForItem(mat); // decodes + caches
        if (thumb.isNull())
            continue;
        // Update the corresponding card in place
        const int imgWidth = m_masonry->columnWidth() - kCardBorder * 2;
        for (QWidget *card : m_masonry->cards()) {
            if (card->property("filePath").toString() == path) {
                QLabel *img = card->findChild<QLabel*>("thumbLabel");
                if (img && !thumb.isNull() && thumb.width() > 0) {
                    qreal aspect = (qreal)thumb.height() / thumb.width();
                    int imgHeight = qBound(60, qRound(imgWidth * aspect), 320);
                    img->setFixedSize(imgWidth, imgHeight);
                    img->setStyleSheet("background: transparent;");
                    img->setPixmap(thumb.scaled(imgWidth, imgHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                break;
            }
        }
    }
    if (m_viewMode != ViewMode::List)
        m_masonry->relayout();
    if (m_pendingThumbs.isEmpty() && m_thumbTimer)
        m_thumbTimer->stop();
}

QPixmap MaterialWidget::thumbnailForItem(const MaterialItem &item)
{
    QString path = item.fullPath(m_libraryRoot);
    // Check cache first (memory, then disk)
    auto it = m_thumbCache.constFind(path);
    if (it == m_thumbCache.constEnd()) {
        QPixmap diskThumb = ThumbCache::load(path);
        if (!diskThumb.isNull()) {
            if (m_thumbCache.size() >= 1024)
                m_thumbCache.clear();
            m_thumbCache.insert(path, diskThumb);
            it = m_thumbCache.constFind(path);
        }
    }
    if (it == m_thumbCache.constEnd()) {
        // Downscaled decode: read only a small version instead of the full image.
        // NOTE: QImageReader::setScaledSize without setScaledClipRect uses
        // IgnoreAspectRatio internally, so we compute the aspect-preserving
        // target size ourselves — otherwise every thumbnail is stretched into a square.
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize orig = reader.size();
        if (orig.isValid() && !orig.isEmpty()) {
            const qreal scale = qMin(360.0 / orig.width(), 360.0 / orig.height());
            reader.setScaledSize(QSize(qMax(1, qRound(orig.width() * scale)),
                                        qMax(1, qRound(orig.height() * scale))));
        } else {
            reader.setScaledSize(QSize(360, 360));
        }
        QImage img = reader.read();
        if (img.isNull()) return QPixmap();
        QPixmap pix = QPixmap::fromImage(img);
        if (m_thumbCache.size() >= 1024)
            m_thumbCache.clear();
        m_thumbCache.insert(path, pix);
        ThumbCache::save(path, pix); // persist for next launch
        it = m_thumbCache.constFind(path);
    }
    int size = 140;
    if (m_thumbnailSize == "small") size = 96;
    if (m_thumbnailSize == "large") size = 180;
    // Extract dominant color for color filtering (lazy, cached)
    if (!m_colorCache.contains(path))
        m_colorCache.insert(path, extractDominantColor(it.value()));
    return it.value().scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QIcon MaterialWidget::iconForType(AssetType type) const
{
    switch (type) {
    case AssetImage: {
        QPixmap pix(96, 96);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(100, 140, 180));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(12, 16, 72, 64, 6, 6);
        p.setBrush(QColor(200, 220, 240));
        p.drawEllipse(24, 28, 16, 16);
        QPolygon poly;
        poly << QPoint(20, 72) << QPoint(44, 48) << QPoint(60, 62) << QPoint(76, 44) << QPoint(76, 72);
        p.setBrush(QColor(180, 200, 220));
        p.drawPolygon(poly);
        return QIcon(pix);
    }
    case AssetVideo: {
        QPixmap pix(96, 96);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(80, 80, 100));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(8, 8, 80, 80, 8, 8);
        p.setBrush(QColor(255, 255, 255));
        QPolygon tri;
        tri << QPoint(38, 32) << QPoint(38, 64) << QPoint(66, 48);
        p.drawPolygon(tri);
        return QIcon(pix);
    }
    case AssetText: {
        QPixmap pix(96, 96);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(240, 240, 240));
        p.setPen(QColor(180, 180, 180));
        p.drawRoundedRect(20, 12, 56, 72, 4, 4);
        p.setPen(QColor(150, 150, 150));
        for (int i = 0; i < 5; i++) {
            p.drawLine(28, 28 + i * 10, 68, 28 + i * 10);
        }
        return QIcon(pix);
    }
    default: return QIcon();
    }
}

void MaterialWidget::onFolderTreeClicked(QTreeWidgetItem *item, int)
{
    if (!item) return;
    QString path = item->data(0, Qt::UserRole).toString();
    setCurrentPath(path);
}

void MaterialWidget::setCurrentPath(const QString &path)
{
    if (m_currentPath != path) {
        m_backHistory.append(m_currentPath);
        if (m_backBtn) m_backBtn->setEnabled(true);
    }
    m_currentPath = path;
    if (m_searchEdit) m_searchEdit->clear();
    refreshAssetGrid();
}

void MaterialWidget::updateBreadcrumb()
{
    if (m_currentPath.isEmpty()) {
        m_breadcrumbLabel->setText("📁 全部");
    } else {
        QString crumb = "📁 全部";
        QStringList parts = m_currentPath.split('/');
        for (const QString &p : parts) crumb += " / " + p;
        m_breadcrumbLabel->setText(crumb);
    }
}

void MaterialWidget::onAssetDoubleClicked(QWidget *card)
{
    if (!card) return;
    QString id = card->property("assetId").toString();
    MaterialItem mat = m_library->itemById(id);
    if (!mat.id.isEmpty()) {
        previewAsset(mat.fullPath(m_libraryRoot), mat.type);
    }
}

void MaterialWidget::previewAsset(const QString &filePath, AssetType type)
{
    QFileInfo fi(filePath);
    m_previewInfoLabel->setText(QString("%1  (%2)").arg(fi.fileName()).arg(type == AssetImage ? "图片" : type == AssetVideo ? "视频" : type == AssetText ? "文本" : "其他"));

    QStackedWidget *previewContent = m_stack->widget(1)->findChild<QStackedWidget*>("previewContent");

    if (type == AssetImage) {
        // Full-resolution decode: QImageReader::setScaledSize() is NOT a free
        // optimisation here. Measured on this Qt build, PNG's scaled decode is
        // byte-identical to a full decode + Qt::SmoothTransformation, but JPEG
        // goes through libjpeg's DCT scaling and loses ~35% of the mean gradient
        // (high-frequency detail) at preview sizes, which reads as a blurry
        // preview. Quality wins over the memory saving.
        QPixmap pix(filePath);
        if (pix.isNull()) return;
        m_previewScene->clear();
        m_previewItem = m_previewScene->addPixmap(pix);
        m_previewScene->setSceneRect(pix.rect());
        m_previewZoom = 1.0;
        previewFit();
        if (previewContent) previewContent->setCurrentIndex(0);
    } else if (type == AssetText) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_textPreview->setPlainText(in.readAll());
            file.close();
        }
        if (previewContent) previewContent->setCurrentIndex(1);
    } else {
        // Video or other: show info
        m_textPreview->setPlainText(QString("文件: %1\n类型: 视频/其他\n\n视频预览需要系统解码器支持。\n可在文件管理器中打开此文件。").arg(fi.fileName()));
        if (previewContent) previewContent->setCurrentIndex(1);
    }

    m_stack->setCurrentIndex(1);
}

void MaterialWidget::closePreview()
{
    m_stack->setCurrentIndex(0);
}

void MaterialWidget::previewZoomIn()
{
    if (!m_previewItem) return;
    // Button zoom: anchor at view center (PureRef: wheel zooms at cursor, buttons at center)
    m_previewFitMode = false;
    m_previewView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    applyPreviewZoom(m_previewZoom * 1.25);
}

void MaterialWidget::previewZoomOut()
{
    if (!m_previewItem) return;
    m_previewFitMode = false;
    m_previewView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    applyPreviewZoom(m_previewZoom / 1.25);
}

void MaterialWidget::showZoomBadge()
{
    if (!m_previewZoomBadge) return;
    m_previewZoomBadge->setText(QString("%1%").arg(qRound(m_previewZoom * 100)));
    m_previewZoomBadge->adjustSize();
    m_previewZoomBadge->move(12, m_previewView->viewport()->height() - m_previewZoomBadge->height() - 12);
    m_previewZoomBadge->show();
    m_previewZoomBadge->raise();
    if (m_zoomBadgeTimer) m_zoomBadgeTimer->start();
}

void MaterialWidget::applyPreviewZoom(double target)
{
    // Clamp like PureRef: 5% .. 2000%
    double newZoom = qBound(0.05, target, 20.0);
    double factor = newZoom / m_previewZoom;
    if (qAbs(factor - 1.0) < 1e-6) return;
    m_previewZoom = newZoom;
    m_previewView->scale(factor, factor);
    showZoomBadge();
}

void MaterialWidget::previewFit()
{
    if (!m_previewItem) return;
    m_previewFitMode = true;
    m_previewView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_previewView->fitInView(m_previewItem, Qt::KeepAspectRatio);
    m_previewZoom = qMax(m_previewView->transform().m11(), m_previewView->transform().m22());
    showZoomBadge();
}

void MaterialWidget::previewActual()
{
    if (!m_previewItem) return;
    m_previewFitMode = false;
    m_previewView->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    m_previewView->resetTransform();
    m_previewZoom = 1.0;
    showZoomBadge();
}

void MaterialWidget::onGridContextMenu(const QPoint &pos)
{
    QMenu menu;
    menu.setStyleSheet("QMenu { background: #2a2a2a; color: #ddd; border: 1px solid #444; padding: 4px; } QMenu::item { padding: 6px 20px; border-radius: 3px; } QMenu::item:selected { background: #2a475e; color: #66c0f4; } QMenu::separator { height: 1px; background: #444; margin: 4px 8px; }");

    QPoint masonryPos = m_masonry->mapFrom(m_gridScroll->viewport(), pos);
    QWidget *card = m_masonry->cardAt(masonryPos);
    if (card) {
        QString path = card->property("filePath").toString();
        QString id = card->property("assetId").toString();
        menu.addAction("🎨 添加到画布", this, [this, path]() {
            if (!path.isEmpty()) emit addToCanvasRequested(path);
        });
        menu.addAction("预览", this, [this, card]() { onAssetDoubleClicked(card); });
        menu.addSeparator();

        // Rating submenu (Eagle-style: 0..5 stars, applies to selection)
        QMenu *ratingMenu = menu.addMenu("★ 评分");
        ratingMenu->setStyleSheet(menu.styleSheet());
        for (int s = 0; s <= 5; ++s) {
            QString label = (s == 0) ? "清除评分" : QString("★").repeated(s) + QString("☆").repeated(5 - s);
            ratingMenu->addAction(label, this, [this, s]() { applyRatingToSelected(s); });
        }

        menu.addAction("🏷 编辑标签…", this, &MaterialWidget::editTagsSelected);
        menu.addAction("📝 编辑备注…", this, &MaterialWidget::editNoteSelected);
        menu.addAction("🔗 来源信息…", this, &MaterialWidget::showSourceInfoForSelected);
        menu.addSeparator();
        menu.addAction("重命名", this, &MaterialWidget::renameSelected);
        menu.addAction("删除", this, &MaterialWidget::deleteSelected);
    } else {
        menu.addAction("导入文件", this, &MaterialWidget::importFiles);
        menu.addAction("粘贴剪贴板图片 (Ctrl+V)", this, &MaterialWidget::importFromClipboard);
        menu.addAction("新建文件夹", this, &MaterialWidget::createFolder);
        if (!m_currentPath.isEmpty()) {
            menu.addAction("返回上级", this, &MaterialWidget::navigateUp);
        }
    }
    menu.exec(m_gridScroll->viewport()->mapToGlobal(pos));
}

void MaterialWidget::onTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_folderTree->itemAt(pos);
    QMenu menu;
    menu.setStyleSheet("QMenu { background: #2a2a2a; color: #ddd; border: 1px solid #444; padding: 4px; } QMenu::item { padding: 6px 20px; border-radius: 3px; } QMenu::item:selected { background: #2a475e; color: #66c0f4; }");
    menu.addAction("新建文件夹", this, &MaterialWidget::createFolder);
    if (item) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            menu.addAction("删除文件夹", this, [this, path]() {
                if (QMessageBox::question(this, "删除文件夹", "确定删除此文件夹及其内容？") == QMessageBox::Yes) {
                    m_library->deleteFolder(path);
                }
            });
        }
    }
    menu.exec(m_folderTree->mapToGlobal(pos));
}

void MaterialWidget::importFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "导入文件", QString(),
        "所有支持的文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.mp4 *.avi *.mkv *.mov *.txt *.md *.json *.xml *.html *.csv *.log *.cpp *.h *.py *.js);;图片 (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;视频 (*.mp4 *.avi *.mkv *.mov);;文本 (*.txt *.md *.json *.xml);;所有文件 (*.*)");
    if (!files.isEmpty()) {
        m_library->importFiles(files, m_currentPath);
    }
}

void MaterialWidget::createFolder()
{
    bool ok;
    QString name = QInputDialog::getText(this, "新建文件夹", "文件夹名称：", QLineEdit::Normal, "新建文件夹", &ok);
    if (!ok || name.isEmpty()) return;
    QString path = m_currentPath.isEmpty() ? name : m_currentPath + "/" + name;
    if (m_library->createFolder(path)) {
        refreshAll();
    } else {
        QMessageBox::warning(this, "错误", "文件夹已存在或创建失败。");
    }
}

void MaterialWidget::deleteSelected()
{
    QStringList ids = selectedIds();
    if (ids.isEmpty()) return;
    if (QMessageBox::question(this, "删除", QString("确定删除 %1 项？").arg(ids.size())) == QMessageBox::Yes) {
        m_library->deleteItems(ids);
    }
}

void MaterialWidget::renameSelected()
{
    QStringList ids = selectedIds();
    if (ids.size() != 1) return;
    MaterialItem item = m_library->itemById(ids.first());
    if (item.id.isEmpty()) return;
    bool ok;
    QString newName = QInputDialog::getText(this, "重命名", "新名称：", QLineEdit::Normal, item.baseName(), &ok);
    if (!ok || newName.isEmpty()) return;
    m_library->renameItem(ids.first(), newName);
}

void MaterialWidget::navigateUp()
{
    if (m_currentPath.isEmpty()) return;
    int idx = m_currentPath.lastIndexOf('/');
    QString parent = idx >= 0 ? m_currentPath.left(idx) : QString();
    setCurrentPath(parent);
    refreshFolderTree();
}

void MaterialWidget::navigateBack()
{
    if (m_backHistory.isEmpty()) return;
    m_currentPath = m_backHistory.takeLast();
    if (m_backHistory.isEmpty() && m_backBtn) m_backBtn->setEnabled(false);
    if (m_searchEdit) m_searchEdit->clear();
    refreshAssetGrid();
    refreshFolderTree();
}

void MaterialWidget::onSearchChanged(const QString &)
{
    refreshAssetGrid();
}

void MaterialWidget::onBreadcrumbClicked(const QString &)
{
}

// ===== Clipboard paste import (Eagle feature #3 / #4) =====
void MaterialWidget::importFromClipboard()
{
    const QClipboard *clip = QApplication::clipboard();
    const QMimeData *mime = clip ? clip->mimeData() : nullptr;
    if (!mime) return;

    // Prefer an actual image on the clipboard.
    if (mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull()) {
            QString newId = m_library->importFromImage(img, m_currentPath);
            if (!newId.isEmpty()) {
                refreshAssetGrid();
                m_statusLabel->setText(QString("已从剪贴板粘贴 (%1×%2)").arg(img.width()).arg(img.height()));
            } else {
                QMessageBox::warning(this, "粘贴失败", "保存剪贴板图片到素材库时出错。");
            }
            return;
        }
    }
    // Fallback: dropped/printed image files on the clipboard (file list)
    if (mime->hasUrls()) {
        QStringList files;
        for (const QUrl &u : mime->urls())
            if (u.isLocalFile()) files << u.toLocalFile();
        if (!files.isEmpty()) {
            m_library->importFiles(files, m_currentPath);
            return;
        }
    }
    QMessageBox::information(this, "剪贴板", "剪贴板里没有可导入的图片或文件。");
}

void MaterialWidget::keyPressEvent(QKeyEvent *event)
{
    // Only intercept paste when the user isn't typing into the search box.
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_V &&
        (!m_searchEdit || !m_searchEdit->hasFocus())) {
        importFromClipboard();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ===== Metadata editing (tags / rating / note / source) =====
void MaterialWidget::applyRatingToSelected(int stars)
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) return;
    for (const QString &id : ids) m_library->setRating(id, stars);
    refreshAssetGrid();
    m_statusLabel->setText(
        stars == 0 ? QString("已清除 %1 项的评分").arg(ids.size())
                  : QString("已把 %1 项设为 %2 星").arg(ids.size()).arg(stars));
}

void MaterialWidget::editTagsSelected()
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) return;
    // When multiple items are selected, start from the union of their tags.
    QSet<QString> merged;
    for (const QString &id : ids) {
        MaterialItem it = m_library->itemById(id);
        for (const QString &t : it.tags) merged.insert(t);
    }
    bool ok;
    QString text = QInputDialog::getText(this,
        ids.size() == 1 ? "编辑标签" : QString("批量编辑标签（%1 项）").arg(ids.size()),
        "标签（用逗号分隔）：", QLineEdit::Normal,
        QStringList(merged.values()).join(", "), &ok);
    if (!ok) return;
    QStringList tags;
    for (const QString &tok : text.split(',', Qt::SkipEmptyParts)) {
        QString s = tok.trimmed();
        if (!s.isEmpty() && !tags.contains(s)) tags << s;
    }
    for (const QString &id : ids) m_library->setTags(id, tags);
    refreshAssetGrid();
}

void MaterialWidget::editNoteSelected()
{
    const QStringList ids = selectedIds();
    if (ids.size() != 1) {
        QMessageBox::information(this, "编辑备注", "请先单选一项素材再编辑备注。");
        return;
    }
    MaterialItem it = m_library->itemById(ids.first());
    bool ok;
    QString text = QInputDialog::getMultiLineText(this, "编辑备注", "备注：", it.note, &ok);
    if (!ok) return;
    m_library->setNote(ids.first(), text);
    refreshAssetGrid();
}

void MaterialWidget::showSourceInfoForSelected()
{
    const QStringList ids = selectedIds();
    if (ids.size() != 1) return;
    MaterialItem it = m_library->itemById(ids.first());
    QString info = QString(
        "<table cellpadding='4'>"
        "<tr><td><b>文件：</b></td><td>%1</td></tr>"
        "<tr><td><b>标签：</b></td><td>%2</td></tr>"
        "<tr><td><b>评分：</b></td><td>%3</td></tr>"
        "<tr><td><b>来源 URL：</b></td><td>%4</td></tr>"
        "<tr><td><b>来源页面：</b></td><td>%5</td></tr>"
        "<tr><td><b>导入时间：</b></td><td>%6</td></tr>"
        "</table>")
        .arg(it.fileName.toHtmlEscaped(),
             it.tags.join(", ").toHtmlEscaped().isEmpty() ? QString("<i>无</i>") : it.tags.join(", ").toHtmlEscaped(),
             it.rating > 0 ? QString("★").repeated(it.rating) : QString("<i>未评分</i>"),
             it.sourceUrl.isEmpty() ? QString("<i>本地导入</i>") : it.sourceUrl.toHtmlEscaped(),
             it.sourceReferrer.isEmpty() ? QString("<i>—</i>") : it.sourceReferrer.toHtmlEscaped(),
             it.importedAt.toString("yyyy-MM-dd HH:mm"));
    QMessageBox::information(this, "来源信息", info);
}

QStringList MaterialWidget::selectedIds() const
{
    QStringList ids;
    for (QWidget *card : m_masonry->selectedCards()) {
        QString id = card->property("assetId").toString();
        if (!id.isEmpty()) ids.append(id);
    }
    return ids;
}

// ===== Library floating zoom bar (mirrors canvas zoom bar) =====
void MaterialWidget::createLibraryZoomBar()
{
    // Ensure viewport resize always repositions the zoom bar (and empty label)
    m_gridScroll->viewport()->installEventFilter(this);

    m_zoomBar = new QWidget(m_gridScroll->viewport());
    m_zoomBar->setStyleSheet(
        "background: rgba(18,28,40,0.85);"
        "border: 1px solid rgba(102,192,244,0.4);"
        "border-radius: 8px;");
    QHBoxLayout *lay = new QHBoxLayout(m_zoomBar);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(2);

    auto makeBtn = [this](const QString &txt, const QString &tip) {
        QToolButton *b = new QToolButton(m_zoomBar);
        b->setText(txt);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedSize(26, 26);
        b->setStyleSheet(
            "QToolButton { background: transparent; border: none; border-radius: 5px;"
            " color: #c7d5e0; font-size: 16px; font-weight: 600; }"
            "QToolButton:hover { background: rgba(102,192,244,0.18); color: #66c0f4; }"
            "QToolButton:pressed { background: rgba(102,192,244,0.32); color: #ffffff; }");
        return b;
    };

    QToolButton *minus = makeBtn("−", "缩小缩略图");
    m_zoomPercentBtn = makeBtn("100%", "点击恢复默认大小");
    m_zoomPercentBtn->setFixedWidth(54);
    m_zoomPercentBtn->setStyleSheet(
        "QToolButton { background: transparent; border: none; border-radius: 5px;"
        " color: #66c0f4; font-size: 12px; font-weight: 700; }"
        "QToolButton:hover { background: rgba(102,192,244,0.18); }");
    QToolButton *plus = makeBtn("＋", "放大缩略图");

    lay->addWidget(minus);
    lay->addWidget(m_zoomPercentBtn);
    lay->addWidget(plus);
    m_zoomBar->adjustSize();

    connect(minus, &QToolButton::clicked, this, &MaterialWidget::libraryZoomOut);
    connect(plus, &QToolButton::clicked, this, &MaterialWidget::libraryZoomIn);
    connect(m_zoomPercentBtn, &QToolButton::clicked, this, &MaterialWidget::libraryZoomReset);

    updateZoomPercent();
    // Defer first positioning until the viewport has its real size
    QTimer::singleShot(0, this, &MaterialWidget::positionLibraryZoomBar);
}

void MaterialWidget::positionLibraryZoomBar()
{
    if (!m_zoomBar || !m_gridScroll) return;
    m_zoomBar->adjustSize();
    QWidget *vp = m_gridScroll->viewport();
    int x = vp->width() - m_zoomBar->width() - 10;
    int y = vp->height() - m_zoomBar->height() - 10;
    m_zoomBar->move(qMax(4, x), qMax(4, y));
    m_zoomBar->raise();
}

void MaterialWidget::libraryZoomIn()
{
    int w = qMin(kMaxCardWidth, m_masonry->columnWidth() + 12);
    m_masonry->setColumnWidth(w);
    refreshAssetGrid();
    updateZoomPercent();
}

void MaterialWidget::libraryZoomOut()
{
    int w = qMax(kMinCardWidth, m_masonry->columnWidth() - 12);
    m_masonry->setColumnWidth(w);
    refreshAssetGrid();
    updateZoomPercent();
}

void MaterialWidget::libraryZoomReset()
{
    m_masonry->setColumnWidth(kDefaultCardWidth);
    refreshAssetGrid();
    updateZoomPercent();
}

void MaterialWidget::updateZoomPercent()
{
    if (!m_zoomPercentBtn) return;
    int pct = qRound(m_masonry->columnWidth() * 100.0 / kDefaultCardWidth);
    m_zoomPercentBtn->setText(QString("%1%").arg(pct));
}

// ===== Color filter =====
void MaterialWidget::createColorFilterBar()
{
    m_colorBar = new QWidget;
    m_colorBar->setFixedHeight(30);
    m_colorBar->setStyleSheet("background: rgba(18,28,40,0.6);");
    QHBoxLayout *lay = new QHBoxLayout(m_colorBar);
    lay->setContentsMargins(8, 3, 8, 3);
    lay->setSpacing(6);

    QLabel *label = new QLabel("🎨 颜色");
    label->setStyleSheet("color: #8f98a0; font-size: 12px; background: transparent;");
    lay->addWidget(label);

    // Color swatch button — click to toggle the wheel panel
    m_colorSwatchBtn = new QToolButton;
    m_colorSwatchBtn->setFixedSize(24, 20);
    m_colorSwatchBtn->setCursor(Qt::PointingHandCursor);
    m_colorSwatchBtn->setToolTip("点击打开色轮拾取颜色");
    m_colorSwatchBtn->setStyleSheet(
        "QToolButton { background: #e03030; border: 1px solid #555; border-radius: 3px; }"
        "QToolButton:hover { border-color: #66c0f4; }");
    connect(m_colorSwatchBtn, &QToolButton::clicked, this, [this]() {
        if (m_colorWheelPanel) {
            m_colorWheelPanel->setVisible(!m_colorWheelPanel->isVisible());
        }
    });
    lay->addWidget(m_colorSwatchBtn);

    // Current color name/hex
    m_colorHexLabel = new QLabel("未筛选");
    m_colorHexLabel->setStyleSheet("color: #8f98a0; font-size: 11px; background: transparent;");
    lay->addWidget(m_colorHexLabel);

    lay->addStretch();

    // Size filter
    m_sizeFilter = new QComboBox;
    m_sizeFilter->addItems({"尺寸: 全部", "小 (<500px)", "中 (500-2000px)", "大 (>2000px)"});
    m_sizeFilter->setFixedHeight(22);
    m_sizeFilter->setStyleSheet(
        "QComboBox { background: #2a2a2a; color: #c7d5e0; border: 1px solid #444; border-radius: 3px; padding: 0 6px; font-size: 11px; }"
        "QComboBox:hover { border-color: #66c0f4; }"
        "QComboBox QAbstractItemView { background: #2a2a2a; color: #e0e0e0; selection-background-color: #2a475e; }");
    connect(m_sizeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialWidget::refreshAssetGrid);
    lay->addWidget(m_sizeFilter);

    // Format filter
    m_formatFilter = new QComboBox;
    m_formatFilter->addItems({"格式: 全部", "图片", "视频", "文本"});
    m_formatFilter->setFixedHeight(22);
    m_formatFilter->setStyleSheet(m_sizeFilter->styleSheet());
    connect(m_formatFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialWidget::refreshAssetGrid);
    lay->addWidget(m_formatFilter);

    // Rating filter: 全部 / ≥1★ / ≥2★ / ... / ≥5★
    m_ratingFilter = new QComboBox;
    m_ratingFilter->addItems({"星级: 全部", "★≥1", "★≥2", "★≥3", "★≥4", "★=5"});
    m_ratingFilter->setFixedHeight(22);
    m_ratingFilter->setStyleSheet(m_sizeFilter->styleSheet());
    connect(m_ratingFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MaterialWidget::refreshAssetGrid);
    lay->addWidget(m_ratingFilter);

    // View mode toggle (Masonry / Grid / List)
    m_viewModeBtn = new QToolButton;
    m_viewModeBtn->setText("▦ 瀑布流");
    m_viewModeBtn->setFixedHeight(22);
    m_viewModeBtn->setCursor(Qt::PointingHandCursor);
    m_viewModeBtn->setStyleSheet(
        "QToolButton { background: #2a2a2a; color: #c7d5e0; border: 1px solid #444; border-radius: 3px; padding: 0 8px; font-size: 11px; }"
        "QToolButton:hover { border-color: #66c0f4; color: #66c0f4; }");
    connect(m_viewModeBtn, &QToolButton::clicked, this, [this]() {
        switch (m_viewMode) {
            case ViewMode::Masonry: m_viewMode = ViewMode::Grid; m_viewModeBtn->setText("▦ 网格"); break;
            case ViewMode::Grid: m_viewMode = ViewMode::List; m_viewModeBtn->setText("≡ 列表"); break;
            case ViewMode::List: m_viewMode = ViewMode::Masonry; m_viewModeBtn->setText("▦ 瀑布流"); break;
        }
        refreshAssetGrid();
    });
    lay->addWidget(m_viewModeBtn);

    // Clear filter button
    QToolButton *clearBtn = new QToolButton;
    clearBtn->setText("✕ 清除");
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(
        "QToolButton { background: transparent; color: #8f98a0; border: none; font-size: 11px; padding: 2px 8px; border-radius: 4px; }"
        "QToolButton:hover { background: rgba(102,192,244,0.18); color: #66c0f4; }");
    connect(clearBtn, &QToolButton::clicked, this, [this]() {
        setActiveColor(QColor());
        if (m_colorWheelPanel) m_colorWheelPanel->hide();
    });
    lay->addWidget(clearBtn);
}

QColor MaterialWidget::extractDominantColor(const QPixmap &pix) const
{
    if (pix.isNull()) return QColor();
    // Scale to 1x1 for average color — fast and good enough for filtering
    QImage img = pix.toImage().scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return img.pixelColor(0, 0);
}

bool MaterialWidget::colorMatches(const QColor &a, const QColor &b) const
{
    if (!a.isValid() || !b.isValid()) return false;
    int dr = a.red() - b.red();
    int dg = a.green() - b.green();
    int db = a.blue() - b.blue();
    return (dr * dr + dg * dg + db * db) < 60 * 60; // threshold
}

void MaterialWidget::setActiveColor(const QColor &c)
{
    m_activeColor = c;
    // Sync swatch button background
    if (m_colorSwatchBtn) {
        if (c.isValid()) {
            m_colorSwatchBtn->setStyleSheet(QString(
                "QToolButton { background: %1; border: 1px solid #66c0f4; border-radius: 3px; }"
                "QToolButton:hover { border-color: #8ad0ff; }").arg(c.name()));
        } else {
            m_colorSwatchBtn->setStyleSheet(
                "QToolButton { background: #333; border: 1px dashed #666; border-radius: 3px; }"
                "QToolButton:hover { border-color: #66c0f4; }");
        }
    }
    if (m_colorHexLabel) {
        m_colorHexLabel->setText(c.isValid() ? c.name().toUpper() : "未筛选");
        m_colorHexLabel->setStyleSheet(QString(
            "color: %1; font-size: 11px; background: transparent;")
            .arg(c.isValid() ? "#c7d5e0" : "#8f98a0"));
    }
    if (m_colorWheel && c.isValid())
        m_colorWheel->setColor(c);
    refreshAssetGrid();
}

void MaterialWidget::setThumbnailSize(const QString &size)
{
    m_thumbnailSize = size;
    int colWidth = 134;
    if (size == "small") colWidth = 100;
    if (size == "large") colWidth = 170;
    m_masonry->setColumnWidth(colWidth);
    refreshAssetGrid();
}

void MaterialWidget::setCurrentFolder(const QString &path)
{
    m_currentPath = path;
    m_backHistory.clear();
    refreshAssetGrid();
}

void MaterialWidget::setBackgroundImage(const QString &path)
{
    if (!m_stack) return;
    if (path == m_bgPath) return;
    m_bgPath = path;

    if (!path.isEmpty() && QFile::exists(path)) {
        QPixmap original(path);
        if (!original.isNull()) {
            m_bgPixmap = original.scaled(1600, 900, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QString tempPath = QDir::tempPath() + "/lyy_library_bg.png";
            m_bgPixmap.save(tempPath, "PNG");
            m_stack->setStyleSheet(QString(
                "QStackedWidget { border-image: url(%1) 0 0 0 0 stretch stretch; }"
            ).arg(QDir::toNativeSeparators(tempPath).replace("\\", "/")));
        }
    } else {
        m_bgPixmap = QPixmap();
        m_stack->setStyleSheet("");
    }
}

void MaterialWidget::hideSidebar()
{
    if (m_splitter && m_splitter->count() > 0) {
        QWidget *left = m_splitter->widget(0);
        if (left) {
            left->setMaximumWidth(0);
            left->setMinimumWidth(0);
            left->hide();
        }
        m_splitter->setHandleWidth(0);
    }
}

void MaterialWidget::setStatusBarTransparent(bool transparent)
{
    if (!m_statusLabel) return;
    if (transparent) {
        m_statusLabel->setStyleSheet("background: transparent; color: #9aa0a6; font-size: 12px; padding: 0 16px;");
    } else {
        m_statusLabel->setStyleSheet("background: rgba(26,26,26,0.9); color: #888; font-size: 12px; padding: 0 16px; border-top: 1px solid #333;");
    }
}

void MaterialWidget::setGridTransparent(bool transparent)
{
    if (!m_gridScroll) return;
    Q_UNUSED(transparent);
    m_gridScroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(120,130,140,0.4); border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(120,130,140,0.6); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");
    m_masonry->setStyleSheet("background: transparent;");
}

bool MaterialWidget::eventFilter(QObject *obj, QEvent *event)
{
    // Global Ctrl+V: paste clipboard image into the library. We intercept at
    // the event-filter level because focus may sit on a child widget (card,
    // scroll area) that doesn't forward key events to this widget.
    if (event->type() == QEvent::KeyPress && obj != m_searchEdit) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if ((ke->modifiers() & Qt::ControlModifier) && ke->key() == Qt::Key_V) {
            importFromClipboard();
            return true;
        }
    }
    // List-view rows: double-click to preview
    if (obj->isWidgetType() && static_cast<QWidget*>(obj)->property("isListRow").toBool()) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            onAssetDoubleClicked(static_cast<QWidget*>(obj));
            return true;
        }
    }
    if (obj == m_previewView) {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *we = static_cast<QWheelEvent*>(event);
            if (we->angleDelta().y() == 0 || !m_previewItem) return true;
            m_previewFitMode = false;
            m_previewView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            // Smooth, continuous zoom (PureRef behaviour): 1.0015^delta
            const double factor = qPow(1.0015, we->angleDelta().y());
            applyPreviewZoom(m_previewZoom * factor);
            return true;
        }
        if (event->type() == QEvent::MouseButtonDblClick) {
            if (m_previewFitMode) previewActual();
            else previewFit();
            return true;
        }
        if (event->type() == QEvent::Resize) {
            // PureRef: while in fit mode the image always follows the window
            if (m_previewFitMode && m_previewItem) {
                previewFit();
                return true;
            }
        }
    }
    // Keep the empty-state hint sized to the scroll viewport
    if (obj == m_gridScroll->viewport() && event->type() == QEvent::Resize) {
        if (m_emptyLabel && m_emptyLabel->isVisible())
            m_emptyLabel->setGeometry(m_gridScroll->viewport()->rect());
        positionLibraryZoomBar();
    }
    return QWidget::eventFilter(obj, event);
}
