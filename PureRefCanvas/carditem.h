#ifndef CARDITEM_H
#define CARDITEM_H

#include <QGraphicsObject>
#include <QTextDocument>
#include <QSizeF>
#include <QString>
#include "canvasnode.h"

// 文本卡片：画布上的便签卡片（白板核心元素）。
// 双击进入编辑（内嵌 QTextEdit 代理）；四角手柄缩放；可移动/选择；
// 可作为链接线的锚点（CanvasNode）。
// 线程安全契约：仅 GUI 线程使用。
class CardItem : public QGraphicsObject, public CanvasNode
{
    Q_OBJECT
public:
    enum Handle { NoHandle, TopLeft, TopRight, BottomLeft, BottomRight };

    explicit CardItem(QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    QString text() const;
    void setText(const QString &t);

    QSizeF cardSize() const { return m_size; }
    void setCardSize(const QSizeF &s);

    // CanvasNode：链接线锚点查询
    QPointF sceneNodeCenter() const override;
    QRectF sceneNodeRect() const override;
    QRectF localNodeRect() const override {
        return QRectF(0, 0, m_size.width(), m_size.height());
    }

    void beginEdit();          // 进入内联编辑态
    void bringToFront();
    void sendToBack();

signals:
    void cardChanged();        // 文本/尺寸变化（用于标记 dirty）

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    Handle handleAt(const QPointF &pos) const;
    QRectF handleRect(Handle h) const;
    void updateCursorForHandle(Handle h);
    void fitHeightToText();    // 编辑后按内容高度自适应（最小 60）

    QSizeF m_size = QSizeF(250.0, 60.0);
    QTextDocument m_doc;
    Handle m_activeHandle = NoHandle;
    QPointF m_pressScenePos;
    QSizeF m_pressSize;
    bool m_moved = false;

    static constexpr qreal HANDLE = 10.0;   // 手柄边长（local px）
    static constexpr qreal PAD = 5.0;       // boundingRect 外扩
    static constexpr qreal MIN_DIM = 50.0;  // 最小宽/高
    static constexpr qreal MARGIN = 10.0;   // 文字内边距
};

#endif // CARDITEM_H
