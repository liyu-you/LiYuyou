#include "autosavecontroller.h"
#include "userauth.h"

#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QDate>

namespace {
// Atomic write: QSaveFile writes to a temp file and renames into place, so an
// interrupted or crashing write can no longer leave a half-written autosave.
bool writeAtomically(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
}

AutoSaveController::AutoSaveController(Hooks hooks, QObject *parent)
    : QObject(parent), m_hooks(std::move(hooks))
{
    mTimer = new QTimer(this);
    connect(mTimer, &QTimer::timeout, this, &AutoSaveController::doAutoSave);
}

void AutoSaveController::setIntervalMinutes(int minutes)
{
    m_intervalMinutes = minutes;
    if (m_enabled && mTimer->isActive()) start();  // re-apply
}

void AutoSaveController::setEnabled(bool on)
{
    m_enabled = on;
    if (!on) mTimer->stop();
    else     start();
}

void AutoSaveController::start()
{
    if (!m_enabled || m_intervalMinutes <= 0) {
        mTimer->stop();
        return;
    }
    mTimer->start(m_intervalMinutes * 60 * 1000);
}

void AutoSaveController::doAutoSave()
{
    // Canvas
    if (m_hooks.canvasDirty && m_hooks.canvasDirty()) {
        const QByteArray state = m_hooks.canvasState ? m_hooks.canvasState() : QByteArray();
        if (!state.isEmpty()) {
            const QString path = UserAuth::dataRoot() + "/autosave.prc";
            QDir().mkpath(QFileInfo(path).absolutePath());
            if (writeAtomically(path, state) && m_hooks.canvasSaved)
                m_hooks.canvasSaved();
        }
    }
    // Note
    if (m_hooks.noteModified && m_hooks.noteModified()) {
        const QByteArray html = m_hooks.noteHtml ? m_hooks.noteHtml() : QByteArray();
        if (!html.isEmpty()) {
            const QString path = UserAuth::dataRoot() + "/autosave_note.html";
            QDir().mkpath(QFileInfo(path).absolutePath());
            writeAtomically(path, html);
        }
    }
    if (m_hooks.showStatus) m_hooks.showStatus(QStringLiteral("已自动保存"));
}

void AutoSaveController::maybeGenerateWeeklyReport()
{
    QSettings settings("PureRefCanvas", "里与由");
    const QString lastReport = settings.value("lastWeeklyReport", "").toString();
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(-(today.dayOfWeek() - 1));
    const QString weekKey = monday.toString("yyyy-MM-dd");
    if (lastReport == weekKey) return;

    const int noteCount     = m_hooks.countNotes    ? m_hooks.countNotes()    : 0;
    const int songCount     = m_hooks.countSongs    ? m_hooks.countSongs()    : 0;
    const int materialCount = m_hooks.countMaterials ? m_hooks.countMaterials() : 0;
    const int bookCount     = m_hooks.countBooks    ? m_hooks.countBooks()    : 0;

    const QString html = QStringLiteral(
        "<h2>📊 每周概览 · %1</h2>"
        "<p style='color:#5f6368'>本周自动生成于 %2</p>"
        "<table style='border-collapse:collapse; width:100%%; margin:16px 0;'>"
        "<tr><td style='padding:10px; background:#f1f3f4; border-radius:8px;'>📁 素材库</td>"
        "<td style='padding:10px; font-weight:500;'>%3 张</td></tr>"
        "<tr><td style='padding:10px; background:#f1f3f4; border-radius:8px;'>📝 笔记</td>"
        "<td style='padding:10px; font-weight:500;'>%4 条</td></tr>"
        "<tr><td style='padding:10px; background:#f1f3f4; border-radius:8px;'>📚 书籍</td>"
        "<td style='padding:10px; font-weight:500;'>%5 本</td></tr>"
        "<tr><td style='padding:10px; background:#f1f3f4; border-radius:8px;'>🎵 音乐</td>"
        "<td style='padding:10px; font-weight:500;'>%6 首</td></tr>"
        "</table>"
        "<p style='color:#888; font-size:12px;'>保持节奏，继续积累 ✨</p>"
    ).arg(weekKey, today.toString("yyyy-MM-dd HH:mm"))
     .arg(materialCount).arg(noteCount).arg(bookCount).arg(songCount);

    const QString title = QStringLiteral("周报 %1").arg(weekKey);
    if (m_hooks.createNote)
        m_hooks.createNote(title, html, QStringLiteral("周报"));
    settings.setValue("lastWeeklyReport", weekKey);
}

void AutoSaveController::generateWeeklyReportIfDue()
{
    maybeGenerateWeeklyReport();
}
