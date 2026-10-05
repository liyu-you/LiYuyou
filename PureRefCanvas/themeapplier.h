#ifndef THEMEAPPLIER_H
#define THEMEAPPLIER_H

// C++23 refactor: all "apply a preference to the UI" plumbing used to live
// as ten private methods on MainWindow (applyTheme / applyFontSize /
// applyThumbnailSize / applyNoteBgColor / applyReaderColors / applyBgImage /
// applyAcrylic / applyTransparencySettings / applyMenuBarMode /
// applyEditorSettings). Pulled out here.
//
// ThemeApplier is a plain class (NOT a QObject): it takes a reference to the
// live AppSettings and a bag of widget pointers, then exposes one method per
// visual knob. MainWindow still owns the widgets and the config; this class
// just knows HOW to paint them.

#include <QString>
#include <QColor>
#include <QColor>
#include "appsettings.h"

class QMainWindow;
class QStackedWidget;
class QToolBar;
class QWidget;
class CanvasView;
class MaterialWidget;
class NoteWidget;
class ReaderWidget;
class MusicPlayerWidget;class ContentBrowserPanel;

class ThemeApplier
{
public:
    // All the widgets this applier is allowed to touch. nullptr means "skip".
    struct Widgets {
        QMainWindow *window = nullptr;          // setStyleSheet / menuBar() / winId()
        QStackedWidget *stack = nullptr;        // page container for bg-image passthrough
        QToolBar *navToolBar = nullptr;
        QWidget *menuHamburger = nullptr;       // ☰ button shown when menubar auto-hides
        CanvasView *canvasView = nullptr;
        MaterialWidget *materialWidget = nullptr;
        NoteWidget *noteWidget = nullptr;
        ReaderWidget *readerWidget = nullptr;
        MusicPlayerWidget *musicWidget = nullptr;
        ContentBrowserPanel *contentBrowser = nullptr;
    };

    ThemeApplier(AppSettings &cfg, Widgets w);

    // One entry point for startup / settings-dialog bulk re-apply.
    void applyAll();

    // Individual knobs (called both at startup and live when the user tweaks
    // a single setting in the dialog).
    void applyTheme(const QString &theme);
    void applyFontSize(int size);
    void applyThumbnailSize(const QString &size);
    void applyNoteBgColor(const QColor &c);
    void applyReaderColors(const QColor &bg, const QColor &text);
    void applyBgImage(const QString &path);
    void applyAcrylic();
    void applyTransparencySettings();
    void applyMenuBarMode();
    void applyEditorSettings(const QString &font, int fontSize);

private:
    AppSettings &m_cfg;
    Widgets      m_w;
};

#endif // THEMEAPPLIER_H
