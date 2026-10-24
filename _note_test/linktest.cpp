// Canvas link-line tests (offscreen).
// Covers LinkItem geometry/anchors, image-move follow, anchor deletion fallback,
// scene serialization roundtrip (incl. old-format compatibility), and
// delete/clear integration.
#include <QApplication>
#include <QPixmap>
#include <QBuffer>
#include <QDataStream>
#include <QPointF>
#include <QColor>
#include <QTransform>
#include <cstdio>

#include "canvasscene.h"
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

    // ---- LinkItem：自由端点 / 颜色 / 线宽 ----
    {
        LinkItem link(QPointF(10, 20), QPointF(130, 90));
        CHECK(near(link.pointA(), QPointF(10, 20)));
        CHECK(near(link.pointB(), QPointF(130, 90)));
        CHECK(!link.isAnchored(LinkItem::EndA));
        CHECK(!link.isAnchored(LinkItem::EndB));

        link.setFreePoint(LinkItem::EndA, QPointF(0, 0));
        CHECK(near(link.pointA(), QPointF(0, 0)));
        CHECK(!link.isAnchored(LinkItem::EndA));   // 解除吸附后仍为自由点

        link.setColor(QColor(1, 2, 3));
        link.setWidth(5.0);
        CHECK(link.color() == QColor(1, 2, 3));
        CHECK(link.width() == 5.0);
    }

    // ---- 场景：锚点吸附 + 图片移动跟随 + 锚图删除回退 ----
    {
        CanvasScene scene;
        ImageItem *img = scene.addImage(makePix(100, 80), QPointF(0, 0));
        CHECK(img != nullptr);
        const QPointF center = img->sceneContentCenter();
        CHECK(near(center, QPointF(50, 40)));

        LinkItem *link = scene.createLink(img, center, nullptr, QPointF(500, 500));
        CHECK(link != nullptr);
        CHECK(link->isAnchored(LinkItem::EndA));
        CHECK(!link->isAnchored(LinkItem::EndB));
        CHECK(near(link->pointA(), center));
        CHECK(near(link->pointB(), QPointF(500, 500)));

        // 图片移动 → 吸附端跟随（itemPositionChanged 同步）
        img->setPos(300, 200);
        CHECK(near(link->pointA(), QPointF(350, 240)));

        // 锚图删除（真实删除路径）→ 端点回退为最后已知位置
        img->setSelected(true);
        scene.removeSelectedItems();
        CHECK(link->anchorItem(LinkItem::EndA) == nullptr);
        CHECK(!link->isAnchored(LinkItem::EndA));
        CHECK(near(link->pointA(), QPointF(350, 240)));   // 保持在最后已知位置
        CHECK(scene.imageItems().isEmpty());
    }

    // ---- 序列化往返：锚定/自由/混合三类链接 + 颜色线宽 ----
    {
        CanvasScene s1;
        ImageItem *i0 = s1.addImage(makePix(100, 80), QPointF(0, 0));
        ImageItem *i1 = s1.addImage(makePix(120, 60), QPointF(200, 100));
        const QPointF c0 = i0->sceneContentCenter();   // (50, 40)
        const QPointF c1 = i1->sceneContentCenter();   // (260, 130)

        LinkItem *l1 = s1.createLink(i0, c0, i1, c1);                 // 双锚定
        LinkItem *l2 = s1.createLink(nullptr, QPointF(10, 10), nullptr, QPointF(300, 300)); // 双自由
        LinkItem *l3 = s1.createLink(i1, c1, nullptr, QPointF(400, 400));                 // 混合
        l1->setWidth(3.0);
        l2->setColor(QColor(0, 200, 0));
        CHECK(s1.links().size() == 3);

        CanvasScene s2;
        CHECK(s2.loadState(s1.saveState()));
        CHECK(s2.links().size() == 3);
        CHECK(s2.imageItems().size() == 2);
        // imageItems() 按层叠序（z 降序）返回，不依赖顺序：按位置定位两图
        const QList<ImageItem *> imgs2 = s2.imageItems();
        ImageItem *imgAt0 = nullptr, *imgAt200 = nullptr;
        for (ImageItem *im : imgs2) {
            if (near(im->pos(), QPointF(0, 0)))      imgAt0 = im;
            if (near(im->pos(), QPointF(200, 100)))  imgAt200 = im;
        }
        CHECK(imgAt0 != nullptr);
        CHECK(imgAt200 != nullptr);

        // 双锚定：端点 = 对应图片内容中心（图片在 load 后重新吸附）
        CHECK(near(s2.links()[0]->pointA(), imgAt0->sceneContentCenter()));       // (50,40)
        CHECK(near(s2.links()[0]->pointB(), imgAt200->sceneContentCenter()));     // (260,130)
        CHECK(s2.links()[0]->isAnchored(LinkItem::EndA));
        CHECK(s2.links()[0]->isAnchored(LinkItem::EndB));
        CHECK(s2.links()[0]->width() == 3.0);

        // 双自由：端点坐标保留
        CHECK(near(s2.links()[1]->pointA(), QPointF(10, 10)));
        CHECK(near(s2.links()[1]->pointB(), QPointF(300, 300)));
        CHECK(s2.links()[1]->color() == QColor(0, 200, 0));

        // 混合：一端锚定、一端自由
        CHECK(s2.links()[2]->isAnchored(LinkItem::EndA));
        CHECK(!s2.links()[2]->isAnchored(LinkItem::EndB));
        CHECK(near(s2.links()[2]->pointB(), QPointF(400, 400)));

        // 加载后的链接仍跟随图片移动（目标点避开智能对齐阈值：SNAP=8px）
        imgAt200->setPos(600, 500);
        CHECK(near(s2.links()[0]->pointB(), QPointF(660, 530)));
        CHECK(near(s2.links()[2]->pointA(), QPointF(660, 530)));
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
        CHECK(near(scene.links().first()->pointA(), QPointF(7, 8)));
        CHECK(near(scene.links().first()->pointB(), QPointF(70, 80)));
    }

    // ---- 删除 / 全选 / 清空 与链接列表一致 ----
    {
        CanvasScene scene;
        ImageItem *i0 = scene.addImage(makePix(100, 80), QPointF(0, 0));
        ImageItem *i1 = scene.addImage(makePix(100, 80), QPointF(200, 0));
        LinkItem *link = scene.createLink(i0, i0->sceneBoundingRect().center(),
                                          i1, i1->sceneBoundingRect().center());
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
