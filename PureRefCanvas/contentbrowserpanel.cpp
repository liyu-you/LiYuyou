#include "contentbrowserpanel.h"
#include "thumbcache.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QMap>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QImageReader>
#include <QTimer>
#include <algorithm>

ContentBrowserPanel::ContentBrowserPanel(MaterialLibrary *sharedLib, QWidget *parent)
    : QWidget(parent), m_lib(sharedLib)
{
    setFixedWidth(210);
    setAttribute(Qt::WA_TranslucentBackground);
    setTransparent(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 8, 10);
    layout->setSpacing(8);

    // Title with icon
    QWidget *titleWidget = new QWidget;
    titleWidget->setStyleSheet("background: transparent; border-bottom: 1px solid #E5E7EB;");
    QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(4, 6, 4, 8);
    titleLayout->setSpacing(7);
    QLabel *iconLabel = new QLabel("📁");
    iconLabel->setStyleSheet("font-size: 14px;");
    QLabel *titleLabel = new QLabel("内容浏览器");
    titleLabel->setStyleSheet("color: #1F2933; font-size: 12px; font-weight: 500; letter-spacing: 1px;");
    titleLayout->addWidget(iconLabel);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    layout->addWidget(titleWidget);

    m_tree = new FolderDropTree(m_lib);
    m_tree->setHeaderHidden(true);
    m_tree->setStyleSheet(R"(
        QTreeWidget { background: transparent; border: none; outline: none; color: #374151; font-size: 13px; }
        QTreeWidget::item { padding: 5px 8px; border-radius: 4px; min-height: 24px; }
        QTreeWidget::item:selected { background: #EAF3DE; color: #27500A; }
        QTreeWidget::item:hover:!selected { background: #F3F4F6; color: #111827; }
        QTreeWidget::branch { background: transparent; }
        QTreeWidget::branch:has-children:!has-siblings:closed,
        QTreeWidget::branch:closed:has-children:has-siblings {
            border-image: none; image: none;
        }
    )");
    connect(m_tree, &QTreeWidget::itemClicked, this, &ContentBrowserPanel::onTreeClicked);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ContentBrowserPanel::onContextMenu);
    layout->addWidget(m_tree, 2);

    // Separator (soft gradient hairline)
    QFrame *sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        " stop:0 rgba(229,231,235,0), stop:0.5 #E5E7EB, stop:1 rgba(229,231,235,0));");
    m_separator = sep;
    layout->addWidget(sep);

    // Asset list title (always visible)
    QWidget *imgTitleWidget = new QWidget;
    imgTitleWidget->setStyleSheet("background: transparent;");
    QHBoxLayout *imgTitleLayout = new QHBoxLayout(imgTitleWidget);
    imgTitleLayout->setContentsMargins(4, 8, 4, 4);
    imgTitleLayout->setSpacing(6);
    QLabel *imgIcon = new QLabel("📦");
    imgIcon->setStyleSheet("font-size: 13px;");
    m_imagesLabel = new QLabel("资产 (0)");
    m_imagesLabel->setStyleSheet("color: #6B7280; font-size: 11px; font-weight: 500; letter-spacing: 1px;");
    imgTitleLayout->addWidget(imgIcon);
    imgTitleLayout->addWidget(m_imagesLabel);
    imgTitleLayout->addStretch();
    m_imagesTitleWidget = imgTitleWidget;
    layout->addWidget(imgTitleWidget);

    // Asset list (always visible, shows all asset types)
    m_imageList = new ImageDragList;
    m_imageList->setViewMode(QListView::ListMode);
    m_imageList->setIconSize(QSize(24, 24));
    m_imageList->setResizeMode(QListView::Adjust);
    m_imageList->setSpacing(2);
    m_imageList->setMovement(QListView::Static);
    m_imageList->setStyleSheet(R"(
        QListWidget { background: rgba(0,0,0,0.15); border: 1px solid #2a475e; border-radius: 6px; outline: none; padding: 4px; }
        QListWidget::item { background: transparent; border-radius: 4px; color: #c7d5e0; padding: 4px 6px; font-size: 12px; }
        QListWidget::item:selected { background: #2a475e; color: #66c0f4; }
        QListWidget::item:hover:!selected { background: rgba(42,71,94,0.5); }
    )");
    connect(m_imageList, &QListWidget::itemDoubleClicked, this, &ContentBrowserPanel::onImageDoubleClicked);
    layout->addWidget(m_imageList, 3);

    connect(m_lib, &MaterialLibrary::libraryChanged, this, &ContentBrowserPanel::refresh);
    buildTree();
    refreshImageList();
}

void ContentBrowserPanel::setShowImages(bool show)
{
    m_showImages = show;
    // Asset list is always visible now; this method kept for API compatibility
    if (show) QTimer::singleShot(0, this, &ContentBrowserPanel::refreshImageList);
}

void ContentBrowserPanel::setTransparent(bool transparent)
{
    if (transparent) {
        setStyleSheet("background: transparent; border-right: 1px solid rgba(42, 71, 94, 0.6);");
    } else {
        setStyleSheet("background: rgba(20, 30, 43, 0.72); border-right: 1px solid rgba(42, 71, 94, 0.6);");
    }
}

void ContentBrowserPanel::buildTree()
{
    m_tree->clear();

    QTreeWidgetItem *rootItem = new QTreeWidgetItem({"📁  Library"});
    rootItem->setData(0, Qt::UserRole, QString());
    rootItem->setExpanded(true);
    m_tree->addTopLevelItem(rootItem);

    QMap<QString, QTreeWidgetItem*> itemMap;
    itemMap[""] = rootItem;

    QStringList allPaths = m_lib->allFolderPaths();
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

    if (m_currentFolder.isEmpty()) {
        m_tree->setCurrentItem(rootItem);
    } else {
        QTreeWidgetItem *found = itemMap.value(m_currentFolder, nullptr);
        if (found) m_tree->setCurrentItem(found);
    }
}

void ContentBrowserPanel::refreshImageList()
{
    m_imageList->clear();
    QList<MaterialItem> items = m_lib->itemsInFolder(m_currentFolder);
    int count = 0;
    for (const MaterialItem &mat : items) {
        QListWidgetItem *item = new QListWidgetItem;
        // Explicitly enable drag
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        QString iconText;
        switch (mat.type) {
            case AssetImage: iconText = "🖼"; break;
            case AssetVideo: iconText = "🎬"; break;
            case AssetText:  iconText = "📄"; break;
            default:          iconText = "📦"; break;
        }
        QString fullPath = mat.fullPath(m_lib->rootPath());
        QString displayName = mat.baseName();
        if (displayName.length() > 4) displayName = displayName.left(4) + "…";
        item->setText(iconText + "  " + displayName);
        item->setData(Qt::UserRole, fullPath);
        item->setToolTip(mat.fileName);
        // Set thumbnail for images (cached in memory + disk, downscaled decode for speed)
        if (mat.type == AssetImage) {
            QPixmap thumb = m_thumbCache.value(fullPath);
            if (thumb.isNull())
                thumb = ThumbCache::load(fullPath); // warm disk cache
            if (thumb.isNull()) {
                QImageReader reader(fullPath);
                reader.setAutoTransform(true);
                // Aspect-preserving decode: setScaledSize alone uses IgnoreAspectRatio,
                // so compute the proportional target size first.
                QSize orig = reader.size();
                if (orig.isValid() && !orig.isEmpty()) {
                    const qreal scale = qMin(48.0 / orig.width(), 48.0 / orig.height());
                    reader.setScaledSize(QSize(qMax(1, qRound(orig.width() * scale)),
                                                qMax(1, qRound(orig.height() * scale))));
                } else {
                    reader.setScaledSize(QSize(48, 48));
                }
                QImage img = reader.read();
                if (!img.isNull()) {
                    thumb = QPixmap::fromImage(img);
                    // Bound the memory cache to avoid unbounded growth
                    if (m_thumbCache.size() >= 512)
                        m_thumbCache.clear();
                    m_thumbCache.insert(fullPath, thumb);
                    ThumbCache::save(fullPath, thumb); // persist for next launch
                }
            }
            if (!thumb.isNull()) {
                // Pad onto a transparent 24x24 square so the list view doesn't
                // stretch non-square thumbnails into a square icon.
                QPixmap thumbScaled = thumb.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPixmap square(24, 24);
                square.fill(Qt::transparent);
                QPainter p(&square);
                p.drawPixmap((24 - thumbScaled.width()) / 2, (24 - thumbScaled.height()) / 2, thumbScaled);
                p.end();
                item->setIcon(QIcon(square));
                item->setText(displayName);
            }
        }
        m_imageList->addItem(item);
        count++;
    }
    if (m_imagesLabel) {
        QString folderName = m_currentFolder.isEmpty() ? "Library" : m_currentFolder.section('/', -1);
        m_imagesLabel->setText(QString("%1 (%2)").arg(folderName).arg(count));
    }
}

void ContentBrowserPanel::refresh()
{
    buildTree();
    refreshImageList();
}

void ContentBrowserPanel::onTreeClicked(QTreeWidgetItem *item, int)
{
    if (!item) return;
    m_currentFolder = item->data(0, Qt::UserRole).toString();
    emit folderChanged(m_currentFolder);
    refreshImageList();
}

void ContentBrowserPanel::onImageDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) emit imageActivated(path);
}

void ContentBrowserPanel::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    QString folderPath;
    if (item) {
        folderPath = item->data(0, Qt::UserRole).toString();
    }

    QMenu menu;
    menu.setStyleSheet("QMenu { background: #2a2a2a; color: #ddd; border: 1px solid #444; padding: 4px; } QMenu::item { padding: 6px 20px; border-radius: 3px; } QMenu::item:selected { background: #2a475e; color: #66c0f4; } QMenu::separator { height: 1px; background: #444; margin: 4px 8px; }");

    menu.addAction("新建文件夹", this, [this, folderPath]() {
        bool ok;
        QString name = QInputDialog::getText(this, "新建文件夹", "文件夹名称：", QLineEdit::Normal, "新建文件夹", &ok);
        if (!ok || name.isEmpty()) return;
        QString path = folderPath.isEmpty() ? name : folderPath + "/" + name;
        if (!m_lib->createFolder(path)) {
            QMessageBox::warning(this, "错误", "文件夹已存在或创建失败。");
        }
    });

    if (!folderPath.isEmpty()) {
        menu.addAction("重命名", this, [this, folderPath]() {
            QFileInfo fi(folderPath);
            bool ok;
            QString newName = QInputDialog::getText(this, "重命名", "新名称：", QLineEdit::Normal, fi.fileName(), &ok);
            if (!ok || newName.isEmpty()) return;
            m_lib->renameFolder(folderPath, newName);
        });
        menu.addAction("删除文件夹", this, [this, folderPath]() {
            if (QMessageBox::question(this, "删除", "确定删除此文件夹及其内容？") == QMessageBox::Yes) {
                m_lib->deleteFolder(folderPath);
            }
        });
    }

    menu.exec(m_tree->mapToGlobal(pos));
}

void ContentBrowserPanel::createFolder() {}
void ContentBrowserPanel::deleteFolder() {}
void ContentBrowserPanel::renameFolder() {}

QTreeWidgetItem *ContentBrowserPanel::findItemByPath(const QString &path) const
{
    QTreeWidgetItem *root = m_tree->topLevelItem(0);
    if (!root) return nullptr;
    if (path.isEmpty()) return root;
    QStringList parts = path.split('/');
    QTreeWidgetItem *current = root;
    for (const QString &part : parts) {
        bool found = false;
        for (int i = 0; i < current->childCount(); i++) {
            QTreeWidgetItem *child = current->child(i);
            if (child->text(0).endsWith(part)) {
                current = child;
                found = true;
                break;
            }
        }
        if (!found) return nullptr;
    }
    return current;
}
