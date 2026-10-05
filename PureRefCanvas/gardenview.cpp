#include "gardenview.h"
#include "notewidget.h"  // NoteData
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- GardenView ---
GardenView::GardenView(QWidget *parent) : QGraphicsView(parent)
{
    QGraphicsScene *sc = new QGraphicsScene(this);
    sc->setSceneRect(-800, -600, 1600, 1200);
    setScene(sc);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setBackgroundBrush(QColor("#fafafa"));
}

void GardenView::buildGraph(const QList<NoteData> &notes)
{
    scene()->clear();
    QMap<QString, QGraphicsEllipseItem*> nodeMap;
    // Place nodes in a rough circle
    const int N = notes.size();
    for (int i = 0; i < N; ++i) {
        const NoteData &n = notes.at(i);
        double angle = 2.0 * M_PI * i / qMax(1, N);
        double r = 200 + (i % 5) * 60;
        qreal x = r * qCos(angle);
        qreal y = r * qSin(angle);
        QGraphicsEllipseItem *dot = scene()->addEllipse(x-30, y-15, 60, 30,
            QPen(Qt::NoPen), QBrush(QColor("#1a73e8")));
        dot->setFlag(QGraphicsItem::ItemIsMovable);
        dot->setFlag(QGraphicsItem::ItemIsSelectable);
        dot->setData(Qt::UserRole, n.id);
        QGraphicsTextItem *lbl = scene()->addText(n.title.isEmpty() ? "无标题" : n.title);
        lbl->setPos(x - lbl->boundingRect().width()/2, y + 20);
        nodeMap[n.id] = dot;
    }
    // Draw edges: find [[links]] in note content
    QRegularExpression re("\\[\\[([^\\]]+)\\]\\]");
    for (const NoteData &n : notes) {
        auto it = re.globalMatch(n.content);
        while (it.hasNext()) {
            const QString linkName = it.next().captured(1);
            // find target note by title
            for (const NoteData &target : notes) {
                if (target.title.compare(linkName, Qt::CaseInsensitive) == 0) {
                    if (nodeMap.contains(n.id) && nodeMap.contains(target.id)) {
                        QGraphicsEllipseItem *a = nodeMap[n.id];
                        QGraphicsEllipseItem *b = nodeMap[target.id];
                        scene()->addLine(a->rect().center().x() + a->x(),
                                         a->rect().center().y() + a->y(),
                                         b->rect().center().x() + b->x(),
                                         b->rect().center().y() + b->y(),
                                         QPen(QColor("#dadce0")));
                    }
                }
            }
        }
    }
}

// --- GardenDialog ---
GardenDialog::GardenDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("🌱 数字花园 — 笔记关系图");
    resize(1000, 700);
    QVBoxLayout *lay = new QVBoxLayout(this);
    GardenView *v = new GardenView(this);
    lay->addWidget(v);
    QLabel *hint = new QLabel("拖拽节点移动 · 滚轮缩放 · 双击节点打开笔记");
    hint->setStyleSheet("color:#888; font-size:12px; padding:6px;");
    lay->addWidget(hint);
}

void GardenDialog::setNotes(const QList<NoteData> &notes)
{
    GardenView *v = findChild<GardenView*>();
    if (v) v->buildGraph(notes);
}
