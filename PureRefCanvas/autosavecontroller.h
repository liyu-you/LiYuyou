#ifndef AUTOSAVECONTROLLER_H
#define AUTOSAVECONTROLLER_H

// C++23 refactor: Auto-save timer + weekly-report generator was previously
// three inline methods on MainWindow (setupAutoSaveTimer / doAutoSave /
// maybeGenerateWeeklyReport) that reached directly into m_canvasView,
// m_noteWidget, m_musicWidget, m_materialWidget, m_readerWidget and the
// status bar. Pulled out here.
//
// The controller owns:
//   * the QTimer and its interval/enable state,
//   * the on-disk autosave paths (under UserAuth::dataRoot()),
//   * the "lastWeeklyReport" QSettings key.
// It does NOT own the widgets: all widget access goes through the Hooks
// callbacks supplied by MainWindow, so this class can be unit-tested with
// fake hooks and knows nothing about QGraphicsScene / NoteWidget.

#include <QObject>
#include <QString>
#include <QByteArray>
#include <functional>

class QTimer;

class AutoSaveController : public QObject
{
    Q_OBJECT
public:
    // Function-pointer-style seams into MainWindow's child widgets. All
    // callbacks are optional in principle, but in practice MainWindow wires
    // all of them.
    struct Hooks {
        // Canvas autosave
        std::function<bool()>      canvasDirty;     // is there unsaved canvas work?
        std::function<QByteArray()> canvasState;    // serialized .prc bytes
        std::function<void()>     canvasSaved;    // clear the dirty flag
        // Optional: per-space canvas autosave path. Defaults to autosave.prc.
        std::function<QString()>  canvasPath;

        // Note autosave
        std::function<bool()>      noteModified;   // is the note doc dirty?
        std::function<QByteArray()> noteHtml;      // toHtml().toUtf8()

        // Crash recovery (recoverPendingSaves): apply a leftover autosave file.
        // confirmRestore asks the user (restore / discard); restoreCanvas applies
        // serialized .prc bytes and returns false if the state was rejected;
        // restoreNoteAsNew files the orphaned note HTML as a brand-new note so it
        // can never overwrite an existing one.
        std::function<bool(const QString &what)> confirmRestore;
        std::function<bool(const QByteArray &)>  restoreCanvas;
        std::function<void(const QString &title, const QString &html)> restoreNoteAsNew;

        // Weekly report: counts + a "create a new note" sink
        std::function<int()>       countNotes;
        std::function<int()>       countSongs;
        std::function<int()>       countMaterials;
        std::function<int()>       countBooks;
        std::function<void(const QString &title,
                           const QString &html,
                           const QString &tag)>    createNote;

        // UI feedback
        std::function<void(const QString &msg)>    showStatus;
    };

    explicit AutoSaveController(Hooks hooks, QObject *parent = nullptr);

    // Apply settings from AppSettings. Safe to call after the user changes
    // auto-save interval in the settings dialog.
    void setIntervalMinutes(int minutes);
    void setEnabled(bool on);
    void start();

    // Call once on startup. Emits the weekly-report note if we have not
    // already generated one for the current ISO week.
    void generateWeeklyReportIfDue();

    // Consume leftover autosave files from a previous session (autosave.prc,
    // autosave_note.html). The user is asked before anything is applied; a
    // declined file is discarded, a missing/absent hook leaves the file alone.
    // Returns how many items were restored. Call after the widgets are wired.
    int recoverPendingSaves();

private:
    void doAutoSave();
    void maybeGenerateWeeklyReport();

    Hooks  m_hooks;
    QTimer *mTimer = nullptr;
    int    m_intervalMinutes = 5;
    bool   m_enabled = false;
};

#endif // AUTOSAVECONTROLLER_H
