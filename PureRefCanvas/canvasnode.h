#ifndef CANVASNODE_H
#define CANVASNODE_H

#include <QPointF>
#include <QRectF>

class QGraphicsItem;

// 画布节点统一接口：图片、文本卡片、分组等可被链接线吸附的元素。
// 纯接口（非 QObject），由具体 QGraphicsItem 多继承实现。
// 线程安全契约：所有方法仅在 GUI 线程调用，读取的是当前场景变换下的几何。
class CanvasNode
{
public:
    virtual ~CanvasNode() = default;

    // 节点内容（不含手柄/内边距）在场景坐标中的中心点。
    virtual QPointF sceneNodeCenter() const = 0;

    // 节点内容（不含手柄/内边距）在场景坐标中的矩形。
    virtual QRectF sceneNodeRect() const = 0;

    // 节点内容在自身 item 坐标系中的矩形（链接线求射线命中边用）。
    virtual QRectF localNodeRect() const = 0;
};

// 从任意场景元素取 CanvasNode 接口（非节点返回 nullptr）。
CanvasNode *canvasNodeOf(QGraphicsItem *item);
const CanvasNode *canvasNodeOf(const QGraphicsItem *item);

#endif // CANVASNODE_H
