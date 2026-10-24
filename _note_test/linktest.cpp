// Canvas link-line tests (offscreen).
// Covers LinkItem edge geometry/anchors, bezier+arrow construction, image-move
// follow, anchor deletion fallback, scene serialization roundtrip (incl.
// old-format compatibility), and delete/clear integration.
#include <QApplication>
#include <QPixmap>
#include <QBuffer>
#include <QDataStream>
#include <QPointF>
#include <QColor>
#include <QTransform>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <cstdio>

#include "canvasscene.h"
#include "canvasview.h"
#include "imageitem.h"
#include "annotationitems.h"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond) do { \
    if (cond) { ++g_passed; printf("PASS: %s\n", #cond); } \
    else { ++g_failed; printf("FAIL: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); } \
} while (0)

static QPixmap makePix(int w, int h)
{
    QPixmap pm(w, h);
    pm.fill(QColor(200, 60, 60));
    return pm;
}

static bool near(const QPointF &a, const QPointF &b, qreal eps = 0.001)
{
    return qAbs(a.x() - b.x()) <= eps && qAbs(a.y() - b.y()) <= eps;
}

// 手工构造“旧版”画布数据（只有图片、无链接段），用于验证向后兼容。
static QByteArray oldFormatCanvas()
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QDataStream out(&buffer);
    out.setVersion(QDataStream::Qt_6_0);

    out << (quint32)1;   // 1 image
    QByteArray png;
    QBuffer pngBuf(&png);
    pngBuf.open(QIODevice::WriteOnly);
    makePix(60, 40).save(&pngBuf, "PNG");
    out << png;
    out << (qreal)12.0 << (qreal)34.0 << (qreal)0.0 << (qreal)1.0 << (qreal)0.0
        << QTransform();
    return data;
}

// 构造「锚点序号越界」的画布（如锚图已不在图片段），验证回退为自由坐标。
static QByteArray canvasWithBadAnchor()
{
    QByteArray data = oldFormatCanvas();
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    buffer.seek(buffer.size());
    QDataStream out(&buffer);
    out.setVersion(QDataStream::Qt_6_0);

    out << (quint32)1;   // 1 link
    out << (qreal)1.0 << (qreal)1.0 << (qreal)1.0;   // color: white
    out << (qreal)2.0 << (qreal)5.0;                 // width, z
    out << (bool)true << (qint32)99;                 // anchoredA, idx 越界
    out << (qreal)7.0 << (qreal)8.0;                 // ptA
    out << (bool)false << (qint32)-1;                // anchoredB
    out << (qreal)70.0 << (qreal)80.0;               // ptB
    return data;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // ---- LinkItem：自由端点 / 颜色 / 线宽 / 默认灰色 ----
    {
        LinkItem link(QPointF(10, 20), QPointF(130, 90));
        CHECK(near(link.edgePointA(), QPointF(10, 20)));
        CHECK(near(link.edgePointB(), QPointF(130, 90)));
        CHECK(!link.isAnchored(LinkItem::EndA));
        CHECK(!link.isAnchored(LinkItem::EndB));
        CHECK(link.color() == QColor(170, 176, 182));   // 默认灰色
        CHECK(link.width() == 2.5);                     // 默认标准粗细

        link.setFreePoint(LinkItem::EndA, QPointF(0, 0));
        CHECK(near(link.edgePointA(), QPointF(0, 0)));
        CHECK(!link.isAnchored(LinkItem::EndA));   // 解除吸附后仍为自由点

        link.setColor(QColor(1, 2, 3));
        link.setWidth(5.0);
        CHECK(link.color() == QColor(1, 2, 3));
        CHECK(link.width() == 5.0);
    }

    // ---- 贝塞尔构造：异边时曲线偏离直线；箭头含端点 ----
    {
        // 水平对：两边同法线 → 曲线退化为直线
        QPainterPath straight = LinkItem::makeConnector(
            QPointF(0, 0), LinkItem::RightEdge, QPointF(300, 0), LinkItem::LeftEdge);
        CHECK(near(straight.pointAtPercent(0.5), QPointF(150, 0)));

        // 垂直对：上下边
        QPainterPath vert = LinkItem::makeConnector(
            QPointF(0, 0), LinkItem::BottomEdge, QPointF(0, 300), LinkItem::TopEdge);
        CHECK(near(vert.pointAtPercent(0.5), QPointF(0, 150)));

        // 异边（右→上）：中点偏离直线中点，确为弯曲
        QPainterPath bend = LinkItem::makeConnector(
            QPointF(0, 0), LinkItem::RightEdge, QPointF(100, 100), LinkItem::TopEdge);
        CHECK(!near(bend.pointAtPercent(0.5), QPointF(50, 50)));

        // dominantEdge：水平/垂直/对角线判定
        CHECK(LinkItem::dominantEdge(QPointF(0, 0), QPointF(100, 10)) == LinkItem::RightEdge);
        CHECK(LinkItem::dominantEdge(QPointF(0, 0), QPointF(-100, 10)) == LinkItem::LeftEdge);
        CHECK(LinkItem::dominantEdge(QPointF(0, 0), QPointF(10, 100)) == LinkItem::BottomEdge);
        CHECK(LinkItem::dominantEdge(QPointF(0, 0), QPointF(10, -100)) == LinkItem::TopEdge);
    }

    // ---- 场景：锚点吸附到图片边缘 + 图片移动跟随 + 锚图删除回退 ----
    {
        CanvasScene scene;
        ImageItem *img = scene.addImage(makePix(100, 100), QPointF(0, 0));
        CHECK(img != nullptr);
        const QPointF center = img->sceneContentCenter();
        CHECK(near(center, QPointF(50, 50)));

        // 对端正右方 → 线端落在右边缘中心（不是图片中心）
        LinkItem *link = scene.createLink(img, center, nullptr, QPointF(500, 50));
        CHECK(link != nullptr);
        CHECK(link->isAnchored(LinkItem::EndA));
        CHECK(!link->isAnchored(LinkItem::EndB));
        CHECK(near(link->edgePointA(), QPointF(100, 50)));
        CHECK(near(link->edgePointB(), QPointF(500, 50)));
        // 箭头三角形包含 B 端
        CHECK(link->arrowPath().boundingRect().contains(QPointF(500, 50)));

        // 图片移动 → 吸附端跟随（itemPositionChanged 同步）
        img->setPos(300, 0);
        CHECK(near(link->centerA(), QPointF(350, 50)));
        CHECK(near(link->edgePointA(), QPointF(400, 50)));

        // 锚图删除（真实删除路径）→ 端点回退为最后边缘点
        img->setSelected(true);
        scene.removeSelectedItems();
        CHECK(link->anchorItem(LinkItem::EndA) == nullptr);
        CHECK(!link->isAnchored(LinkItem::EndA));
        CHECK(near(link->edgePointA(), QPointF(400, 50)));   // 停在最后边缘
        CHECK(scene.imageItems().isEmpty());
    }

    // ---- 场景默认粗细：新链接采用，选中链接可实时改 ----
    {
        CanvasScene scene;
        CHECK(scene.linkWidth() == 2.5);
        ImageItem *img = scene.addImage(makePix(100, 100), QPointF(0, 0));
        LinkItem *l1 = scene.createLink(img, img->sceneContentCenter(),
                                        nullptr, QPointF(500, 50));
        CHECK(l1->width() == 2.5);

        // 改默认粗细 → 只影响之后新建的链接
        scene.setLinkWidth(5.0);
        CHECK(scene.linkWidth() == 5.0);
        LinkItem *l2 = scene.createLink(nullptr, QPointF(0, 600),
                                        nullptr, QPointF(500, 600));
        CHECK(l2->width() == 5.0);
        CHECK(l1->width() == 2.5);   // 旧链接不受影响

        // 粗细预设：右键菜单与工具栏共用同一组（4 档）
        CHECK(LinkItem::widthPresets().size() == 4);

        // 选中链接后 applyWidth（右键菜单同款）→ 实时改并同步场景默认
        l1->setSelected(true);
        LinkItem::applyWidth(l1, 4.0);
        CHECK(l1->width() == 4.0);
        CHECK(scene.linkWidth() == 4.0);

        // 未选中的链接 applyWidth → 只改自身
        l1->setSelected(false);
        LinkItem::applyWidth(l2, 6.0);
        CHECK(l2->width() == 6.0);
        CHECK(scene.linkWidth() == 6.0);
        CHECK(l1->width() == 4.0);

        // 多选时 applyWidth → 批量应用到全部选中链接
        l1->setSelected(true);
        l2->setSelected(true);
        LinkItem::applyWidth(l1, 1.5);
        CHECK(l1->width() == 1.5);
        CHECK(l2->width() == 1.5);
    }

    // ---- 图片右键「创建链接」：视图待选第二端（点击完成 / Esc 取消） ----
    {
        CanvasScene scene;
        CanvasView view(&scene);
        view.resize(1200, 800);
        ImageItem *i0 = scene.addImage(makePix(100, 100), QPointF(0, 0));
        ImageItem *i1 = scene.addImage(makePix(100, 100), QPointF(400, 0));

        auto sendClick = [&view](const QPointF &scenePt) {
            const QPoint local = view.mapFromScene(scenePt);
            QMouseEvent press(QEvent::MouseButtonPress, local, Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            // 鼠标事件必须投递给视口部件，QGraphicsView 才会收到 mousePressEvent
            QApplication::sendEvent(view.viewport(), &press);
        };

        // 模拟右键菜单「创建链接」：直接发场景信号
        emit scene.linkStartRequested(i0);
        // 第二击落在第二张图 → 完成双锚定链接
        sendClick(i1->sceneContentCenter());
        CHECK(scene.links().size() == 1);
        CHECK(scene.links().first()->anchorItem(LinkItem::EndA) == i0);
        CHECK(scene.links().first()->anchorItem(LinkItem::EndB) == i1);

        // 再次启动后 Esc 取消 → 不产生链接
        emit scene.linkStartRequested(i0);
        QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&view, &esc);
        CHECK(scene.links().size() == 1);
        // 取消后点击空白处 → 走选择模式，不产生链接
        sendClick(QPointF(700, 500));
        CHECK(scene.links().size() == 1);

        // 第二击落在空白处 → 自由端链接
        emit scene.linkStartRequested(i0);
        sendClick(QPointF(700, 500));
        CHECK(scene.links().size() == 2);
        CHECK(scene.links().last()->anchorItem(LinkItem::EndA) == i0);
        CHECK(!scene.links().last()->isAnchored(LinkItem::EndB));
    }

    // ---- linksForImage：图片右键菜单的作用范围 ----
    {
        CanvasScene scene;
        ImageItem *i0 = scene.addImage(makePix(100, 100), QPointF(0, 0));
        ImageItem *i1 = scene.addImage(makePix(100, 100), QPointF(400, 0));
        LinkItem *l1 = scene.createLink(i0, i0->sceneContentCenter(),
                                        i1, i1->sceneContentCenter());
        LinkItem *l2 = scene.createLink(i0, i0->sceneContentCenter(),
                                        nullptr, QPointF(0, 500));
        LinkItem *l3 = scene.createLink(nullptr, QPointF(0, 700),
                                        nullptr, QPointF(500, 700));
        // i0 吸附了 l1（A 端）和 l2（A 端）
        CHECK(scene.linksForImage(i0).size() == 2);
        CHECK(scene.linksForImage(i0).contains(l1));
        CHECK(scene.linksForImage(i0).contains(l2));
        // i1 只吸附 l1（B 端）
        CHECK(scene.linksForImage(i1).size() == 1);
        CHECK(scene.linksForImage(i1).contains(l1));
        // 自由链接 l3 不属于任何图片
        CHECK(!scene.linksForImage(i0).contains(l3));
        CHECK(!scene.linksForImage(i1).contains(l3));

        // 模拟图片右键菜单「粗」：吸附该图的链接全部变粗
        const QList<LinkItem *> targets = scene.linksForImage(i0);
        for (LinkItem *l : targets)
            l->setWidth(4.0);
        CHECK(l1->width() == 4.0);
        CHECK(l2->width() == 4.0);
        CHECK(l3->width() == 2.5);   // 未吸附 i0 的链接不受影响
    }

    // ---- 垂直锚点：上下排列时线端落在上下边 ----
    {
        CanvasScene scene;
        ImageItem *i0 = scene.addImage(makePix(100, 100), QPointF(0, 0));
        ImageItem *i1 = scene.addImage(makePix(100, 100), QPointF(0, 400));
        LinkItem *link = scene.createLink(i0, i0->sceneContentCenter(),
                                          i1, i1->sceneContentCenter());
        CHECK(near(link->edgePointA(), QPointF(50, 100)));   // i0 底边
        CHECK(near(link->edgePointB(), QPointF(50, 400)));   // i1 顶边
    }

    // ---- 序列化往返：锚定/自由/混合三类链接 + 颜色线宽 ----
    {
        CanvasScene s1;
        ImageItem *i0 = s1.addImage(makePix(100, 100), QPointF(0, 0));
        ImageItem *i1 = s1.addImage(makePix(100, 100), QPointF(400, 0));
        const QPointF c0 = i0->sceneContentCenter();   // (50, 50)
        const QPointF c1 = i1->sceneContentCenter();   // (450, 50)

        LinkItem *l1 = s1.createLink(i0, c0, i1, c1);                                  // 双锚定
        LinkItem *l2 = s1.createLink(nullptr, QPointF(10, 10), nullptr, QPointF(300, 300)); // 双自由
        LinkItem *l3 = s1.createLink(i1, c1, nullptr, QPointF(700, 50));               // 混合
        l1->setWidth(3.0);
        l2->setColor(QColor(0, 200, 0));
        CHECK(s1.links().size() == 3);

        CanvasScene s2;
        CHECK(s2.loadState(s1.saveState()));
        CHECK(s2.links().size() == 3);
        CHECK(s2.imageItems().size() == 2);
        // imageItems() 按层叠序（z 降序）返回，不依赖顺序：按位置定位两图
        const QList<ImageItem *> imgs2 = s2.imageItems();
        ImageItem *imgAt0 = nullptr, *imgAt400 = nullptr;
        for (ImageItem *im : imgs2) {
            if (near(im->pos(), QPointF(0, 0)))     imgAt0 = im;
            if (near(im->pos(), QPointF(400, 0)))   imgAt400 = im;
        }
        CHECK(imgAt0 != nullptr);
        CHECK(imgAt400 != nullptr);

        // 双锚定：线端 = 对应图片边缘（load 后重新吸附）
        CHECK(near(s2.links()[0]->edgePointA(), QPointF(100, 50)));
        CHECK(near(s2.links()[0]->edgePointB(), QPointF(400, 50)));
        CHECK(s2.links()[0]->isAnchored(LinkItem::EndA));
        CHECK(s2.links()[0]->isAnchored(LinkItem::EndB));
        CHECK(s2.links()[0]->width() == 3.0);

        // 双自由：端点坐标保留
        CHECK(near(s2.links()[1]->edgePointA(), QPointF(10, 10)));
        CHECK(near(s2.links()[1]->edgePointB(), QPointF(300, 300)));
        CHECK(s2.links()[1]->color() == QColor(0, 200, 0));

        // 混合：一端锚定（边缘）、一端自由
        CHECK(s2.links()[2]->isAnchored(LinkItem::EndA));
        CHECK(!s2.links()[2]->isAnchored(LinkItem::EndB));
        CHECK(near(s2.links()[2]->edgePointA(), QPointF(500, 50)));
        CHECK(near(s2.links()[2]->edgePointB(), QPointF(700, 50)));

        // 加载后的链接仍跟随图片移动（目标点避开智能对齐阈值：SNAP=8px）
        imgAt400->setPos(600, 0);
        CHECK(near(s2.links()[0]->edgePointB(), QPointF(600, 50)));
        CHECK(near(s2.links()[2]->edgePointA(), QPointF(700, 50)));
    }

    // ---- 旧版画布（无链接段）兼容：正常加载图片、无链接 ----
    {
        CanvasScene scene;
        CHECK(scene.loadState(oldFormatCanvas()));
        CHECK(scene.imageItems().size() == 1);
        CHECK(scene.links().isEmpty());
        CHECK(near(scene.imageItems().first()->pos(), QPointF(12, 34)));
    }

    // ---- 锚点序号越界：回退为保存的自由坐标，不崩溃 ----
    {
        CanvasScene scene;
        CHECK(scene.loadState(canvasWithBadAnchor()));
        CHECK(scene.imageItems().size() == 1);
        CHECK(scene.links().size() == 1);
        CHECK(!scene.links().first()->isAnchored(LinkItem::EndA));
        CHECK(!scene.links().first()->isAnchored(LinkItem::EndB));
        CHECK(near(scene.links().first()->edgePointA(), QPointF(7, 8)));
        CHECK(near(scene.links().first()->edgePointB(), QPointF(70, 80)));
    }

    // ---- 删除 / 全选 / 清空 与链接列表一致 ----
    {
        CanvasScene scene;
        ImageItem *i0 = scene.addImage(makePix(100, 100), QPointF(0, 0));
        ImageItem *i1 = scene.addImage(makePix(100, 100), QPointF(400, 0));
        LinkItem *link = scene.createLink(i0, i0->sceneContentCenter(),
                                          i1, i1->sceneContentCenter());
        CHECK(scene.links().size() == 1);

        // 选中链接 → 删除 → 列表同步清空，图片保留
        link->setSelected(true);
        scene.removeSelectedItems();
        CHECK(scene.links().isEmpty());
        CHECK(scene.imageItems().size() == 2);

        // 全选：图片与链接都被选中
        LinkItem *l2 = scene.createLink(nullptr, QPointF(0, 0), nullptr, QPointF(10, 10));
        scene.selectAllItems();
        CHECK(l2->isSelected());
        CHECK(scene.imageItems().first()->isSelected());

        // 清空：列表与场景全部清空
        scene.clearAll();
        CHECK(scene.links().isEmpty());
        CHECK(scene.items().isEmpty());
    }

    printf("\n=== %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
