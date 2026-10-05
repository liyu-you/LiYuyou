#ifndef MATERIALWIDGET_H
#define MATERIALWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QMimeData>
#include <QUrl>
#include <QSet>
#include <QColor>
#include <QHash>
#include <QImage>
#include <QComboBox>
#include "materiallibrary.h"

// UE-style HSV color wheel: angle = hue, radius = saturation
class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(QWidget *parent = nullptr);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
signals:
    void colorChanged(const QColor &c);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;
private:
    void renderWheel();
    QColor colorAtPos(const QPoint &pos) const;
    QPoint posForColor(const QColor &c) const;
    QImage m_wheelImage;
    QColor m_color = Qt::red;
    bool m_pressed = false;
};

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QLabel;
class QTimer;
class QSplitter;
class QStackedWidget;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QPushButton;
class QToolButton;
class QTextEdit;
class QScrollArea;

// Masonry / waterfall layout: fixed column width, each card flows into the
// shortest column. Card height follows the image aspect ratio so non-square
// images never get letterboxed inside a uniform cell.
class MasonryContainer : public QWidget
{
    Q_OBJECT
public:
    explicit MasonryContainer(QWidget *parent = nullptr);
    void clear();
    void addCard(QWidget *card);
    void relayout();
    void setColumnWidth(int w);
    void setSpacing(int s);
    int columnWidth() const { return m_columnWidth; }
    QList<QWidget*> cards() const { return m_cards; }
    int count() const { return m_cards.size(); }
    QWidget* cardAt(const QPoint &pos) const;
    void setSelectedCard(QWidget *card);
    QWidget* selectedCard() const { return m_selectedCard; }
    QList<QWidget*> selectedCards() const;
    QSize sizeHint() const override;
signals:
    void cardDoubleClicked(QWidget *card);
    void cardContextMenuRequested(const QPoint &globalPos, QWidget *card);
protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    void applyCardSelection(QWidget *card, bool selected);
    void setCardInfoVisible(QWidget *card, bool visible);
    QList<QWidget*> m_cards;
    QWidget *m_selectedCard = nullptr;
    int m_columnWidth = 134;
    int m_spacing = 10;
    int m_contentHeight = 0;
    bool m_inRelayout = false;
    QPoint m_pressPos;
    bool m_pressed = false;
};

class MaterialWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MaterialWidget(const QString &libraryRoot, QWidget *parent = nullptr);

    MaterialLibrary *library() const { return m_library; }
    QString libraryRoot() const { return m_libraryRoot; }
    void setThumbnailSize(const QString &size); // small/medium/large
    void setCurrentFolder(const QString &path);
    void setBackgroundImage(const QString &path);
    void hideSidebar();
    void setStatusBarTransparent(bool transparent);
    void setGridTransparent(bool transparent);
    void clearThumbCache() { m_thumbCache.clear(); refreshAssetGrid(); }

signals:
    void addToCanvasRequested(const QString &filePath);

private:
    QPixmap m_bgPixmap;
    QString m_bgPath;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void importFiles();
    void importFromClipboard();   // Ctrl+V: paste clipboard image into library

private slots:
    void refreshAll();
    void refreshFolderTree();
    void refreshAssetGrid();
    void onFolderTreeClicked(QTreeWidgetItem *item, int col);
    void onAssetDoubleClicked(QWidget *card);
    void onGridContextMenu(const QPoint &pos);
    void onTreeContextMenu(const QPoint &pos);
    void onSearchChanged(const QString &text);
    void onBreadcrumbClicked(const QString &path);

    void createFolder();
    void deleteSelected();
    void renameSelected();
    void navigateUp();
    void navigateBack();

    // Eagle-style metadata editing on current selection
    void editTagsSelected();
    void editNoteSelected();
    void applyRatingToSelected(int stars);
    void showSourceInfoForSelected();
    void previewAsset(const QString &filePath, AssetType type);
    void closePreview();
    void previewZoomIn();
    void previewZoomOut();
    void applyPreviewZoom(double target);
    void showZoomBadge();
    void previewActual();
    void previewFit();

