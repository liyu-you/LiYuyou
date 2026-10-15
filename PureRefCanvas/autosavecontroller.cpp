#include "autosavecontroller.h"
#include "userauth.h"

#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QDate>
#include <QDateTime>

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
            const QString path = m_hooks.canvasPath
                ? m_hooks.canvasPath()
                : (UserAuth::dataRoot() + "/autosave.prc");
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

int AutoSaveController::recoverPendingSaves()
{
    int recovered = 0;
    const QString root = UserAuth::dataRoot();

    // Canvas: autosave.prc is the only automatic backup (manual saves go to a
    // user-chosen file), so restoring it can only add back lost work.
    const QString canvasPath = root + "/autosave.prc";
    QFile canvasFile(canvasPath);
    if (canvasFile.exists() && canvasFile.open(QIODevice::ReadOnly)) {
        const QByteArray state = canvasFile.readAll();
        canvasFile.close();
        if (!state.isEmpty() && m_hooks.confirmRestore && m_hooks.restoreCanvas
            && m_hooks.confirmRestore(QStringLiteral("检测到上次未保存的画布内容，要恢复吗？"))) {
            if (m_hooks.restoreCanvas(state)) {
                ++recovered;
                if (m_hooks.showStatus)
                    m_hooks.showStatus(QStringLiteral("已恢复未保存的画布内容"));
            }
        }
        // Consumed either way: stale content must not resurface on next start.
        canvasFile.remove();
    }

    // Note: the autosave holds raw HTML with no note id, so it is restored as a
    // brand-new note — never merged into an existing one, nothing can be lost.
    const QString notePath = root + "/autosave_note.html";
    QFile noteFile(notePath);
    if (noteFile.exists() && noteFile.open(QIODevice::ReadOnly)) {
        const QByteArray html = noteFile.readAll();
        noteFile.close();
        if (!html.isEmpty() && m_hooks.confirmRestore && m_hooks.restoreNoteAsNew
            && m_hooks.confirmRestore(QStringLiteral("检测到上次未保存的笔记内容，要恢复为新笔记吗？"))) {
            m_hooks.restoreNoteAsNew(
                QStringLiteral("自动恢复 ") + QDateTime::currentDateTime().toString("MM-dd HH:mm"),
                QString::fromUtf8(html));
            ++recovered;
            if (m_hooks.showStatus)
                m_hooks.showStatus(QStringLiteral("已恢复未保存的笔记内容"));
        }
        noteFile.remove();
    }

    return recovered;
}
