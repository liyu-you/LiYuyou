#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QColor>

class QRadioButton;
class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QKeySequenceEdit;
class QSlider;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Appearance
    QString theme() const;
    int fontSize() const;
    void setTheme(const QString &theme);
    void setFontSize(int size);

    // Note & Reader backgrounds
    QColor noteBgColor() const;
    QColor readerBgColor() const;
    QColor readerTextColor() const;
    void setNoteBgColor(const QColor &c);
    void setReaderBgColor(const QColor &c);
    void setReaderTextColor(const QColor &c);

    // Background image
    QString bgImagePath() const;
    void setBgImagePath(const QString &path);

    // Acrylic blur (毛玻璃)
    bool acrylicEnabled() const;
    void setAcrylicEnabled(bool enabled);
    int acrylicOpacity() const;
    void setAcrylicOpacity(int opacity);

    // Panel transparency (透出背景图)
    bool menuBarTransparent() const;
    void setMenuBarTransparent(bool v);
    bool menuBarAutoHide() const;
    void setMenuBarAutoHide(bool v);
    bool navToolbarTransparent() const;
    void setNavToolbarTransparent(bool v);
    bool libraryStatusBarTransparent() const;
    void setLibraryStatusBarTransparent(bool v);
    bool libraryGridTransparent() const;
    void setLibraryGridTransparent(bool v);
    bool contentBrowserTransparent() const;
    void setContentBrowserTransparent(bool v);
    bool musicPanelsTransparent() const;
    void setMusicPanelsTransparent(bool v);
    bool notePanelsTransparent() const;
    void setNotePanelsTransparent(bool v);
    bool readerPanelsTransparent() const;
    void setReaderPanelsTransparent(bool v);
    bool cacheCleared() const { return m_cacheCleared; }

    // Reader shelf background
    QString readerShelfBgPath() const;
    void setReaderShelfBgPath(const QString &path);
    QString readerBgPath() const;
    void setReaderBgPath(const QString &path);

    // Workspace backgrounds
    QString canvasBgPath() const;
    void setCanvasBgPath(const QString &path);
    QString noteBgPath() const;
    void setNoteBgPath(const QString &path);
    QString libraryBgPath() const;
    void setLibraryBgPath(const QString &path);
    QString musicBgPath() const;
    void setMusicBgPath(const QString &path);

    // Minimap opacity (30-200 percent)
    int minimapOpacity() const;
    void setMinimapOpacity(int opacity);
    // Zoom bar opacity (20-100 percent)
    int zoomBarOpacity() const;
    void setZoomBarOpacity(int opacity);
    // Minimap visibility & size
    bool minimapEnabled() const;
    void setMinimapEnabled(bool enabled);
    QString minimapSize() const; // "small","medium","large"
    void setMinimapSize(const QString &size);
    // Minimap performance throttling
    bool minimapThrottle() const;
    void setMinimapThrottle(bool enabled);

    // Editor settings
    QString editorFont() const;
    int editorFontSize() const;
    void setEditorFont(const QString &font);
    void setEditorFontSize(int size);

    // General
    QString closeBehavior() const; // "tray" or "quit"
    bool startupRestore() const;
    bool autoSave() const;
    int autoSaveInterval() const; // minutes
    QString startupPage() const; // "last","canvas","note","library","reader","music"
    void setCloseBehavior(const QString &behavior);
    void setStartupRestore(bool restore);
    void setAutoSave(bool enabled);
    void setAutoSaveInterval(int minutes);
    void setStartupPage(const QString &page);

    // Material library
    QString thumbnailSize() const; // "small", "medium", "large"
    QString defaultSort() const; // "name","date","size","type"
    bool showFileExtension() const;
    QString doubleClickBehavior() const; // "preview","external","canvas"
    void setThumbnailSize(const QString &size);
    void setDefaultSort(const QString &sort);
    void setShowFileExtension(bool show);
    void setDoubleClickBehavior(const QString &behavior);

    // Reader
    QString readerTheme() const; // "light","dark","eye","parchment"
    QString readerFont() const;
    qreal readerLineSpacing() const;
    int readerFontSize() const;
    QString readerMargin() const; // "narrow","medium","wide"
    void setReaderTheme(const QString &theme);
    void setReaderFont(const QString &font);
    void setReaderLineSpacing(qreal spacing);
    void setReaderFontSize(int size);
    void setReaderMargin(const QString &margin);

    // Music
    int defaultVolume() const;
    QString defaultPlayMode() const; // "sequential","loopOne","loopAll","shuffle"
    bool showLyrics() const;
    void setDefaultVolume(int vol);
    void setDefaultPlayMode(const QString &mode);
    void setShowLyrics(bool show);

    // Advanced (require restart)
    int uiScale() const; // 75, 100, 125, 150
    QString language() const; // "zh" or "en"
    bool hardwareAcceleration() const;
    QString materialDir() const;
    QString booksDir() const;
    QString musicDir() const;
    void setUiScale(int scale);
    void setLanguage(const QString &lang);
    void setHardwareAcceleration(bool enabled);
    void setMaterialDir(const QString &dir);
    void setBooksDir(const QString &dir);
    void setMusicDir(const QString &dir);
    bool singleInstance() const;
    void setSingleInstance(bool enabled);

    // Timer shutdown
    bool timerShutdownEnabled() const;
    int timerShutdownHours() const;
    int timerShutdownMinutes() const;
    bool shutdownPCEnabled() const;
    void setTimerShutdownEnabled(bool enabled);
    void setTimerShutdownHours(int hours);
    void setTimerShutdownMinutes(int minutes);
    void setShutdownPCEnabled(bool enabled);

    // Music shortcuts
    QString shortcutPlayPause() const;
    QString shortcutPrev() const;
    QString shortcutNext() const;
    QString shortcutVolumeUp() const;
    QString shortcutVolumeDown() const;
    void setShortcutPlayPause(const QString &key);
    void setShortcutPrev(const QString &key);
    void setShortcutNext(const QString &key);
    void setShortcutVolumeUp(const QString &key);
    void setShortcutVolumeDown(const QString &key);

    // Update checker
    QString updateUrl() const;
    bool autoUpdate() const;

