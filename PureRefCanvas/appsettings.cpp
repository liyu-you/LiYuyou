#include "appsettings.h"

#include <QSettings>
#include <QColor>

void AppSettings::load(int &outLastPage)
{
    QSettings s("PureRefCanvas", "里与由");
    currentTheme    = s.value("theme", currentTheme).toString();
    fontSize        = s.value("fontSize", fontSize).toInt();
    closeBehavior   = s.value("closeBehavior", closeBehavior).toString();
    startupRestore  = s.value("startupRestore", startupRestore).toBool();
    autoSave        = s.value("autoSave", autoSave).toBool();
    autoSaveInterval = s.value("autoSaveInterval", autoSaveInterval).toInt();
    thumbnailSize   = s.value("thumbnailSize", thumbnailSize).toString();
    outLastPage     = s.value("lastPage", 0).toInt();
    uiScale         = s.value("uiScale", uiScale).toInt();
    language        = s.value("language", language).toString();
    hardwareAcceleration = s.value("hardwareAcceleration", hardwareAcceleration).toBool();
    singleInstance  = s.value("singleInstance", singleInstance).toBool();
    autostart       = s.value("autostart", autostart).toBool();

    acrylicEnabled  = s.value("acrylicEnabled", acrylicEnabled).toBool();
    acrylicOpacity  = s.value("acrylicOpacity", acrylicOpacity).toInt();
    menuBarTransparent = s.value("menuBarTransparent", menuBarTransparent).toBool();
    menuBarAutoHide  = s.value("menuBarAutoHide", menuBarAutoHide).toBool();
    navToolbarTransparent = s.value("navToolbarTransparent", navToolbarTransparent).toBool();
    libraryStatusBarTransparent = s.value("libraryStatusBarTransparent", libraryStatusBarTransparent).toBool();
    libraryGridTransparent = s.value("libraryGridTransparent", libraryGridTransparent).toBool();
    contentBrowserTransparent = s.value("contentBrowserTransparent", contentBrowserTransparent).toBool();
    musicPanelsTransparent = s.value("musicPanelsTransparent", musicPanelsTransparent).toBool();
    notePanelsTransparent = s.value("notePanelsTransparent", notePanelsTransparent).toBool();
    readerPanelsTransparent = s.value("readerPanelsTransparent", readerPanelsTransparent).toBool();

    materialDir = s.value("materialDir", materialDir).toString();
    booksDir    = s.value("booksDir", booksDir).toString();
    musicDir    = s.value("musicDir", musicDir).toString();

    noteBgColor    = s.value("noteBgColor", noteBgColor).value<QColor>();
    readerBgColor  = s.value("readerBgColor", readerBgColor).value<QColor>();
    readerTextColor = s.value("readerTextColor", readerTextColor).value<QColor>();

    bgImagePath       = s.value("bgImagePath", bgImagePath).toString();
    readerShelfBgPath = s.value("readerShelfBgPath", readerShelfBgPath).toString();
    readerBgPath      = s.value("readerBgPath", readerBgPath).toString();
    canvasBgPath      = s.value("canvasBgPath", canvasBgPath).toString();
    noteBgPath        = s.value("noteBgPath", noteBgPath).toString();
    libraryBgPath     = s.value("libraryBgPath", libraryBgPath).toString();
    musicBgPath       = s.value("musicBgPath", musicBgPath).toString();

    minimapOpacity = s.value("minimapOpacity", minimapOpacity).toInt();
    zoomBarOpacity = s.value("zoomBarOpacity", zoomBarOpacity).toInt();
    minimapVisible = s.value("minimapVisible", minimapVisible).toBool();
    minimapSize    = s.value("minimapSize", minimapSize).toString();
    minimapThrottle = s.value("minimapThrottle", minimapThrottle).toBool();

    noteDefaultFont     = s.value("noteDefaultFont", noteDefaultFont).toString();
    noteDefaultFontSize = s.value("noteDefaultFontSize", noteDefaultFontSize).toInt();

    scPlayPause = s.value("shortcut/playPause", scPlayPause).toString();
    scPrev      = s.value("shortcut/prev", scPrev).toString();
    scNext      = s.value("shortcut/next", scNext).toString();
    scVolUp     = s.value("shortcut/volUp", scVolUp).toString();
    scVolDown   = s.value("shortcut/volDown", scVolDown).toString();

    timerShutdownEnabled  = s.value("timerShutdown/enabled", timerShutdownEnabled).toBool();
    timerShutdownHours    = s.value("timerShutdown/hours", timerShutdownHours).toInt();
    timerShutdownMinutes  = s.value("timerShutdown/minutes", timerShutdownMinutes).toInt();
    shutdownPCEnabled     = s.value("timerShutdown/shutdownPC", shutdownPCEnabled).toBool();

    startupPage       = s.value("startupPage", startupPage).toString();
    defaultSort       = s.value("library/defaultSort", defaultSort).toString();
    showFileExt       = s.value("library/showFileExt", showFileExt).toBool();
    doubleClickBehavior = s.value("library/doubleClickBehavior", doubleClickBehavior).toString();
    readerTheme       = s.value("reader/theme", readerTheme).toString();
    readerFont        = s.value("reader/font", readerFont).toString();
    readerLineSpacing = s.value("reader/lineSpacing", readerLineSpacing).toDouble();
    readerFontSize    = s.value("reader/fontSize", readerFontSize).toInt();
    readerMargin      = s.value("reader/margin", readerMargin).toString();
    defaultVolume     = s.value("music/defaultVolume", defaultVolume).toInt();
    defaultPlayMode   = s.value("music/defaultPlayMode", defaultPlayMode).toString();
    showLyrics        = s.value("music/showLyrics", showLyrics).toBool();
}

