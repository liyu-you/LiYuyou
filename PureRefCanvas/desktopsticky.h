#ifndef DESKTOPSTICKY_H
#define DESKTOPSTICKY_H

#include <QWidget>
#include <QJsonArray>
#include <QList>
#include <QHash>
#include <QString>
#include <functional>

class QLabel;
class QPushButton;
class QTextBrowser;
class QSizeGrip;
class NoteData;

// 桌面便签的持久化状态：笔记 id + 窗口几何（虚拟桌面坐标）+ 透明模式。
// 纯数据 + 纯函数（序列化/解析），便于无头测试（_note_test）。
struct StickyState {
    QString noteId;
    int x = 0;
    int y = 0;
    int w = 260;   // 默认尺寸与 DesktopSticky::resize 保持一致
    int h = 200;
    bool transparent = false;   // 透明模式：背景全透明，只显示文字
};

// 序列化为 JSON 数组：[{"id":…, "x":…, "y":…, "w":…, "h":…}, …]
QJsonArray serializeStickyStates(const QList<StickyState> &states);

// 解析 JSON 数组；跳过 id 为空或几何异常的条目；非数组/损坏返回空列表。
QList<StickyState> parseStickyStates(const QJsonDocument &doc);

// 桌面便签窗口：无边框、置顶（Tool）、可拖动、可调整大小、显示笔记标题与富文本正文。
// 生命周期：顶层窗口（无 parent），由 DesktopStickyManager 管理。
// 采用 Qt 惯用所有权：WA_DeleteOnClose + closed() 信号交还管理权，无裸 new/delete。
class DesktopSticky : public QWidget
{
    Q_OBJECT
public:
    explicit DesktopSticky(const NoteData &note, QWidget *parent = nullptr);

    QString noteId() const { return m_noteId; }
    void updateNote(const NoteData &note);   // 笔记被编辑后刷新标题与正文
    StickyState state() const;               // 当前几何 + 透明模式（供持久化）

    // 透明模式：背景全透明、只显示文字（标题/正文）；按钮缩小为半透明锚点。
    bool transparentMode() const { return m_transparentMode; }
    void setTransparentMode(bool on);

signals:
    void closed(const QString &noteId);      // 关闭时发出（× 按钮或 toggle 取消）

protected:
    void mousePressEvent(QMouseEvent *event) override;   // 标题栏/空白区按下开始拖动
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;        // 先发 closed 再放行

private:
    void toggleTransparentMode();            // 标题栏「透/回」按钮

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_transparentBtn = nullptr; // 「透」进入透明模式 / 「回」恢复
    QTextBrowser *m_body = nullptr;
    QSizeGrip *m_grip = nullptr;
    QString m_noteId;
    bool m_dragging = false;
    bool m_transparentMode = false;
    QPoint m_dragOffset;   // 按下时 全局坐标 - 窗口左上角
};

// 桌面便签管理器：持有全部便签窗口，负责置顶/取消、持久化与重启恢复。
// 线程契约：仅在 GUI 线程使用（Qt 控件均非线程安全）。
class DesktopStickyManager : public QObject
{
    Q_OBJECT
public:
    // 从外部获取全量笔记（MainWindow 注入 NoteWidget::allNotes 的包装）。
    using NotesGetter = std::function<QList<NoteData>()>;

    // statePath: 便签状态文件路径（如 Documents/PureRefNotes/desktop_stickies.json）。
    DesktopStickyManager(const QString &statePath, NotesGetter getter, QObject *parent = nullptr);
    ~DesktopStickyManager() override;

    void restoreAll();                      // 启动时按持久化状态恢复仍存在的笔记
    void toggle(const QString &noteId);     // 置顶 ⇄ 取消（note 不存在时忽略）
    bool isPinned(const QString &noteId) const;
    void syncNote(const QString &noteId);   // 笔记内容变更后刷新便签正文
    void noteRemoved(const QString &noteId); // 笔记被删除后关闭对应便签
    void saveState() const;                 // 持久化全部便签几何（退出前调用）
    int stickyCount() const { return int(m_windows.size()); }

    // 测试/调试用：返回某笔记的便签窗口（未置顶时返回 nullptr）。
    DesktopSticky *windowFor(const QString &noteId) const;

private:
    void attach(DesktopSticky *sticky, const QString &noteId); // 登记 + closed 信号接线
    void closeSticky(const QString &noteId);

    QString m_statePath;
    NotesGetter m_getter;
    // 键：noteId；值：顶层窗口。窗口在 close 后由 WA_DeleteOnClose 自行销毁，
    // 管理器只保留「当前打开的」指针，closed 信号里同步移除，无悬垂风险。
    QHash<QString, DesktopSticky *> m_windows;
};

#endif // DESKTOPSTICKY_H
