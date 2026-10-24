#include "desktopsticky.h"
#include "notewidget.h"   // NoteData 完整定义

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QSizeGrip>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

// ========== 纯函数：序列化 / 解析（无头可测） ==========

QJsonArray serializeStickyStates(const QList<StickyState> &states)
{
    QJsonArray arr;
    for (const StickyState &s : states) {
        QJsonObject o;
        o.insert("id", s.noteId);
        o.insert("x", s.x);
        o.insert("y", s.y);
        o.insert("w", s.w);
        o.insert("h", s.h);
        arr.append(o);
    }
    return arr;
}

QList<StickyState> parseStickyStates(const QJsonDocument &doc)
{
    QList<StickyState> out;
    if (!doc.isArray())
        return out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        const QString id = o.value("id").toString();
        if (id.isEmpty())
            continue;
        // 缺失时回落到默认尺寸；异常几何（越界/负值）视为脏数据跳过。
        const int w = o.value("w").toInt(StickyState{}.w);
        const int h = o.value("h").toInt(StickyState{}.h);
        if (w < 120 || w > 2000 || h < 80 || h > 2000)
            continue;
        StickyState s;
        s.noteId = id;
        s.x = o.value("x").toInt(0);
        s.y = o.value("y").toInt(0);
        s.w = w;
        s.h = h;
        out.append(s);
    }
    return out;
}

// ========== DesktopSticky：置顶便签窗口 ==========