void AppSettings::save(int lastPage) const
{
    QSettings s("PureRefCanvas", "里与由");
    s.setValue("theme", currentTheme);
    s.setValue("fontSize", fontSize);
    s.setValue("closeBehavior", closeBehavior);
    s.setValue("startupRestore", startupRestore);
    s.setValue("autoSave", autoSave);
    s.setValue("autoSaveInterval", autoSaveInterval);
    s.setValue("thumbnailSize", thumbnailSize);
    s.setValue("lastPage", lastPage);
    s.setValue("uiScale", uiScale);
    s.setValue("language", language);
    s.setValue("hardwareAcceleration", hardwareAcceleration);
    s.setValue("singleInstance", singleInstance);
    s.setValue("autostart", autostart);

    s.setValue("acrylicEnabled", acrylicEnabled);
    s.setValue("acrylicOpacity", acrylicOpacity);
    s.setValue("menuBarTransparent", menuBarTransparent);
    s.setValue("menuBarAutoHide", menuBarAutoHide);
    s.setValue("navToolbarTransparent", navToolbarTransparent);
    s.setValue("libraryStatusBarTransparent", libraryStatusBarTransparent);
    s.setValue("libraryGridTransparent", libraryGridTransparent);
    s.setValue("contentBrowserTransparent", contentBrowserTransparent);
    s.setValue("musicPanelsTransparent", musicPanelsTransparent);
    s.setValue("notePanelsTransparent", notePanelsTransparent);
    s.setValue("readerPanelsTransparent", readerPanelsTransparent);

    s.setValue("materialDir", materialDir);
    s.setValue("booksDir", booksDir);
    s.setValue("musicDir", musicDir);

    s.setValue("noteBgColor", noteBgColor);
    s.setValue("readerBgColor", readerBgColor);
    s.setValue("readerTextColor", readerTextColor);

    s.setValue("bgImagePath", bgImagePath);
    s.setValue("readerShelfBgPath", readerShelfBgPath);
    s.setValue("readerBgPath", readerBgPath);
    s.setValue("canvasBgPath", canvasBgPath);
    s.setValue("noteBgPath", noteBgPath);
    s.setValue("libraryBgPath", libraryBgPath);
    s.setValue("musicBgPath", musicBgPath);

    s.setValue("minimapOpacity", minimapOpacity);
    s.setValue("zoomBarOpacity", zoomBarOpacity);
    s.setValue("minimapVisible", minimapVisible);
    s.setValue("minimapSize", minimapSize);
    s.setValue("minimapThrottle", minimapThrottle);

    s.setValue("noteDefaultFont", noteDefaultFont);
    s.setValue("noteDefaultFontSize", noteDefaultFontSize);

    s.setValue("shortcut/playPause", scPlayPause);
    s.setValue("shortcut/prev", scPrev);
    s.setValue("shortcut/next", scNext);
    s.setValue("shortcut/volUp", scVolUp);
    s.setValue("shortcut/volDown", scVolDown);

    s.setValue("timerShutdown/enabled", timerShutdownEnabled);
    s.setValue("timerShutdown/hours", timerShutdownHours);
    s.setValue("timerShutdown/minutes", timerShutdownMinutes);
    s.setValue("timerShutdown/shutdownPC", shutdownPCEnabled);

    s.setValue("startupPage", startupPage);
    s.setValue("library/defaultSort", defaultSort);
    s.setValue("library/showFileExt", showFileExt);
    s.setValue("library/doubleClickBehavior", doubleClickBehavior);
    s.setValue("reader/theme", readerTheme);
    s.setValue("reader/font", readerFont);
    s.setValue("reader/lineSpacing", readerLineSpacing);
    s.setValue("reader/fontSize", readerFontSize);
    s.setValue("reader/margin", readerMargin);
    s.setValue("music/defaultVolume", defaultVolume);
    s.setValue("music/defaultPlayMode", defaultPlayMode);
    s.setValue("music/showLyrics", showLyrics);
}
