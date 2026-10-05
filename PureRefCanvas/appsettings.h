#ifndef APPSETTINGS_H
#define APPSETTINGS_H

// C++23 refactor: all persistent preferences previously living as 65 loose
// members on MainWindow are collected here. This struct is a plain POD with
// defaults; MainWindow owns one instance and calls load()/save() around
// startup/shutdown. The QSettings keys and defaults are byte-identical to the
// old inline loadSettings()/saveSettings(), so existing user profiles keep
// working untouched.
//
// Why this split: MainWindow was 2800+ lines and carried every preference as
// a direct member. Centralising them here means the storage format has one
// home, and MainWindow can later drop the apply* plumbing without touching
// persistence.

#include <QString>
#include <QColor>
#include <QtGlobal>

struct AppSettings {
    // ---- General / window ----
    QString currentTheme      = "dark";
    int     fontSize          = 13;
    QString closeBehavior     = "tray";   // "tray" | "quit"
    bool    startupRestore    = true;
    bool    autoSave          = true;
    int     autoSaveInterval  = 5;        // minutes
    QString thumbnailSize     = "medium";
    int     uiScale           = 100;
    QString language          = "zh";
    bool    hardwareAcceleration = true;
    bool    singleInstance    = false;
    bool    autostart          = false;

    // ---- Acrylic / transparency ----
    bool acrylicEnabled      = false;
    int  acrylicOpacity      = 73;
    bool menuBarTransparent  = true;
    bool menuBarAutoHide     = true;
    bool navToolbarTransparent = true;
    bool libraryStatusBarTransparent = true;
    bool libraryGridTransparent = true;
    bool contentBrowserTransparent = true;
    bool musicPanelsTransparent = true;
    bool notePanelsTransparent = true;
    bool readerPanelsTransparent = true;

    // ---- Per-module root dirs ----
    QString materialDir;
    QString booksDir;
    QString musicDir;

    // ---- Colors ----
    QColor noteBgColor    = QColor(255, 255, 255);
    QColor readerBgColor  = QColor(255, 250, 240);
    QColor readerTextColor = QColor(60, 50, 40);

    // ---- Background image paths ----
    QString bgImagePath;
    QString readerShelfBgPath;
    QString readerBgPath;
    QString canvasBgPath;
    QString noteBgPath;
    QString libraryBgPath;
    QString musicBgPath;

    // ---- Minimap / zoom bar ----
    int  minimapOpacity   = 70;
    int  zoomBarOpacity    = 85;
    bool minimapVisible    = true;
    QString minimapSize    = "medium";
    bool minimapThrottle   = true;

    // ---- Note editor defaults ----
    QString noteDefaultFont     = "Microsoft YaHei";
    int     noteDefaultFontSize = 14;

    // ---- Music shortcuts (global) ----
    QString scPlayPause = "Ctrl+P";
    QString scPrev      = "Ctrl+Left";
    QString scNext      = "Ctrl+Right";
    QString scVolUp     = "Ctrl+Up";
    QString scVolDown   = "Ctrl+Down";

    // ---- Timer shutdown ----
    bool    timerShutdownEnabled = false;
    int     timerShutdownHours   = 0;
    int     timerShutdownMinutes = 0;
    bool    shutdownPCEnabled    = false;

    // ---- Startup / library ----
    QString startupPage      = "last";   // last/canvas/note/library/reader/music
    QString defaultSort      = "name";    // name/date/size/type
    bool    showFileExt      = true;
    QString doubleClickBehavior = "preview"; // preview/external/canvas

    // ---- Reader ----
    QString readerTheme     = "light";    // light/dark/eye/parchment
    QString readerFont      = "系统默认";
    double  readerLineSpacing = 1.5;
    int     readerFontSize  = 16;
    QString readerMargin    = "medium";   // narrow/medium/wide

    // ---- Music defaults ----
    int     defaultVolume   = 70;
    QString defaultPlayMode = "sequential"; // sequential/loopOne/loopAll/shuffle
    bool    showLyrics      = false;

    // ---- Logging ----
    bool    loggingEnabled  = true;   // write logs to dataRoot/logs/

    // Persist lastPage separately: it is UI state (current tab index), not a
    // preference. Caller passes it in on save because MainWindow owns the stack.
    void load(int &outLastPage);
    void save(int lastPage) const;
};

#endif // APPSETTINGS_H