private:
    void setupUI();
    void updateColorBtn(QPushButton *btn, const QColor &c);

    // Appearance
    QRadioButton *m_darkRadio;
    QRadioButton *m_lightRadio;
    QComboBox *m_fontSizeCombo;
    QLabel *m_previewLabel;
    QPushButton *m_noteBgBtn;
    QPushButton *m_readerBgBtn;
    QPushButton *m_readerTextBtn;
    QPushButton *m_bgImageBtn;
    QLineEdit *m_bgImageEdit;
    QPushButton *m_bgImageClearBtn;
    QCheckBox *m_acrylicCheck;
    QSlider *m_acrylicOpacitySlider;
    QLabel *m_acrylicOpacityLabel;
    QCheckBox *m_menuBarTransparentCheck;
    QCheckBox *m_menuBarAutoHideCheck;
    QCheckBox *m_navToolbarTransparentCheck;
    QCheckBox *m_libraryStatusBarTransparentCheck;
    QCheckBox *m_libraryGridTransparentCheck;
    QCheckBox *m_contentBrowserTransparentCheck;
    QCheckBox *m_musicPanelsTransparentCheck;
    QCheckBox *m_notePanelsTransparentCheck;
    QCheckBox *m_readerPanelsTransparentCheck;
    QPushButton *m_clearCacheBtn;
    bool m_cacheCleared = false;
    // Update checker
    QLineEdit *m_updateUrlEdit;
    QPushButton *m_checkUpdateBtn;
    QCheckBox *m_autoUpdateCheck;
    QLabel *m_currentVersionLabel;
    QPushButton *m_readerShelfBgBtn;
    QLineEdit *m_readerShelfBgEdit;
    QPushButton *m_readerShelfBgClearBtn;
    QPushButton *m_readerBgImgBtn;
    QLineEdit *m_readerBgImgEdit;
    QPushButton *m_readerBgImgClearBtn;
    QPushButton *m_canvasBgImgBtn;
    QLineEdit *m_canvasBgImgEdit;
    QPushButton *m_canvasBgImgClearBtn;
    QSlider *m_minimapOpacitySlider;
    QLabel *m_minimapOpacityLabel;
    QSlider *m_zoomBarOpacitySlider;
    QLabel *m_zoomBarOpacityLabel;
    QCheckBox *m_minimapShowCheck;
    QComboBox *m_minimapSizeCombo;
    QCheckBox *m_minimapThrottleCheck;
    QPushButton *m_noteBgImgBtn;
    QLineEdit *m_noteBgImgEdit;
    QPushButton *m_noteBgImgClearBtn;
    QPushButton *m_libraryBgImgBtn;
    QLineEdit *m_libraryBgImgEdit;
    QPushButton *m_libraryBgImgClearBtn;
    QPushButton *m_musicBgImgBtn;
    QLineEdit *m_musicBgImgEdit;
    QPushButton *m_musicBgImgClearBtn;
    QComboBox *m_editorFontCombo;
    QComboBox *m_editorFontSizeCombo;
    QColor m_noteBg;
    QColor m_readerBg;
    QColor m_readerText;
    QString m_bgImagePath;
    QString m_readerShelfBgPath;
    QString m_readerBgPath;
    QString m_canvasBgPath;
    QString m_noteBgPath;
    QString m_libraryBgPath;
    QString m_musicBgPath;

    // General
    QRadioButton *m_closeTrayRadio;
    QRadioButton *m_closeQuitRadio;
    QCheckBox *m_startupRestoreCheck;
    QCheckBox *m_autoSaveCheck;
    QComboBox *m_autoSaveIntervalCombo;
    QComboBox *m_startupPageCombo;

    // Material
    QRadioButton *m_thumbSmallRadio;
    QRadioButton *m_thumbMediumRadio;
    QRadioButton *m_thumbLargeRadio;
    QComboBox *m_defaultSortCombo;
    QCheckBox *m_showFileExtCheck;
    QComboBox *m_doubleClickCombo;

    // Reader
    QComboBox *m_readerThemeCombo;
    QComboBox *m_readerFontCombo;
    QComboBox *m_readerLineSpacingCombo;
    QComboBox *m_readerFontSizeCombo;
    QComboBox *m_readerMarginCombo;

    // Music
    QComboBox *m_defaultVolumeCombo;
    QComboBox *m_defaultPlayModeCombo;
    QCheckBox *m_showLyricsCheck;

    // Advanced
    QComboBox *m_uiScaleCombo;
    QRadioButton *m_langZhRadio;
    QRadioButton *m_langEnRadio;
    QCheckBox *m_hwAccelCheck;
    QLineEdit *m_materialDirEdit;
    QLineEdit *m_booksDirEdit;
    QLineEdit *m_musicDirEdit;
    QCheckBox *m_singleInstanceCheck;

    // Music shortcuts
    QKeySequenceEdit *m_scPlayPause;
    QKeySequenceEdit *m_scPrev;
    QKeySequenceEdit *m_scNext;
    QKeySequenceEdit *m_scVolUp;
    QKeySequenceEdit *m_scVolDown;

    // Timer shutdown
    QCheckBox *m_timerShutdownCheck;
    QComboBox *m_timerHoursCombo;
    QComboBox *m_timerMinutesCombo;
    QCheckBox *m_shutdownPCCheck;
};

#endif
