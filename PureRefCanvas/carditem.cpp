#include "carditem.h"
#include "canvasscene.h"
#include "annotationitems.h"

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsProxyWidget>
#include <QTextEdit>
#include <QTextCursor>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QMenu>
#include <QKeyEvent>
#include <QTimer>
#include <QPointer>
#include <QEvent>
#include <functional>

namespace {

// 编辑代理的焦点/按键监听：失焦或 Esc 时结束编辑并提交。
class EditFocusWatcher : public QObject
{
public:
    using QObject::QObject;
    std::function<void()> onFinish;

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (e->type() == QEvent::FocusOut) {
            if (onFinish) onFinish();
            return true;
        }
        if (e->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Escape) {
                if (onFinish) onFinish();
                return true;
            }
        }
        return QObject::eventFilter(obj, e);
    }
};

}   // namespace

CardItem::CardItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setAcceptHoverEvents(true);

    QFont font(QStringLiteral("Microsoft YaHei"), 10);
    m_doc.setDefaultFont(font);
    m_doc.setDocumentMargin(0);
}

QRectF CardItem::boundingRect() const
{
    return QRectF(-PAD, -PAD, m_size.width() + PAD * 2, m_size.height() + PAD * 2);
}

QString CardItem::text() const
{
    return m_doc.toPlainText();
}

void CardItem::setText(const QString &t)
{
    if (m_doc.toPlainText() == t)
        return;
    m_doc.setPlainText(t);
    fitHeightToText();
    update();
    emit cardChanged();
}

void CardItem::setCardSize(const QSizeF &s)
{
    prepareGeometryChange();
    m_size = QSizeF(qMax(MIN_DIM, s.width()), qMax(MIN_DIM, s.height()));
    update();
    emit cardChanged();
}

QPointF CardItem::sceneNodeCenter() const
{
    return mapToScene(QPointF(m_size.width() / 2.0, m_size.height() / 2.0));
}

QRectF CardItem::sceneNodeRect() const
{
    return mapToScene(QRectF(0, 0, m_size.width(), m_size.height())).boundingRect();
}

void CardItem::fitHeightToText()
{
    const qreal textW = m_size.width() - MARGIN * 2;
    if (textW <= 0)
        return;
    m_doc.setTextWidth(textW);
    const qreal h = qMax(60.0, m_doc.size().height() + MARGIN * 2);
    if (qAbs(h - m_size.height()) > 0.5) {
        prepareGeometryChange();
        m_size.setHeight(h);
    }
}

void CardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect(0, 0, m_size.width(), m_size.height());
    QPainterPath body;
    body.addRoundedRect(rect, 8.0, 8.0);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillPath(body, QColor(34, 47, 66));
    painter->setPen(QPen(isSelected() ? QColor(102, 192, 244) : QColor(72, 92, 114),
                         isSelected() ? 2.0 : 1.0));
    painter->drawPath(body);

    // 文本内容
    painter->save();
    painter->translate(MARGIN, MARGIN);
    const QRectF clip(0, 0, m_size.width() - MARGIN * 2,
                      m_size.height() - MARGIN * 2);
    m_doc.setTextWidth(clip.width());
    if (text().isEmpty()) {
        painter->setPen(QPen(QColor(120, 138, 158)));
        painter->drawText(clip, Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("双击编辑"));
    } else {
        m_doc.drawContents(painter, clip);
    }
    painter->restore();

    // 选中时绘制缩放手柄
    if (isSelected()) {
        const Handle handles[] = { TopLeft, TopRight, BottomLeft, BottomRight };
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(102, 192, 244));
        for (Handle h : handles)
            painter->drawRect(handleRect(h));
    }
}

QRectF CardItem::handleRect(Handle h) const
{
    const qreal s = HANDLE;
    switch (h) {
    case TopLeft:
        return QRectF(-s / 2, -s / 2, s, s);
    case TopRight:
        return QRectF(m_size.width() - s / 2, -s / 2, s, s);
    case BottomLeft:
        return QRectF(-s / 2, m_size.height() - s / 2, s, s);
    case BottomRight:
        return QRectF(m_size.width() - s / 2, m_size.height() - s / 2, s, s);
    default:
        return QRectF();
    }
}