private:
    void setupUI();
    void setupPreviewPage();
    void updateBreadcrumb();
    QString currentPath() const { return m_currentPath; }
    void setCurrentPath(const QString &path);
    QStringList selectedIds() const;
    QIcon iconForType(AssetType type) const;
    QPixmap thumbnailForItem(const MaterialItem &item);
    QPixmap cachedThumb(const MaterialItem &item) const;   // memory/disk only, no decode
    void enqueueAsyncThumb(const MaterialItem &item);
    void processAsyncThumbs();
    void createLibraryZoomBar();
    void positionLibraryZoomBar();
    void libraryZoomIn();
    void libraryZoomOut();
    void libraryZoomReset();
    void updateZoomPercent();
    void createColorFilterBar();
    QColor extractDominantColor(const QPixmap &pix) const;
    void setActiveColor(const QColor &c);
    bool colorMatches(const QColor &a, const QColor &b) const;

    MaterialLibrary *m_library = nullptr;
    QString m_libraryRoot;
    QString m_currentPath; // current folder path, empty = root
    QStringList m_backHistory;
    QString m_thumbnailSize = "medium";
    mutable QHash<QString, QPixmap> m_thumbCache; // thumbnail cache by file path
    QTimer *m_thumbTimer = nullptr;               // async thumbnail generator
    QList<MaterialItem> m_pendingThumbs;          // queue of items awaiting thumbnails
    QSet<QString> m_pendingPaths;                 // dedupe of queued paths

    // Main layout
    QSplitter *m_splitter = nullptr;
    QTreeWidget *m_folderTree = nullptr;
    QWidget *m_rightPanel = nullptr;

    // Top toolbar
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_upBtn = nullptr;
    QLabel *m_breadcrumbLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_importBtn = nullptr;
    QPushButton *m_newFolderBtn = nullptr;

    // Asset grid (masonry waterfall layout)
    QScrollArea *m_gridScroll = nullptr;
    QWidget *m_gridHost = nullptr;      // fixed host inside scroll area
    MasonryContainer *m_masonry = nullptr;
    QWidget *m_listContainer = nullptr; // for List view mode
    QLabel *m_statusLabel = nullptr;
    QLabel *m_emptyLabel = nullptr;
    // Color filter bar + wheel popup (UE-style)
    QWidget *m_colorBar = nullptr;
    QToolButton *m_colorSwatchBtn = nullptr;
    QLabel *m_colorHexLabel = nullptr;
    QWidget *m_colorWheelPanel = nullptr;
    ColorWheel *m_colorWheel = nullptr;
    QColor m_activeColor;
    QHash<QString, QColor> m_colorCache; // filePath -> dominant color
    // Additional filters
    QComboBox *m_sizeFilter = nullptr;   // 全部/小/中/大
    QComboBox *m_formatFilter = nullptr; // 全部/图片/视频/文本
    QComboBox *m_ratingFilter = nullptr; // 全部/≥1/≥2/.../≥5
    // View mode
    enum class ViewMode { Masonry, Grid, List };
    ViewMode m_viewMode = ViewMode::Masonry;
    QToolButton *m_viewModeBtn = nullptr;
    // Floating zoom bar (like canvas)
    QWidget *m_zoomBar = nullptr;
    QToolButton *m_zoomPercentBtn = nullptr;
    static constexpr int kDefaultCardWidth = 134;
    static constexpr int kMinCardWidth = 80;
    static constexpr int kMaxCardWidth = 220;

    // Preview page
    QStackedWidget *m_stack = nullptr;
    QWidget *m_browserPage = nullptr;
    QGraphicsView *m_previewView = nullptr;
    QGraphicsScene *m_previewScene = nullptr;
    QGraphicsPixmapItem *m_previewItem = nullptr;
    QTextEdit *m_textPreview = nullptr;
    QLabel *m_previewInfoLabel = nullptr;
    QLabel *m_previewZoomBadge = nullptr;   // PureRef-style transient zoom % badge
    QTimer *m_zoomBadgeTimer = nullptr;
    QPushButton *m_previewBackBtn = nullptr;
    double m_previewZoom = 1.0;
    bool m_previewFitMode = true;
};

#endif