DesktopSticky::DesktopSticky(const NoteData &note, QWidget *parent)
    : QWidget(parent), m_noteId(note.id)
{
    // 与 PureRef 参考画布同一套窗口语义：无边框 + 工具窗 + 置顶。
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);   // 圆角外透明
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(200, 140);
    resize(260, 200);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 10);
    root->setSpacing(6);

    // ---- 标题栏：整行可拖动（子控件不拦截鼠标即向父级传播） ----
    auto *titleBar = new QWidget(this);
    titleBar->setObjectName(QStringLiteral("stickyTitleBar"));
    auto *titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(6, 2, 2, 2);
    titleLay->setSpacing(6);

    m_titleLabel = new QLabel(titleBar);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents); // 不拦鼠标，按下仍可拖
    m_titleLabel->setStyleSheet("color: #4d3f1f; font-weight: 600; font-size: 14px;"
                                "background: transparent;");

    m_closeBtn = new QPushButton(QStringLiteral("×"), titleBar);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setFixedSize(22, 22);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setStyleSheet(R"(
        QPushButton { background: transparent; border: none; color: #b8a85a;
                      font-size: 17px; border-radius: 11px; }
        QPushButton:hover { background: #e57373; color: #ffffff; }
    )");
    connect(m_closeBtn, &QPushButton::clicked, this, &DesktopSticky::close);

    titleLay->addWidget(m_titleLabel, 1);
    titleLay->addWidget(m_closeBtn);

    // ---- 正文：只读富文本 ----
    m_body = new QTextBrowser(this);
    m_body->setFrameShape(QFrame::NoFrame);
    m_body->setOpenExternalLinks(true);
    m_body->setStyleSheet(R"(
        QTextBrowser { background: rgba(255,255,255,0.35); border: 1px solid rgba(0,0,0,0.05);
                       border-radius: 8px; padding: 6px; color: #4a3f22; font-size: 13px; }
    )");

    // ---- 右下角大小调整手柄 ----
    m_grip = new QSizeGrip(this);
    m_grip->setFixedSize(16, 16);

    root->addWidget(titleBar);
    root->addWidget(m_body, 1);
    root->addWidget(m_grip, 0, Qt::AlignRight);

    // 经典便利贴黄；圆角由 WA_TranslucentBackground + 背景色提供
    setStyleSheet(R"(
        DesktopSticky { background: rgba(255, 248, 190, 0.96); border-radius: 12px; }
    )");

    updateNote(note);
}

void DesktopSticky::updateNote(const NoteData &note)
{
    m_noteId = note.id;
    const QString title = note.title.trimmed().isEmpty()
                              ? QStringLiteral("无标题")
                              : note.title;
    m_titleLabel->setText(title);
    setWindowTitle(title);

    if (note.content.trimmed().isEmpty()) {
        m_body->setHtml(QStringLiteral("<div style='color:#9c8f5e;'>（空笔记）</div>"));
    } else {
        m_body->setHtml(note.content);
    }
}

StickyState DesktopSticky::state() const
{
    StickyState s;
    s.noteId = m_noteId;
    const QRect g = frameGeometry();
    s.x = g.x();
    s.y = g.y();
    s.w = g.width();
    s.h = g.height();
    return s;
}

void DesktopSticky::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DesktopSticky::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DesktopSticky::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void DesktopSticky::closeEvent(QCloseEvent *event)
{
    emit closed(m_noteId);   // 管理器据此移除记录并持久化几何
    QWidget::closeEvent(event);
}

// ========== DesktopStickyManager：便签生命周期与持久化 ==========

DesktopStickyManager::DesktopStickyManager(const QString &statePath, NotesGetter getter,
                                           QObject *parent)
    : QObject(parent), m_statePath(statePath), m_getter(std::move(getter))
{
}

DesktopStickyManager::~DesktopStickyManager()
{
    // 兜底：正常退出走 aboutToQuit → saveState()；若窗口在析构时仍存活也保存一次。
    saveState();
}

void DesktopStickyManager::attach(DesktopSticky *sticky, const QString &noteId)
{
    m_windows.insert(noteId, sticky);
    connect(sticky, &DesktopSticky::closed, this, [this](const QString &id) {
        m_windows.remove(id);   // 窗口随后由 WA_DeleteOnClose 销毁
        saveState();
    });
}

void DesktopStickyManager::toggle(const QString &noteId)
{
    if (m_windows.contains(noteId)) {
        closeSticky(noteId);
        return;
    }
    if (!m_getter)
        return;
    const QList<NoteData> notes = m_getter();
    const auto it = std::find_if(notes.cbegin(), notes.cend(),
                                 [&](const NoteData &n) { return n.id == noteId; });
    if (it == notes.cend())
        return;   // 笔记不存在（已被删除/未找到）

    DesktopSticky *sticky = new DesktopSticky(*it);
    attach(sticky, noteId);
    sticky->show();
    sticky->raise();
    sticky->activateWindow();
    saveState();
}

void DesktopStickyManager::closeSticky(const QString &noteId)
{
    DesktopSticky *w = m_windows.take(noteId);
    if (!w)
        return;
    w->close();      // → closeEvent → closed()（槽内 remove 幂等 + saveState）
    saveState();     // 兜底持久化，即使 closed 槽未触发
}

bool DesktopStickyManager::isPinned(const QString &noteId) const
{
    return m_windows.contains(noteId);
}

void DesktopStickyManager::syncNote(const QString &noteId)
{
    DesktopSticky *w = m_windows.value(noteId, nullptr);
    if (!w || !m_getter)
        return;
    const QList<NoteData> notes = m_getter();
    const auto it = std::find_if(notes.cbegin(), notes.cend(),
                                 [&](const NoteData &n) { return n.id == noteId; });
    if (it != notes.cend())
        w->updateNote(*it);
}

void DesktopStickyManager::noteRemoved(const QString &noteId)
{
    closeSticky(noteId);
}

void DesktopStickyManager::restoreAll()
{
    QFile f(m_statePath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;   // 状态文件损坏/为空：直接忽略（便签无数据价值，不弹窗不覆盖）
    const QList<StickyState> states = parseStickyStates(doc);
    if (states.isEmpty() || !m_getter)
        return;

    const QList<NoteData> notes = m_getter();
    QHash<QString, const NoteData *> byId;
    byId.reserve(notes.size());
    for (const NoteData &n : notes)
        byId.insert(n.id, &n);

    const QRect avail = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1920, 1080);

    for (const StickyState &st : states) {
        const auto it = byId.constFind(st.noteId);
        if (it == byId.constEnd())
            continue;   // 该笔记已不存在，不恢复
        DesktopSticky *sticky = new DesktopSticky(**it);
        sticky->setGeometry(st.x, st.y, st.w, st.h);
        // 显示器被拔掉后几何可能落在屏幕外：钳回主屏可用区域
        if (!avail.intersects(sticky->frameGeometry()))
            sticky->move(avail.center() - sticky->rect().center());
        attach(sticky, st.noteId);
        sticky->show();
    }
}

void DesktopStickyManager::saveState() const
{
    if (m_windows.isEmpty()) {
        QFile::remove(m_statePath);   // 无便签时不残留空状态文件
        return;
    }
    QList<StickyState> states;
    states.reserve(m_windows.size());
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it)
        states.append(it.value()->state());

    QDir().mkpath(QFileInfo(m_statePath).absolutePath());
    QFile f(m_statePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(serializeStickyStates(states)).toJson(QJsonDocument::Indented));
}

DesktopSticky *DesktopStickyManager::windowFor(const QString &noteId) const
{
    return m_windows.value(noteId, nullptr);
}