CardItem::Handle CardItem::handleAt(const QPointF &pos) const
{
    if (!isSelected())
        return NoHandle;
    const Handle handles[] = { TopLeft, TopRight, BottomLeft, BottomRight };
    for (Handle h : handles) {
        if (handleRect(h).contains(pos))
            return h;
    }
    return NoHandle;
}

void CardItem::updateCursorForHandle(Handle h)
{
    switch (h) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    default:
        setCursor(Qt::SizeAllCursor);
        break;
    }
}

void CardItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    updateCursorForHandle(handleAt(event->pos()));
    QGraphicsObject::hoverMoveEvent(event);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_activeHandle = handleAt(event->pos());
        m_pressScenePos = event->scenePos();
        m_pressSize = m_size;
        m_moved = false;
        if (m_activeHandle != NoHandle) {
            event->accept();
            return;
        }
    }
    QGraphicsObject::mousePressEvent(event);
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle != NoHandle && (event->buttons() & Qt::LeftButton)) {
        m_moved = true;
        const QPointF delta = mapFromScene(event->scenePos())
                              - mapFromScene(m_pressScenePos);
        qreal newW = m_pressSize.width();
        qreal newH = m_pressSize.height();

        auto clampSize = [](qreal v) { return qMax(MIN_DIM, v); };

        switch (m_activeHandle) {
        case TopLeft: {
            newW = clampSize(m_pressSize.width() - delta.x());
            newH = clampSize(m_pressSize.height() - delta.y());
            const QPointF fixed = mapToScene(m_pressSize.width(), m_pressSize.height());
            prepareGeometryChange();
            m_size = QSizeF(newW, newH);
            const QPointF nowFixed = mapToScene(m_size.width(), m_size.height());
            moveBy(fixed.x() - nowFixed.x(), fixed.y() - nowFixed.y());
            break;
        }
        case TopRight: {
            newW = clampSize(m_pressSize.width() + delta.x());
            newH = clampSize(m_pressSize.height() - delta.y());
            const QPointF fixed = mapToScene(0, m_pressSize.height());
            prepareGeometryChange();
            m_size = QSizeF(newW, newH);
            const QPointF nowFixed = mapToScene(0, m_size.height());
            moveBy(fixed.x() - nowFixed.x(), fixed.y() - nowFixed.y());
            break;
        }
        case BottomLeft: {
            newW = clampSize(m_pressSize.width() - delta.x());
            newH = clampSize(m_pressSize.height() + delta.y());
            const QPointF fixed = mapToScene(m_pressSize.width(), 0);
            prepareGeometryChange();
            m_size = QSizeF(newW, newH);
            const QPointF nowFixed = mapToScene(m_size.width(), 0);
            moveBy(fixed.x() - nowFixed.x(), fixed.y() - nowFixed.y());
            break;
        }
        case BottomRight:
            prepareGeometryChange();
            m_size = QSizeF(clampSize(m_pressSize.width() + delta.x()),
                            clampSize(m_pressSize.height() + delta.y()));
            break;
        default:
            break;
        }
        update();
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle != NoHandle) {
        m_activeHandle = NoHandle;
        // 手动缩放尊重用户设定的尺寸（不再回缩；之后编辑文本才重新自适应）
        if (m_moved)
            emit cardChanged();
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void CardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        beginEdit();
        event->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void CardItem::beginEdit()
{
    // 已在编辑态（代理存在）则不重复创建
    if (findChild<QGraphicsProxyWidget *>() != nullptr)
        return;

    auto *edit = new QTextEdit;
    edit->setPlainText(text());
    edit->setFrameStyle(QFrame::NoFrame);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setStyleSheet(
        "QTextEdit { background: transparent; color: #e6edf3; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background: rgba(120,140,160,0.6);"
        " border-radius: 4px; }");
    edit->setFixedSize(QSizeF(m_size.width() + MARGIN * 2,
                              m_size.height() + MARGIN * 2)
                           .toSize());

    auto *proxy = new QGraphicsProxyWidget(this);
    proxy->setWidget(edit);
    proxy->setPos(-MARGIN, -MARGIN);

    QPointer<CardItem> guard(this);
    QPointer<QGraphicsProxyWidget> proxyGuard(proxy);
    auto finish = [guard, proxyGuard, edit]() {
        // 延迟到事件循环，避免在 FocusOut 中删除部件
        QTimer::singleShot(0, [guard, proxyGuard, edit]() {
            const QString t = edit->toPlainText();
            if (proxyGuard)
                delete proxyGuard.data();
            if (guard) {
                guard->m_doc.setPlainText(t);
                guard->fitHeightToText();
                guard->update();
                emit guard->cardChanged();
            }
        });
    };

    auto *watcher = new EditFocusWatcher(edit);
    watcher->onFinish = finish;
    edit->installEventFilter(watcher);

    // 编辑过程中实时同步文本与高度
    connect(edit, &QTextEdit::textChanged, this, [this, edit]() {
        m_doc.setPlainText(edit->toPlainText());
        fitHeightToText();
        edit->setFixedSize(QSizeF(m_size.width() + MARGIN * 2,
                                  m_size.height() + MARGIN * 2)
                               .toSize());
        update();
        emit cardChanged();
    });

    edit->setFocus();
    QTextCursor cur = edit->textCursor();
    cur.movePosition(QTextCursor::End);
    edit->setTextCursor(cur);
}

void CardItem::bringToFront()
{
    if (auto *cs = dynamic_cast<CanvasScene *>(scene()))
        setZValue(cs->nextZ());
}

void CardItem::sendToBack()
{
    if (auto *cs = dynamic_cast<CanvasScene *>(scene()))
        setZValue(cs->lowestZ());
}

void CardItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    setSelected(true);
    QMenu menu;
    QAction *topAct = menu.addAction(QStringLiteral("置于顶层"));
    QAction *bottomAct = menu.addAction(QStringLiteral("置于底层"));

    QAction *linkAct = nullptr;
    if (auto *cs = dynamic_cast<CanvasScene *>(scene())) {
        linkAct = menu.addAction(QStringLiteral("创建链接"));

        const QList<LinkItem *> attached = cs->linksForNode(this);
        if (!attached.isEmpty()) {
            QMenu *widthMenu = menu.addMenu(QStringLiteral("线条粗细"));
            for (const auto &preset : LinkItem::widthPresets()) {
                bool allMatch = true;
                for (LinkItem *l : attached) {
                    if (qAbs(l->width() - preset.second) > 0.01) {
                        allMatch = false;
                        break;
                    }
                }
                const QList<LinkItem *> targets = attached;
                const qreal w = preset.second;
                QAction *act = widthMenu->addAction(preset.first, [cs, targets, w]() {
                    cs->setLinkWidth(w);
                    for (LinkItem *l : targets)
                        l->setWidth(w);
                });
                act->setCheckable(true);
                act->setChecked(allMatch);
            }
        }
    }

    menu.addSeparator();
    QAction *delAct = menu.addAction(QStringLiteral("删除"));

    QAction *chosen = menu.exec(event->screenPos());
    if (chosen == topAct) {
        bringToFront();
    } else if (chosen == bottomAct) {
        sendToBack();
    } else if (chosen == linkAct) {
        if (auto *cs = dynamic_cast<CanvasScene *>(scene()))
            emit cs->linkStartRequested(this);
    } else if (chosen == delAct) {
        if (auto *cs = dynamic_cast<CanvasScene *>(scene())) {
            setSelected(true);
            cs->removeSelectedItems();
        }
    }
}

QVariant CardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        if (auto *cs = dynamic_cast<CanvasScene *>(scene()))
            cs->refreshLinksFor(this);
    }
    return QGraphicsObject::itemChange(change, value);
}
