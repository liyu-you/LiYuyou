#ifndef CONTENTBROWSERPANEL_H
#define CONTENTBROWSERPANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QDrag>
#include <QUrl>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include "materiallibrary.h"

class QLabel;
class QFrame;

// Drag-enabled image list that outputs file URLs
class ImageDragList : public QListWidget
{
    Q_OBJECT
public:
    explicit ImageDragList(QWidget *parent = nullptr) : QListWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setMovement(QListView::Static);
        setSpacing(2);
    }
protected:
    void startDrag(Qt::DropActions supportedActions) override {
        QList<QListWidgetItem*> items = selectedItems();
        if (items.isEmpty()) return;
        QMimeData *mime = mimeData(items);
        if (!mime) return;
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mime);
        // Set drag pixmap preview
        QListWidgetItem *item = items.first();
        if (!item->icon().isNull()) {
            drag->setPixmap(item->icon().pixmap(48, 48));
        }
        drag->exec(supportedActions);
    }

    QMimeData *mimeData(const QList<QListWidgetItem*> &items) const override {
        QMimeData *data = new QMimeData();
        if (!items.isEmpty()) {
            QString path = items.first()->data(Qt::UserRole).toString();
            if (!path.isEmpty()) {
                QList<QUrl> urls;
                urls << QUrl::fromLocalFile(path);
                data->setUrls(urls);
                data->setText(path);
            }
        }
        return data;
    }
};

// Drop-enabled folder tree that imports files to target folder
class FolderDropTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit FolderDropTree(MaterialLibrary *lib, QWidget *parent = nullptr)
        : QTreeWidget(parent), m_lib(lib) {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        viewport()->setAcceptDrops(true);
    }
signals:
    void filesImported(const QStringList &files, const QString &folder);
protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QTreeWidget::dragEnterEvent(event);
        }
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        } else {
            QTreeWidget::dragMoveEvent(event);
        }
    }
    void dropEvent(QDropEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            QStringList files;
            for (const QUrl &url : event->mimeData()->urls()) {
                if (url.isLocalFile()) files << url.toLocalFile();
            }
            if (!files.isEmpty()) {
                QTreeWidgetItem *item = itemAt(event->position().toPoint());
                QString folder;
                if (item) folder = item->data(0, Qt::UserRole).toString();
                if (m_lib) m_lib->importFiles(files, folder);
                emit filesImported(files, folder);
                event->acceptProposedAction();
                return;
            }
        }
        QTreeWidget::dropEvent(event);
    }
private:
    MaterialLibrary *m_lib;
};

class ContentBrowserPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ContentBrowserPanel(MaterialLibrary *sharedLib, QWidget *parent = nullptr);

    QString currentFolder() const { return m_currentFolder; }
    QTreeWidget *tree() const { return m_tree; }
    void refresh();
    void setShowImages(bool show);
    void setTransparent(bool transparent);
    void clearThumbCache() { m_thumbCache.clear(); }

signals:
    void folderChanged(const QString &folderPath);
    void imageActivated(const QString &filePath);

private slots:
    void onTreeClicked(QTreeWidgetItem *item, int col);
    void onContextMenu(const QPoint &pos);
    void onImageDoubleClicked(QListWidgetItem *item);
    void createFolder();
    void deleteFolder();
    void renameFolder();

private:
    void buildTree();
    void refreshImageList();
    QTreeWidgetItem *findItemByPath(const QString &path) const;

    MaterialLibrary *m_lib;
    QString m_currentFolder;
    FolderDropTree *m_tree;
    ImageDragList *m_imageList;
    QLabel *m_imagesLabel;
    QWidget *m_imagesTitleWidget;
    QFrame *m_separator;
    bool m_showImages = false;
    QHash<QString, QPixmap> m_thumbCache; // thumbnail cache by full file path
};

#endif
