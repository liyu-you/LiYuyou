#pragma once
// A simple 2D "digital garden" view: notes as nodes, [[wiki-links]] as edges.
#include <QDialog>
#include <QGraphicsView>
#include <QMap>

class QGraphicsScene;
class QGraphicsEllipseItem;
class QTimer;

struct NoteData;

class GardenView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GardenView(QWidget *parent = nullptr);
    void buildGraph(const QList<NoteData> &notes);
signals:
    void openNoteRequested(const QString &noteId);
};

class GardenDialog : public QDialog {
    Q_OBJECT
public:
    explicit GardenDialog(QWidget *parent = nullptr);
    void setNotes(const QList<NoteData> &notes);
signals:
    void openNoteRequested(const QString &noteId);
};
