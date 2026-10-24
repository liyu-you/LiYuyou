#include "canvasnode.h"

#include <QGraphicsItem>

CanvasNode *canvasNodeOf(QGraphicsItem *item)
{
    return item ? dynamic_cast<CanvasNode *>(item) : nullptr;
}

const CanvasNode *canvasNodeOf(const QGraphicsItem *item)
{
    return item ? dynamic_cast<const CanvasNode *>(item) : nullptr;
}
