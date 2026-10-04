#include "settingsdialog.h"
#include "updatechecker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QScrollArea>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QFileDialog>
#include <QProgressDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QSettings>
#include <QListWidget>
#include <QStackedWidget>
#include <QFrame>
#include "thumbcache.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("偏好设置");
    setMinimumWidth(760);
    setMinimumHeight(560);
    resize(780, 580);
    m_noteBg = QColor(255, 255, 255);
    m_readerBg = QColor(255, 250, 240);
    m_readerText = QColor(60, 50, 40);
    m_bgImagePath.clear();
    setupUI();
}

void SettingsDialog::setupUI()
{
    QSettings settings("PureRefCanvas", "里与由");
    // MainWindow persists the theme under "theme" — keep the key in sync so
    // the dialog actually opens in the user's chosen theme.
    const bool light = settings.value("theme", "dark").toString() == "light";

    // Theme-dependent inline colors (the big stylesheet below can't cover the
    // per-widget setStyleSheet calls made further down).
    const QString txtMain    = light ? "#1f2937" : "#c7d5e0";
    const QString txtSub     = light ? "#6b7280" : "#8f98a0";
    const QString accent     = light ? "#2FA66A" : "#66c0f4";
    const QString accentDim  = light ? "#bfe6d4" : "#2a475e";
    const QString comboBg    = light ? "#ffffff" : "#16202d";
    const QString comboBorder= light ? "#d1d5db" : "#2a475e";
    const QString btnBg      = light ? "#f3f4f6" : "#2a475e";
    const QString btnBorder  = light ? "#d1d5db" : "#2a475e";
    const QString btnText    = light ? "#1f2937" : "#c7d5e0";
    const QString barBg      = light ? "#ffffff" : "#16202d";
    const QString barBorder  = light ? "#e5e7eb" : "#2a475e";
    const QString sepColor   = light ? "rgba(0,0,0,0.08)" : "rgba(255,255,255,0.06)";

    if (light) {
        // Light theme stylesheet
        setStyleSheet(R"(
            QDialog { background: #f5f7fa; }
            QListWidget { background: #ffffff; border: none; outline: none; border-right: 1px solid #e5e7eb; }
            QListWidget::item { color: #4b5563; padding: 14px 20px 14px 24px; border: none; font-size: 13px; font-weight: 500; }
            QListWidget::item:selected { background: #eaf7f0; color: #2FA66A; border-left: 3px solid #2FA66A; padding-left: 21px; }
            QListWidget::item:hover:!selected { background: rgba(0,0,0,0.04); color: #1f2937; }
            QLabel { color: #1f2937; background: transparent; }
            QCheckBox { color: #1f2937; spacing: 8px; font-size: 13px; }
            QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid #d1d5db; background: #ffffff; }
            QCheckBox::indicator:checked { background: #2FA66A; border-color: #2FA66A; }
            QRadioButton { color: #1f2937; spacing: 8px; font-size: 13px; }
            QRadioButton::indicator { width: 16px; height: 16px; border-radius: 8px; border: 1px solid #d1d5db; background: #ffffff; }
            QRadioButton::indicator:checked { background: #2FA66A; border-color: #2FA66A; }
            QPushButton { background: #f3f4f6; color: #1f2937; border: 1px solid #d1d5db; border-radius: 5px; padding: 7px 16px; font-size: 13px; font-weight: 500; }
            QPushButton:hover { background: #e5e7eb; border-color: #2FA66A; color: #000; }
            QPushButton:pressed { background: #d1d5db; }
            QPushButton#okBtn {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                    stop:0 #3FCB84, stop:0.5 #2FA66A, stop:1 #27945D);
                border: 1px solid #2FA66A; border-radius: 6px; color: #fff;
                font-weight: 600; font-size: 13px; padding: 8px 24px;
            }
            QPushButton#cancelBtn {
                background: #fff; border: 1px solid #d1d5db; border-radius: 6px;
                color: #1f2937; font-weight: 500; font-size: 13px; padding: 8px 24px;
            }
            QComboBox { background: #fff; color: #1f2937; border: 1px solid #d1d5db; border-radius: 5px; padding: 6px 12px; font-size: 13px; min-width: 100px; }
            QComboBox:hover { border-color: #2FA66A; }
            QComboBox::drop-down { border: none; width: 24px; }
            QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #6b7280; width: 0; height: 0; margin-right: 8px; }
            QComboBox QAbstractItemView { background: #fff; color: #1f2937; border: 1px solid #d1d5db; selection-background-color: #eaf7f0; selection-color: #2FA66A; padding: 4px; }
            QLineEdit { background: #fff; color: #1f2937; border: 1px solid #d1d5db; border-radius: 5px; padding: 6px 10px; font-size: 13px; }
            QLineEdit:focus { border-color: #2FA66A; }
            QScrollArea { border: none; background: #f5f7fa; }
            QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; }
            QScrollBar::handle:vertical { background: #d1d5db; border-radius: 5px; min-height: 30px; }
            QScrollBar::handle:vertical:hover { background: #9ca3af; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
            QFrame#separator { background: #e5e7eb; max-height: 1px; min-height: 1px; border: none; margin: 0 20px; }
        )");
    } else {
    setStyleSheet(R"(
        QDialog { background: #1b2838; }
        QListWidget { background: #16202d; border: none; outline: none; border-right: 1px solid #2a475e; }
        QListWidget::item { color: #8f98a0; padding: 14px 20px 14px 24px; border: none; font-size: 13px; font-weight: 500; }
        QListWidget::item:selected { background: #1b2838; color: #66c0f4; border-left: 3px solid #66c0f4; padding-left: 21px; }
        QListWidget::item:hover:!selected { background: rgba(42,71,94,0.5); color: #c7d5e0; }
        QLabel { color: #c7d5e0; background: transparent; }
        QCheckBox { color: #c7d5e0; spacing: 8px; font-size: 13px; }
        QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px; border: 1px solid #2a475e; background: #16202d; }
        QCheckBox::indicator:checked { background: #66c0f4; border-color: #66c0f4; }
        QRadioButton { color: #c7d5e0; spacing: 8px; font-size: 13px; }
        QRadioButton::indicator { width: 16px; height: 16px; border-radius: 8px; border: 1px solid #2a475e; background: #16202d; }
        QRadioButton::indicator:checked { background: #66c0f4; border-color: #66c0f4; }
        QPushButton { background: #2a475e; color: #c7d5e0; border: 1px solid #3a576e; border-radius: 5px; padding: 7px 16px; font-size: 13px; font-weight: 500; }
        QPushButton:hover { background: #3a576e; border-color: #66c0f4; color: #fff; }
        QPushButton:pressed { background: #1a374e; }
        QPushButton#okBtn {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #7dd0ff, stop:0.5 #66c0f4, stop:1 #4a9fd4);
            border: 1px solid #66c0f4;
            border-radius: 6px;
            color: #ffffff;
            font-weight: 600;
            font-size: 13px;
            padding: 8px 24px;
        }
        QPushButton#okBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #8dd8ff, stop:0.5 #77cbf7, stop:1 #5aa8dc);
            border-color: #8dd8ff;
        }
        QPushButton#okBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #4a9fd4, stop:0.5 #3a8fc4, stop:1 #2a7fb4);
            border-color: #4a9fd4;
        }
        QPushButton#cancelBtn {
            background: rgba(42,71,94,0.4);
            border: 1px solid #2a475e;
            border-radius: 6px;
            color: #c7d5e0;
            font-weight: 500;
            font-size: 13px;
            padding: 8px 24px;
        }
        QPushButton#cancelBtn:hover {
            background: rgba(42,71,94,0.8);
            border-color: #66c0f4;
            color: #ffffff;
        }
        QPushButton#cancelBtn:pressed {
            background: #1a374e;
            border-color: #3a576e;
        }
        QComboBox { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 5px; padding: 6px 12px; font-size: 13px; min-width: 100px; }
        QComboBox:hover { border-color: #66c0f4; }
        QComboBox::drop-down { border: none; width: 24px; }
        QComboBox::down-arrow { image: none; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #8f98a0; width: 0; height: 0; margin-right: 8px; }
        QComboBox QAbstractItemView { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 5px; outline: none; selection-background-color: #2a475e; selection-color: #66c0f4; padding: 4px; }
        QLineEdit { background: #16202d; color: #c7d5e0; border: 1px solid #2a475e; border-radius: 5px; padding: 6px 10px; font-size: 13px; }
        QLineEdit:focus { border-color: #66c0f4; }
        QScrollArea { border: none; background: #1b2838; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 4px; }
        QScrollBar::handle:vertical { background: #2a475e; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #3a576e; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QFrame#separator { background: #2a475e; max-height: 1px; min-height: 1px; border: none; margin: 0 20px; }
    )");
    } // end dark theme

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QHBoxLayout *splitLayout = new QHBoxLayout;
    splitLayout->setContentsMargins(0, 0, 0, 0);
    splitLayout->setSpacing(0);

    // Left navigation
    QListWidget *navList = new QListWidget;
    navList->setFixedWidth(180);
    navList->setFont(QFont("Microsoft YaHei", 10));
    navList->addItem("  外观");
    navList->addItem("  画布");
    navList->addItem("  笔记");
    navList->addItem("  编辑器");
    navList->addItem("  通用");
    navList->addItem("  库");
    navList->addItem("  阅读");
    navList->addItem("  音乐");
    navList->addItem("  高级");
    navList->addItem("  快捷键");
    navList->setCurrentRow(0);
    splitLayout->addWidget(navList);

    QStackedWidget *contentStack = new QStackedWidget;

    // Helper: separator line
    auto makeSeparator = [&sepColor]() -> QFrame* {
        QFrame *line = new QFrame;
        line->setObjectName("separator");
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("color: " + sepColor + ";");
        line->setFixedHeight(1);
        return line;
    };

    // Helper: settings row (title + desc + control)
    auto makeRow = [&txtMain, &txtSub](const QString &title, const QString &desc, QWidget *control) -> QWidget* {
        QWidget *row = new QWidget;
        row->setAttribute(Qt::WA_TranslucentBackground);
        row->setStyleSheet("background: transparent;");
        row->setMinimumHeight(44);
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(24, 8, 24, 8);
        layout->setSpacing(16);
        QVBoxLayout *textLayout = new QVBoxLayout;
        textLayout->setSpacing(1);
        QLabel *titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("color: " + txtMain + "; font-size: 13px; font-weight: 500; background: transparent;");
        textLayout->addWidget(titleLabel);
        if (!desc.isEmpty()) {
            QLabel *descLabel = new QLabel(desc);
            descLabel->setStyleSheet("color: " + txtSub + "; font-size: 11px; background: transparent;");
            descLabel->setWordWrap(true);
            textLayout->addWidget(descLabel);
        }
        layout->addLayout(textLayout, 1);
        if (control)
            layout->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
        return row;
    };

    // Helper: section title (accent bar + icon + label)
    auto addSection = [&light, &txtMain, &accent, &accentDim](QVBoxLayout *layout, const QString &title, const QString &icon = QString()) {
        QWidget *header = new QWidget;
        header->setStyleSheet("background: transparent;");
        QHBoxLayout *hl = new QHBoxLayout(header);
        hl->setContentsMargins(24, 16, 24, 6);
        hl->setSpacing(8);
        QFrame *bar = new QFrame;
        bar->setFixedSize(3, 14);
        bar->setStyleSheet(QString(
            "background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2); border-radius: 2px;")
            .arg(light ? "#3FCB84" : "#66c0f4", accentDim));
        hl->addWidget(bar);
        if (!icon.isEmpty()) {
            QLabel *ic = new QLabel(icon);
            ic->setStyleSheet("font-size: 13px; background: transparent;");
            hl->addWidget(ic);
        }
        QLabel *label = new QLabel(title);
        label->setStyleSheet("color: " + txtMain + "; font-size: 12px; font-weight: 700; letter-spacing: 1.5px; background: transparent;");
        hl->addWidget(label);
        hl->addStretch();
        layout->addWidget(header);
    };

    // Helper: create scroll page
    auto makePage = [&light](QScrollArea *&scroll, QVBoxLayout *&layout) {
        scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        const QString bg = light ? "#f5f7fa" : "#1b2838";
        scroll->setStyleSheet("background: " + bg + ";");
        QWidget *page = new QWidget;
        page->setStyleSheet("background: " + bg + ";");
        layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 8, 0, 20);
        layout->setSpacing(0);
        scroll->setWidget(page);
    };

    // ========== Page 1: 外观 ==========
    QScrollArea *appearanceScroll; QVBoxLayout *appearanceLayout;
    makePage(appearanceScroll, appearanceLayout);

    addSection(appearanceLayout, "主题");

    QWidget *themeWidget = new QWidget;
    QHBoxLayout *themeLayout = new QHBoxLayout(themeWidget);
    themeLayout->setContentsMargins(0, 0, 0, 0); themeLayout->setSpacing(12);
    m_darkRadio = new QRadioButton("深色");
    m_lightRadio = new QRadioButton("浅色");
    m_darkRadio->setChecked(true);
    QButtonGroup *themeGroup = new QButtonGroup(this);
    themeGroup->addButton(m_darkRadio); themeGroup->addButton(m_lightRadio);
    themeLayout->addWidget(m_darkRadio); themeLayout->addWidget(m_lightRadio); themeLayout->addStretch();
    appearanceLayout->addWidget(makeRow("主题模式", "选择深色或浅色界面", themeWidget));
    appearanceLayout->addWidget(makeSeparator());

    m_fontSizeCombo = new QComboBox;
    m_fontSizeCombo->addItems({"10px", "11px", "12px", "13px", "14px", "15px", "16px", "18px"});
    m_fontSizeCombo->setCurrentText("13px");
    appearanceLayout->addWidget(makeRow("界面字体大小", "调整全局文字大小", m_fontSizeCombo));
    appearanceLayout->addWidget(makeSeparator());

    addSection(appearanceLayout, "整体背景");

    QWidget *bgImageWidget = new QWidget;
    QHBoxLayout *bgImageLayout = new QHBoxLayout(bgImageWidget);
    bgImageLayout->setContentsMargins(0, 0, 0, 0); bgImageLayout->setSpacing(6);
    m_bgImageEdit = new QLineEdit;
    m_bgImageEdit->setPlaceholderText("未设置（默认纯色背景）");
    m_bgImageEdit->setMinimumWidth(200);
    m_bgImageBtn = new QPushButton("选择图片");
    m_bgImageBtn->setFixedWidth(80);
    m_bgImageClearBtn = new QPushButton("清除");
    m_bgImageClearBtn->setFixedWidth(60);
    connect(m_bgImageBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
        if (!path.isEmpty()) { m_bgImagePath = path; m_bgImageEdit->setText(path); }
    });
    connect(m_bgImageClearBtn, &QPushButton::clicked, this, [this]() {
        m_bgImagePath.clear(); m_bgImageEdit->clear();
    });
    bgImageLayout->addWidget(m_bgImageEdit);
    bgImageLayout->addWidget(m_bgImageBtn);
    bgImageLayout->addWidget(m_bgImageClearBtn);
    appearanceLayout->addWidget(makeRow("背景图片", "自定义程序整体背景图片", bgImageWidget));
    appearanceLayout->addWidget(makeSeparator());

    addSection(appearanceLayout, "毛玻璃效果");

    m_acrylicCheck = new QCheckBox("启用毛玻璃（亚克力模糊）");
    m_acrylicCheck->setStyleSheet("QCheckBox { color: " + txtMain + "; font-size: 13px; spacing: 8px; }");
    appearanceLayout->addWidget(m_acrylicCheck);

    QWidget *acrylicOpacityWidget = new QWidget;
    QHBoxLayout *acrylicOpacityLayout = new QHBoxLayout(acrylicOpacityWidget);
    acrylicOpacityLayout->setContentsMargins(0, 0, 0, 0); acrylicOpacityLayout->setSpacing(8);
    m_acrylicOpacitySlider = new QSlider(Qt::Horizontal);
    m_acrylicOpacitySlider->setRange(30, 100);
    m_acrylicOpacitySlider->setValue(73);
    m_acrylicOpacitySlider->setMinimumWidth(180);
    m_acrylicOpacityLabel = new QLabel("73%");
    m_acrylicOpacityLabel->setFixedWidth(40);
    m_acrylicOpacityLabel->setStyleSheet("color: " + txtSub + "; font-size: 12px;");
    connect(m_acrylicOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_acrylicOpacityLabel->setText(QString("%1%").arg(val));
    });
    acrylicOpacityLayout->addWidget(m_acrylicOpacitySlider);
    acrylicOpacityLayout->addWidget(m_acrylicOpacityLabel);
    acrylicOpacityLayout->addStretch();
    appearanceLayout->addWidget(makeRow("毛玻璃透明度", "数值越大越不透明（30%-100%）", acrylicOpacityWidget));

    addSection(appearanceLayout, "面板透明");
    QString checkStyle = "QCheckBox { color: " + txtMain + "; font-size: 13px; spacing: 8px; }";
    m_menuBarTransparentCheck = new QCheckBox("菜单栏透明");
    m_menuBarTransparentCheck->setStyleSheet(checkStyle);
    m_menuBarTransparentCheck->setToolTip("文件/编辑/视图/帮助菜单栏透出背景图");
    appearanceLayout->addWidget(m_menuBarTransparentCheck);
    m_menuBarAutoHideCheck = new QCheckBox("菜单栏半隐藏（自动隐藏）");
    m_menuBarAutoHideCheck->setStyleSheet(checkStyle);
    m_menuBarAutoHideCheck->setToolTip("隐藏顶部文件/编辑/视图/帮助菜单栏，改用导航栏最左侧 ☰ 按钮打开菜单");
    appearanceLayout->addWidget(m_menuBarAutoHideCheck);
    m_navToolbarTransparentCheck = new QCheckBox("导航栏透明");
    m_navToolbarTransparentCheck->setStyleSheet(checkStyle);
    m_navToolbarTransparentCheck->setToolTip("画布/笔记/库/阅读/音乐导航栏透出背景图");
    appearanceLayout->addWidget(m_navToolbarTransparentCheck);
    m_libraryStatusBarTransparentCheck = new QCheckBox("库状态栏透明");
    m_libraryStatusBarTransparentCheck->setStyleSheet(checkStyle);
    m_libraryStatusBarTransparentCheck->setToolTip("库页面底部「N 项」计数栏透出素材缩略图");
    appearanceLayout->addWidget(m_libraryStatusBarTransparentCheck);
    m_libraryGridTransparentCheck = new QCheckBox("库素材网格透明");
    m_libraryGridTransparentCheck->setStyleSheet(checkStyle);
    m_libraryGridTransparentCheck->setToolTip("库页面右侧缩略图卡片透明，透出背景图");
    appearanceLayout->addWidget(m_libraryGridTransparentCheck);
    m_contentBrowserTransparentCheck = new QCheckBox("内容浏览器面板透明");
    m_contentBrowserTransparentCheck->setStyleSheet(checkStyle);
    m_contentBrowserTransparentCheck->setToolTip("左侧「内容浏览器」文件夹面板透出背景图");
    appearanceLayout->addWidget(m_contentBrowserTransparentCheck);
    m_musicPanelsTransparentCheck = new QCheckBox("音乐播放器面板透明");
    m_musicPanelsTransparentCheck->setStyleSheet(checkStyle);
    m_musicPanelsTransparentCheck->setToolTip("音乐页侧边栏、底部播放条、歌词面板、歌曲列表表头透出背景图");
    appearanceLayout->addWidget(m_musicPanelsTransparentCheck);
    m_notePanelsTransparentCheck = new QCheckBox("笔记页面板透明");
    m_notePanelsTransparentCheck->setStyleSheet(checkStyle);
    m_notePanelsTransparentCheck->setToolTip("笔记页侧边栏、编辑器顶栏、格式工具栏、状态栏透出背景图");
    appearanceLayout->addWidget(m_notePanelsTransparentCheck);
    m_readerPanelsTransparentCheck = new QCheckBox("阅读页面板透明");
    m_readerPanelsTransparentCheck->setStyleSheet(checkStyle);
    m_readerPanelsTransparentCheck->setToolTip("阅读页顶栏/底栏透出「阅读页背景」图（未设置背景图时保持主题色）");
    appearanceLayout->addWidget(m_readerPanelsTransparentCheck);

    appearanceLayout->addStretch();
    contentStack->addWidget(appearanceScroll);

    // ========== Page 2: 画布 ==========
    QScrollArea *canvasScroll; QVBoxLayout *canvasLayout;
    makePage(canvasScroll, canvasLayout);

    addSection(canvasLayout, "画布背景");

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_canvasBgImgEdit = new QLineEdit;
        m_canvasBgImgEdit->setPlaceholderText("未设置（默认背景）");
        m_canvasBgImgEdit->setMinimumWidth(200);
        m_canvasBgImgBtn = new QPushButton("选择图片");
        m_canvasBgImgBtn->setFixedWidth(80);
        m_canvasBgImgClearBtn = new QPushButton("清除");
        m_canvasBgImgClearBtn->setFixedWidth(60);
        connect(m_canvasBgImgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_canvasBgPath = p; m_canvasBgImgEdit->setText(p); }
        });
        connect(m_canvasBgImgClearBtn, &QPushButton::clicked, this, [this]() {
            m_canvasBgPath.clear(); m_canvasBgImgEdit->clear();
        });
        l->addWidget(m_canvasBgImgEdit);
        l->addWidget(m_canvasBgImgBtn);
        l->addWidget(m_canvasBgImgClearBtn);
        canvasLayout->addWidget(makeRow("画布背景图片", "无限画布工作区的背景图片", w));
    }
    canvasLayout->addWidget(makeSeparator());

    addSection(canvasLayout, "迷你地图");

    QWidget *minimapOpacityWidget = new QWidget;
    QHBoxLayout *minimapOpacityLayout = new QHBoxLayout(minimapOpacityWidget);
    minimapOpacityLayout->setContentsMargins(0, 0, 0, 0); minimapOpacityLayout->setSpacing(8);
    m_minimapOpacitySlider = new QSlider(Qt::Horizontal);
    m_minimapOpacitySlider->setRange(30, 200);
    m_minimapOpacitySlider->setValue(70);
    m_minimapOpacitySlider->setMinimumWidth(180);
    m_minimapOpacityLabel = new QLabel("70%");
    m_minimapOpacityLabel->setFixedWidth(40);
    m_minimapOpacityLabel->setStyleSheet("color: " + txtSub + "; font-size: 12px;");
    connect(m_minimapOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_minimapOpacityLabel->setText(QString("%1%").arg(val));
    });
    minimapOpacityLayout->addWidget(m_minimapOpacitySlider);
    minimapOpacityLayout->addWidget(m_minimapOpacityLabel);
    minimapOpacityLayout->addStretch();
    canvasLayout->addWidget(makeRow("迷你地图透明度", "数值越大越不透明，越小越能看到画布内容（30%-200%）", minimapOpacityWidget));

    QWidget *zoomBarOpacityWidget = new QWidget;
    QHBoxLayout *zoomBarOpacityLayout = new QHBoxLayout(zoomBarOpacityWidget);
    zoomBarOpacityLayout->setContentsMargins(0, 0, 0, 0); zoomBarOpacityLayout->setSpacing(8);
    m_zoomBarOpacitySlider = new QSlider(Qt::Horizontal);
    m_zoomBarOpacitySlider->setRange(20, 100);
    m_zoomBarOpacitySlider->setValue(85);
    m_zoomBarOpacitySlider->setMinimumWidth(180);
    m_zoomBarOpacityLabel = new QLabel("85%");
    m_zoomBarOpacityLabel->setFixedWidth(40);
    m_zoomBarOpacityLabel->setStyleSheet("color: " + txtSub + "; font-size: 12px;");
    connect(m_zoomBarOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_zoomBarOpacityLabel->setText(QString("%1%").arg(val));
    });
    zoomBarOpacityLayout->addWidget(m_zoomBarOpacitySlider);
    zoomBarOpacityLayout->addWidget(m_zoomBarOpacityLabel);
    zoomBarOpacityLayout->addStretch();
    canvasLayout->addWidget(makeRow("缩放条透明度", "右下角缩放控制条的背景透明度（20%-100%）", zoomBarOpacityWidget));

    m_minimapShowCheck = new QCheckBox("显示迷你地图");
    m_minimapShowCheck->setChecked(true);
    m_minimapShowCheck->setStyleSheet("QCheckBox { color: " + txtMain + "; font-size: 13px; spacing: 8px; }");
    canvasLayout->addWidget(makeRow("显示迷你地图", "在画布右下角显示导航小地图", m_minimapShowCheck));

    m_minimapSizeCombo = new QComboBox;
    m_minimapSizeCombo->addItem("小", "small");
    m_minimapSizeCombo->addItem("中", "medium");
    m_minimapSizeCombo->addItem("大", "large");
    m_minimapSizeCombo->setMinimumWidth(90);
    canvasLayout->addWidget(makeRow("迷你地图尺寸", "迷你地图的显示大小", m_minimapSizeCombo));

    m_minimapThrottleCheck = new QCheckBox("启用性能节流");
    m_minimapThrottleCheck->setChecked(true);
    m_minimapThrottleCheck->setStyleSheet("QCheckBox { color: " + txtMain + "; font-size: 13px; spacing: 8px; }");
    canvasLayout->addWidget(makeRow("迷你地图性能节流", "拖动素材时减少缩略图重绘频率，更流畅（关闭后实时更新但可能卡顿）", m_minimapThrottleCheck));

    canvasLayout->addStretch();
    contentStack->addWidget(canvasScroll);

    // ========== Page 3: 笔记 ==========
    QScrollArea *noteScroll; QVBoxLayout *noteLayout;
    makePage(noteScroll, noteLayout);

    addSection(noteLayout, "笔记编辑器");

    QWidget *editorFontWidget = new QWidget;
    QHBoxLayout *editorFontLayout = new QHBoxLayout(editorFontWidget);
    editorFontLayout->setContentsMargins(0, 0, 0, 0); editorFontLayout->setSpacing(8);
    m_editorFontCombo = new QComboBox;
    m_editorFontCombo->addItems({"Microsoft YaHei", "SimSun", "SimHei", "KaiTi", "FangSong", "Segoe UI", "Arial", "Times New Roman", "Consolas", "Courier New"});
    m_editorFontCombo->setMinimumWidth(160);
    m_editorFontSizeCombo = new QComboBox;
    m_editorFontSizeCombo->addItems({"10px", "11px", "12px", "13px", "14px", "15px", "16px", "18px", "20px", "24px"});
    m_editorFontSizeCombo->setCurrentText("14px");
    m_editorFontSizeCombo->setMinimumWidth(80);
    editorFontLayout->addWidget(m_editorFontCombo);
    editorFontLayout->addWidget(m_editorFontSizeCombo);
    editorFontLayout->addStretch();
    noteLayout->addWidget(makeRow("默认字体", "笔记编辑器的默认字体和字号", editorFontWidget));
    noteLayout->addWidget(makeSeparator());

    addSection(noteLayout, "笔记背景");

    m_noteBgBtn = new QPushButton;
    m_noteBgBtn->setFixedWidth(100); m_noteBgBtn->setCursor(Qt::PointingHandCursor);
    connect(m_noteBgBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_noteBg, this, "选择笔记背景色");
        if (c.isValid()) { m_noteBg = c; updateColorBtn(m_noteBgBtn, c); }
    });
    noteLayout->addWidget(makeRow("笔记背景色", "自定义笔记编辑器背景", m_noteBgBtn));
    noteLayout->addWidget(makeSeparator());

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_noteBgImgEdit = new QLineEdit;
        m_noteBgImgEdit->setPlaceholderText("未设置（默认背景）");
        m_noteBgImgEdit->setMinimumWidth(200);
        m_noteBgImgBtn = new QPushButton("选择图片");
        m_noteBgImgBtn->setFixedWidth(80);
        m_noteBgImgClearBtn = new QPushButton("清除");
        m_noteBgImgClearBtn->setFixedWidth(60);
        connect(m_noteBgImgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_noteBgPath = p; m_noteBgImgEdit->setText(p); }
        });
        connect(m_noteBgImgClearBtn, &QPushButton::clicked, this, [this]() {
            m_noteBgPath.clear(); m_noteBgImgEdit->clear();
        });
        l->addWidget(m_noteBgImgEdit);
        l->addWidget(m_noteBgImgBtn);
        l->addWidget(m_noteBgImgClearBtn);
        noteLayout->addWidget(makeRow("笔记背景图片", "笔记工作区的背景图片", w));
    }

    noteLayout->addStretch();
    contentStack->addWidget(noteScroll);

    // ========== Page 4: 编辑器 ==========
    QScrollArea *editorScroll; QVBoxLayout *editorLayout;
    makePage(editorScroll, editorLayout);

    addSection(editorLayout, "缓存");

    m_clearCacheBtn = new QPushButton("清空缩略图缓存");
    m_clearCacheBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { border-color: %4; }")
        .arg(btnBg, btnText, btnBorder, accent));
    connect(m_clearCacheBtn, &QPushButton::clicked, this, [this]() {
        ThumbCache::clear();
        m_cacheCleared = true;
        QMessageBox::information(this, "缓存已清空", "缩略图磁盘缓存已清空，下次浏览素材时会重新生成。");
    });
    editorLayout->addWidget(makeRow("缩略图缓存", "删除已生成的素材缩略图（更换素材目录或想强制重新生成时使用）", m_clearCacheBtn));

    editorLayout->addWidget(makeSeparator());

    QLabel *editorHint = new QLabel("提示：修改后对新输入的文字生效，已有文字可全选后应用格式。");
    editorHint->setStyleSheet("color: " + txtSub + "; font-size: 12px; padding: 8px 24px;");
    editorHint->setWordWrap(true);
    editorLayout->addWidget(editorHint);
    editorLayout->addStretch();
    contentStack->addWidget(editorScroll);

    // ========== Page 2: 通用 ==========
    QScrollArea *generalScroll; QVBoxLayout *generalLayout;
    makePage(generalScroll, generalLayout);

    addSection(generalLayout, "启动与关闭");

    QWidget *closeWidget = new QWidget;
    QHBoxLayout *closeLayout = new QHBoxLayout(closeWidget);
    closeLayout->setContentsMargins(0, 0, 0, 0); closeLayout->setSpacing(12);
    m_closeTrayRadio = new QRadioButton("最小化到托盘");
    m_closeQuitRadio = new QRadioButton("直接退出");
    m_closeTrayRadio->setChecked(true);
    QButtonGroup *closeGroup = new QButtonGroup(this);
    closeGroup->addButton(m_closeTrayRadio); closeGroup->addButton(m_closeQuitRadio);
    closeLayout->addWidget(m_closeTrayRadio); closeLayout->addWidget(m_closeQuitRadio); closeLayout->addStretch();
    generalLayout->addWidget(makeRow("关闭窗口时", "点击关闭按钮的行为", closeWidget));
    generalLayout->addWidget(makeSeparator());

    m_startupRestoreCheck = new QCheckBox;
    m_startupRestoreCheck->setChecked(true);
    generalLayout->addWidget(makeRow("启动恢复", "记住上次打开的页面，启动时自动切回", m_startupRestoreCheck));
    generalLayout->addWidget(makeSeparator());

    m_startupPageCombo = new QComboBox;
    m_startupPageCombo->addItems({"上次页面", "画布", "笔记", "库", "阅读", "音乐"});
    generalLayout->addWidget(makeRow("启动页面", "启动时默认打开的工作区", m_startupPageCombo));
    generalLayout->addWidget(makeSeparator());

    addSection(generalLayout, "自动保存");

    QWidget *autoSaveWidget = new QWidget;
    QHBoxLayout *autoSaveLayout = new QHBoxLayout(autoSaveWidget);
    autoSaveLayout->setContentsMargins(0, 0, 0, 0); autoSaveLayout->setSpacing(8);
    m_autoSaveCheck = new QCheckBox;
    m_autoSaveCheck->setChecked(true);
    m_autoSaveIntervalCombo = new QComboBox;
    m_autoSaveIntervalCombo->addItems({"1 分钟", "5 分钟", "10 分钟", "30 分钟"});
    m_autoSaveIntervalCombo->setCurrentIndex(1);
    autoSaveLayout->addWidget(m_autoSaveCheck); autoSaveLayout->addWidget(m_autoSaveIntervalCombo); autoSaveLayout->addStretch();
    generalLayout->addWidget(makeRow("自动保存", "画布和笔记自动保存间隔", autoSaveWidget));
    generalLayout->addStretch();
    contentStack->addWidget(generalScroll);

    // ========== Page 3: 库 ==========
    QScrollArea *materialScroll; QVBoxLayout *materialLayout;
    makePage(materialScroll, materialLayout);

    addSection(materialLayout, "显示");

    QWidget *thumbWidget = new QWidget;
    QHBoxLayout *thumbLayout = new QHBoxLayout(thumbWidget);
    thumbLayout->setContentsMargins(0, 0, 0, 0); thumbLayout->setSpacing(12);
    m_thumbSmallRadio = new QRadioButton("小");
    m_thumbMediumRadio = new QRadioButton("中");
    m_thumbLargeRadio = new QRadioButton("大");
    m_thumbMediumRadio->setChecked(true);
    QButtonGroup *thumbGroup = new QButtonGroup(this);
    thumbGroup->addButton(m_thumbSmallRadio); thumbGroup->addButton(m_thumbMediumRadio); thumbGroup->addButton(m_thumbLargeRadio);
    thumbLayout->addWidget(m_thumbSmallRadio); thumbLayout->addWidget(m_thumbMediumRadio); thumbLayout->addWidget(m_thumbLargeRadio); thumbLayout->addStretch();
    materialLayout->addWidget(makeRow("缩略图大小", "库图片缩略图尺寸", thumbWidget));
    materialLayout->addWidget(makeSeparator());

    m_defaultSortCombo = new QComboBox;
    m_defaultSortCombo->addItems({"名称", "日期", "大小", "类型"});
    materialLayout->addWidget(makeRow("默认排序", "素材列表默认排序方式", m_defaultSortCombo));
    materialLayout->addWidget(makeSeparator());

    m_showFileExtCheck = new QCheckBox;
    m_showFileExtCheck->setChecked(true);
    materialLayout->addWidget(makeRow("显示扩展名", "卡片上显示文件扩展名", m_showFileExtCheck));
    materialLayout->addWidget(makeSeparator());

    m_doubleClickCombo = new QComboBox;
    m_doubleClickCombo->addItems({"预览", "在外部打开", "添加到画布"});
    materialLayout->addWidget(makeRow("双击行为", "双击素材卡片的默认操作", m_doubleClickCombo));
    materialLayout->addWidget(makeSeparator());

    addSection(materialLayout, "库背景");

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_libraryBgImgEdit = new QLineEdit;
        m_libraryBgImgEdit->setPlaceholderText("未设置（默认背景）");
        m_libraryBgImgEdit->setMinimumWidth(200);
        m_libraryBgImgBtn = new QPushButton("选择图片");
        m_libraryBgImgBtn->setFixedWidth(80);
        m_libraryBgImgClearBtn = new QPushButton("清除");
        m_libraryBgImgClearBtn->setFixedWidth(60);
        connect(m_libraryBgImgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_libraryBgPath = p; m_libraryBgImgEdit->setText(p); }
        });
        connect(m_libraryBgImgClearBtn, &QPushButton::clicked, this, [this]() {
            m_libraryBgPath.clear(); m_libraryBgImgEdit->clear();
        });
        l->addWidget(m_libraryBgImgEdit);
        l->addWidget(m_libraryBgImgBtn);
        l->addWidget(m_libraryBgImgClearBtn);
        materialLayout->addWidget(makeRow("库背景图片", "素材库工作区背景", w));
    }

    materialLayout->addStretch();
    contentStack->addWidget(materialScroll);

    // ========== Page 4: 阅读 ==========
    QScrollArea *readerScroll; QVBoxLayout *readerLayout;
    makePage(readerScroll, readerLayout);

    addSection(readerLayout, "显示");

    m_readerThemeCombo = new QComboBox;
    m_readerThemeCombo->addItems({"浅色", "深色", "护眼", "羊皮纸"});
    readerLayout->addWidget(makeRow("默认主题", "阅读器默认显示主题", m_readerThemeCombo));
    readerLayout->addWidget(makeSeparator());

    m_readerFontCombo = new QComboBox;
    m_readerFontCombo->addItems({"系统默认", "宋体", "黑体", "楷体", "微软雅黑"});
    readerLayout->addWidget(makeRow("默认字体", "阅读器默认字体", m_readerFontCombo));
    readerLayout->addWidget(makeSeparator());

    m_readerFontSizeCombo = new QComboBox;
    for (int i = 12; i <= 24; i += 2) m_readerFontSizeCombo->addItem(QString::number(i) + " px");
    m_readerFontSizeCombo->setCurrentText("16 px");
    readerLayout->addWidget(makeRow("字体大小", "阅读器默认字号", m_readerFontSizeCombo));
    readerLayout->addWidget(makeSeparator());

    m_readerLineSpacingCombo = new QComboBox;
    m_readerLineSpacingCombo->addItems({"1.0", "1.2", "1.5", "1.8", "2.0"});
    m_readerLineSpacingCombo->setCurrentText("1.5");
    readerLayout->addWidget(makeRow("行间距", "文本行间距倍数", m_readerLineSpacingCombo));
    readerLayout->addWidget(makeSeparator());

    m_readerMarginCombo = new QComboBox;
    m_readerMarginCombo->addItems({"窄", "中", "宽"});
    m_readerMarginCombo->setCurrentText("中");
    readerLayout->addWidget(makeRow("页边距", "阅读区域左右边距", m_readerMarginCombo));
    readerLayout->addWidget(makeSeparator());

    addSection(readerLayout, "阅读背景");

    m_readerBgBtn = new QPushButton;
    m_readerBgBtn->setFixedWidth(100); m_readerBgBtn->setCursor(Qt::PointingHandCursor);
    connect(m_readerBgBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_readerBg, this, "选择阅读背景色");
        if (c.isValid()) { m_readerBg = c; updateColorBtn(m_readerBgBtn, c); }
    });
    readerLayout->addWidget(makeRow("阅读背景色", "自定义阅读器页面背景", m_readerBgBtn));
    readerLayout->addWidget(makeSeparator());

    m_readerTextBtn = new QPushButton;
    m_readerTextBtn->setFixedWidth(100); m_readerTextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_readerTextBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(m_readerText, this, "选择阅读文字色");
        if (c.isValid()) { m_readerText = c; updateColorBtn(m_readerTextBtn, c); }
    });
    readerLayout->addWidget(makeRow("阅读文字色", "自定义阅读器文字颜色", m_readerTextBtn));
    readerLayout->addWidget(makeSeparator());

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_readerShelfBgEdit = new QLineEdit;
        m_readerShelfBgEdit->setPlaceholderText("未设置（默认背景）");
        m_readerShelfBgEdit->setMinimumWidth(200);
        m_readerShelfBgBtn = new QPushButton("选择图片");
        m_readerShelfBgBtn->setFixedWidth(80);
        m_readerShelfBgClearBtn = new QPushButton("清除");
        m_readerShelfBgClearBtn->setFixedWidth(60);
        connect(m_readerShelfBgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_readerShelfBgPath = p; m_readerShelfBgEdit->setText(p); }
        });
        connect(m_readerShelfBgClearBtn, &QPushButton::clicked, this, [this]() {
            m_readerShelfBgPath.clear(); m_readerShelfBgEdit->clear();
        });
        l->addWidget(m_readerShelfBgEdit);
        l->addWidget(m_readerShelfBgBtn);
        l->addWidget(m_readerShelfBgClearBtn);
        readerLayout->addWidget(makeRow("书架背景图片", "阅读工作区书架背景", w));
    }
    readerLayout->addWidget(makeSeparator());

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_readerBgImgEdit = new QLineEdit;
        m_readerBgImgEdit->setPlaceholderText("未设置（默认背景）");
        m_readerBgImgEdit->setMinimumWidth(200);
        m_readerBgImgBtn = new QPushButton("选择图片");
        m_readerBgImgBtn->setFixedWidth(80);
        m_readerBgImgClearBtn = new QPushButton("清除");
        m_readerBgImgClearBtn->setFixedWidth(60);
        connect(m_readerBgImgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_readerBgPath = p; m_readerBgImgEdit->setText(p); }
        });
        connect(m_readerBgImgClearBtn, &QPushButton::clicked, this, [this]() {
            m_readerBgPath.clear(); m_readerBgImgEdit->clear();
        });
        l->addWidget(m_readerBgImgEdit);
        l->addWidget(m_readerBgImgBtn);
        l->addWidget(m_readerBgImgClearBtn);
        readerLayout->addWidget(makeRow("阅读页背景图片", "阅读正文页面背景（配面板透明开关使用）", w));
    }

    readerLayout->addStretch();
    contentStack->addWidget(readerScroll);

    // ========== Page 5: 音乐 ==========
    QScrollArea *musicScroll; QVBoxLayout *musicLayout;
    makePage(musicScroll, musicLayout);

    addSection(musicLayout, "播放");

    m_defaultVolumeCombo = new QComboBox;
    for (int i = 0; i <= 100; i += 10) m_defaultVolumeCombo->addItem(QString::number(i) + " %");
    m_defaultVolumeCombo->setCurrentText("70 %");
    musicLayout->addWidget(makeRow("默认音量", "启动时默认播放音量", m_defaultVolumeCombo));
    musicLayout->addWidget(makeSeparator());

    m_defaultPlayModeCombo = new QComboBox;
    m_defaultPlayModeCombo->addItems({"顺序播放", "单曲循环", "列表循环", "随机播放"});
    musicLayout->addWidget(makeRow("默认播放模式", "启动时默认播放模式", m_defaultPlayModeCombo));
    musicLayout->addWidget(makeSeparator());

    m_showLyricsCheck = new QCheckBox;
    m_showLyricsCheck->setChecked(false);
    musicLayout->addWidget(makeRow("显示歌词", "启动时默认显示歌词面板", m_showLyricsCheck));
    musicLayout->addWidget(makeSeparator());

    addSection(musicLayout, "音乐背景");

    {
        QWidget *w = new QWidget;
        QHBoxLayout *l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0); l->setSpacing(6);
        m_musicBgImgEdit = new QLineEdit;
        m_musicBgImgEdit->setPlaceholderText("未设置（默认背景）");
        m_musicBgImgEdit->setMinimumWidth(200);
        m_musicBgImgBtn = new QPushButton("选择图片");
        m_musicBgImgBtn->setFixedWidth(80);
        m_musicBgImgClearBtn = new QPushButton("清除");
        m_musicBgImgClearBtn->setFixedWidth(60);
        connect(m_musicBgImgBtn, &QPushButton::clicked, this, [this]() {
            QString p = QFileDialog::getOpenFileName(this, "选择背景图片", QString(), "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
            if (!p.isEmpty()) { m_musicBgPath = p; m_musicBgImgEdit->setText(p); }
        });
        connect(m_musicBgImgClearBtn, &QPushButton::clicked, this, [this]() {
            m_musicBgPath.clear(); m_musicBgImgEdit->clear();
        });
        l->addWidget(m_musicBgImgEdit);
        l->addWidget(m_musicBgImgBtn);
        l->addWidget(m_musicBgImgClearBtn);
        musicLayout->addWidget(makeRow("音乐背景图片", "音乐播放器工作区背景", w));
    }

    musicLayout->addStretch();
    contentStack->addWidget(musicScroll);

    // ========== Page 6: 高级 ==========
    QScrollArea *advancedScroll; QVBoxLayout *advancedLayout;
    makePage(advancedScroll, advancedLayout);

    addSection(advancedLayout, "界面（修改后需重启生效）");

    m_uiScaleCombo = new QComboBox;
    m_uiScaleCombo->addItems({"75%", "100%", "125%", "150%"});
    m_uiScaleCombo->setCurrentText("100%");
    advancedLayout->addWidget(makeRow("界面缩放", "全局 DPI 缩放比例", m_uiScaleCombo));
    advancedLayout->addWidget(makeSeparator());

    QWidget *langWidget = new QWidget;
    QHBoxLayout *langLayout = new QHBoxLayout(langWidget);
    langLayout->setContentsMargins(0, 0, 0, 0); langLayout->setSpacing(12);
    m_langZhRadio = new QRadioButton("简体中文");
    m_langEnRadio = new QRadioButton("English");
    m_langZhRadio->setChecked(true);
    QButtonGroup *langGroup = new QButtonGroup(this);
    langGroup->addButton(m_langZhRadio); langGroup->addButton(m_langEnRadio);
    langLayout->addWidget(m_langZhRadio); langLayout->addWidget(m_langEnRadio); langLayout->addStretch();
    advancedLayout->addWidget(makeRow("界面语言", "切换显示语言", langWidget));
    advancedLayout->addWidget(makeSeparator());

    m_hwAccelCheck = new QCheckBox;
    m_hwAccelCheck->setChecked(true);
    advancedLayout->addWidget(makeRow("硬件加速", "关闭可解决部分显卡兼容问题", m_hwAccelCheck));
    advancedLayout->addWidget(makeSeparator());

    m_singleInstanceCheck = new QCheckBox;
    m_singleInstanceCheck->setChecked(false);
    advancedLayout->addWidget(makeRow("单实例模式", "开启后只能运行一个程序窗口（重启生效）", m_singleInstanceCheck));
    advancedLayout->addWidget(makeSeparator());

    addSection(advancedLayout, "数据目录");

    auto makeDirRow = [this, &txtMain, &txtSub](const QString &label, const QString &desc, QLineEdit *&edit) -> QWidget* {
        QWidget *row = new QWidget;
        row->setMinimumHeight(52);
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(28, 10, 28, 10);
        layout->setSpacing(12);
        QVBoxLayout *textLayout = new QVBoxLayout;
        textLayout->setSpacing(2);
        QLabel *titleLabel = new QLabel(label);
        titleLabel->setStyleSheet("color: " + txtMain + "; font-size: 14px; font-weight: 500;");
        textLayout->addWidget(titleLabel);
        QLabel *descLabel = new QLabel(desc);
        descLabel->setStyleSheet("color: " + txtSub + "; font-size: 12px;");
        textLayout->addWidget(descLabel);
        layout->addLayout(textLayout, 1);
        QWidget *rightWidget = new QWidget;
        QHBoxLayout *rightLayout = new QHBoxLayout(rightWidget);
        rightLayout->setContentsMargins(0, 0, 0, 0); rightLayout->setSpacing(6);
        edit = new QLineEdit;
        edit->setPlaceholderText("默认（文档目录）");
        edit->setMinimumWidth(180);
        QPushButton *browseBtn = new QPushButton("浏览");
        browseBtn->setFixedWidth(60);
        connect(browseBtn, &QPushButton::clicked, this, [this, edit]() {
            QString dir = QFileDialog::getExistingDirectory(this, "选择目录", edit->text());
            if (!dir.isEmpty()) edit->setText(dir);
        });
        rightLayout->addWidget(edit); rightLayout->addWidget(browseBtn);
        layout->addWidget(rightWidget, 0, Qt::AlignVCenter | Qt::AlignRight);
        return row;
    };

    advancedLayout->addWidget(makeDirRow("库目录", "素材图片存储位置", m_materialDirEdit));
    advancedLayout->addWidget(makeSeparator());
    advancedLayout->addWidget(makeDirRow("书籍库目录", "电子书存储位置", m_booksDirEdit));
    advancedLayout->addWidget(makeSeparator());
    advancedLayout->addWidget(makeDirRow("音乐库目录", "音乐文件存储位置", m_musicDirEdit));
    advancedLayout->addWidget(makeSeparator());

    addSection(advancedLayout, "系统");

    // Timer shutdown checkbox
    m_timerShutdownCheck = new QCheckBox("开启定时关闭软件");
    m_timerShutdownCheck->setStyleSheet(QString(
        "QCheckBox { color: %1; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid %2; background: %3; }"
        "QCheckBox::indicator:checked { background: %4; border-color: %4; }")
        .arg(txtMain, comboBorder, comboBg, accent));
    advancedLayout->addWidget(makeRow("定时关闭", "到达设定时间后自动关闭软件", m_timerShutdownCheck));
    advancedLayout->addWidget(makeSeparator());

    // Timer duration: hours + minutes
    QWidget *timerWidget = new QWidget;
    QHBoxLayout *timerLayout = new QHBoxLayout(timerWidget);
    timerLayout->setContentsMargins(0, 0, 0, 0);
    timerLayout->setSpacing(8);
    QLabel *timerLabel = new QLabel("剩余关闭时间");
    timerLabel->setStyleSheet("color: " + txtMain + "; font-size: 14px;");
    timerLayout->addWidget(timerLabel);
    const QString timerComboSS = QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 5px; padding: 5px 10px; font-size: 13px; }"
        "QComboBox:hover { border-color: %4; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; selection-background-color: %3; }")
        .arg(comboBg, txtMain, comboBorder, accent);
    m_timerHoursCombo = new QComboBox;
    for (int i = 0; i <= 23; i++) m_timerHoursCombo->addItem(QString::number(i));
    m_timerHoursCombo->setFixedWidth(80);
    m_timerHoursCombo->setStyleSheet(timerComboSS);
    timerLayout->addWidget(m_timerHoursCombo);
    QLabel *hourLabel = new QLabel("小时");
    hourLabel->setStyleSheet("color: " + txtSub + "; font-size: 13px;");
    timerLayout->addWidget(hourLabel);
    m_timerMinutesCombo = new QComboBox;
    for (int i = 0; i <= 59; i++) m_timerMinutesCombo->addItem(QString::number(i));
    m_timerMinutesCombo->setFixedWidth(80);
    m_timerMinutesCombo->setStyleSheet(timerComboSS);
    timerLayout->addWidget(m_timerMinutesCombo);
    QLabel *minLabel = new QLabel("分钟");
    minLabel->setStyleSheet("color: " + txtSub + "; font-size: 13px;");
    timerLayout->addWidget(minLabel);
    timerLayout->addStretch();
    advancedLayout->addWidget(makeRow("关闭倒计时", "设置距离关闭的剩余时间", timerWidget));
    advancedLayout->addWidget(makeSeparator());

    // Shutdown PC checkbox
    m_shutdownPCCheck = new QCheckBox("关闭软件同时关机");
    m_shutdownPCCheck->setStyleSheet(QString(
        "QCheckBox { color: %1; font-size: 14px; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid %2; background: %3; }"
        "QCheckBox::indicator:checked { background: %4; border-color: %4; }")
        .arg(txtMain, comboBorder, comboBg, accent));
    advancedLayout->addWidget(makeRow("同时关机", "关闭软件时一并关闭电脑", m_shutdownPCCheck));
    advancedLayout->addWidget(makeSeparator());

    addSection(advancedLayout, "数据管理");

    QPushButton *resetBtn = new QPushButton("恢复默认设置");
    resetBtn->setFixedHeight(36);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet("QPushButton { background: rgba(180,60,60,0.3); color: #ff8a8a; border: 1px solid rgba(180,60,60,0.5); border-radius: 6px; font-size: 13px; } QPushButton:hover { background: rgba(180,60,60,0.5); color: #ffb0b0; }");
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "恢复默认设置", "确定要恢复所有设置为默认值吗？\n此操作不可撤销。") == QMessageBox::Yes) {
            QSettings settings("PureRefCanvas", "里与由");
            settings.clear();
            QMessageBox::information(this, "已恢复", "所有设置已恢复默认值，重启程序后生效。");
        }
    });
    advancedLayout->addWidget(makeRow("重置设置", "一键恢复所有设置为默认值", resetBtn));
    advancedLayout->addWidget(makeSeparator());

    addSection(advancedLayout, "在线更新");

    m_currentVersionLabel = new QLabel(QString("当前版本：v%1").arg(UpdateChecker::currentVersionString()));
    m_currentVersionLabel->setStyleSheet("color: " + accent + "; font-size: 13px; font-weight: 600; padding: 0 24px;");
    advancedLayout->addWidget(m_currentVersionLabel);

    m_updateUrlEdit = new QLineEdit;
    m_updateUrlEdit->setPlaceholderText("请输入更新源 JSON 文件的 URL 地址");
    m_updateUrlEdit->setMinimumWidth(280);
    advancedLayout->addWidget(makeRow("更新源地址", "版本信息 JSON 文件的 URL", m_updateUrlEdit));
    advancedLayout->addWidget(makeSeparator());

    m_autoUpdateCheck = new QCheckBox;
    m_autoUpdateCheck->setChecked(true);
    advancedLayout->addWidget(makeRow("自动检查更新", "启动时自动检查是否有新版本", m_autoUpdateCheck));
    advancedLayout->addWidget(makeSeparator());

    m_checkUpdateBtn = new QPushButton("立即检查更新");
    m_checkUpdateBtn->setFixedHeight(36);
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this]() {
        QString url = m_updateUrlEdit->text().trimmed();
        if (url.isEmpty()) {
            QMessageBox::warning(this, "无法检查", "请先填写更新源地址");
            return;
        }
        m_checkUpdateBtn->setEnabled(false);
        m_checkUpdateBtn->setText("检查中...");
        UpdateChecker *checker = new UpdateChecker(this);
        connect(checker, &UpdateChecker::updateAvailable, this, [this, checker](const UpdateChecker::UpdateInfo &info) {
            m_checkUpdateBtn->setEnabled(true);
            m_checkUpdateBtn->setText("立即检查更新");
            QString msg = QString("发现新版本 v%1\n\n更新内容：\n%2\n\n是否立即下载更新？").arg(info.version.toString(), info.releaseNotes);
            if (QMessageBox::question(this, "发现更新", msg) == QMessageBox::Yes) {
                QString savePath = QDir::tempPath() + "/里与由_update.exe";
                QProgressDialog *progress = new QProgressDialog("正在下载更新...", "取消", 0, 100, this);
                progress->setWindowModality(Qt::WindowModal);
                progress->setMinimumDuration(0);
                connect(checker, &UpdateChecker::downloadProgress, progress, [progress](qint64 received, qint64 total) {
                    if (total > 0) progress->setValue(qRound(received * 100.0 / total));
                });
                connect(checker, &UpdateChecker::updateDownloaded, this, [this, progress, savePath](const QString &path) {
                    progress->close();
                    if (QMessageBox::question(this, "下载完成", "更新已下载完成，是否立即安装并重启？") == QMessageBox::Yes) {
                        UpdateChecker::installAndRestart(path);
                    }
                });
                connect(checker, &UpdateChecker::downloadFailed, this, [this, progress](const QString &err) {
                    progress->close();
                    QMessageBox::warning(this, "下载失败", err);
                });
                connect(progress, &QProgressDialog::canceled, checker, &QObject::deleteLater);
                checker->downloadUpdate(info.downloadUrl, savePath);
            } else {
                checker->deleteLater();
            }
        });
        connect(checker, &UpdateChecker::noUpdateAvailable, this, [this, checker]() {
            m_checkUpdateBtn->setEnabled(true);
            m_checkUpdateBtn->setText("立即检查更新");
            QMessageBox::information(this, "已是最新", "当前已是最新版本");
            checker->deleteLater();
        });
        connect(checker, &UpdateChecker::checkFailed, this, [this, checker](const QString &err) {
            m_checkUpdateBtn->setEnabled(true);
            m_checkUpdateBtn->setText("立即检查更新");
            QMessageBox::warning(this, "检查失败", err);
            checker->deleteLater();
        });
        checker->checkForUpdates(url);
    });
    advancedLayout->addWidget(makeRow("手动检查", "立即连接更新源检查新版本", m_checkUpdateBtn));

    // Load saved update settings
    {
        QSettings us("PureRefCanvas", "里与由");
        if (m_updateUrlEdit) m_updateUrlEdit->setText(us.value("update/url", "").toString());
        if (m_autoUpdateCheck) m_autoUpdateCheck->setChecked(us.value("update/autoCheck", true).toBool());
    }

    advancedLayout->addStretch();
    contentStack->addWidget(advancedScroll);

    // ========== Page 6: 快捷键 ==========
    QScrollArea *shortcutScroll; QVBoxLayout *shortcutLayout;
    makePage(shortcutScroll, shortcutLayout);

    addSection(shortcutLayout, "音乐播放控制");

    // Helper to create a shortcut row
    auto makeShortcutRow = [&txtMain, &txtSub, &accent, &comboBg, &comboBorder](const QString &label, const QString &desc, QKeySequenceEdit *&edit, const QKeySequence &defaultSeq) -> QWidget* {
        QWidget *row = new QWidget;
        row->setMinimumHeight(52);
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(24, 10, 24, 10);
        layout->setSpacing(16);
        QVBoxLayout *textLayout = new QVBoxLayout;
        textLayout->setSpacing(3);
        QLabel *titleLabel = new QLabel(label);
        titleLabel->setStyleSheet("color: " + txtMain + "; font-size: 14px; font-weight: 500;");
        textLayout->addWidget(titleLabel);
        if (!desc.isEmpty()) {
            QLabel *descLabel = new QLabel(desc);
            descLabel->setStyleSheet("color: " + txtSub + "; font-size: 12px;");
            textLayout->addWidget(descLabel);
        }
        layout->addLayout(textLayout, 1);
        edit = new QKeySequenceEdit(defaultSeq);
        edit->setMaximumWidth(220);
        edit->setStyleSheet(QString(
            "QKeySequenceEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 5px; padding: 6px 12px; font-size: 13px; font-weight: 600; }"
            "QKeySequenceEdit:focus { border-color: %2; }"
            "QKeySequenceEdit QLineEdit { background: transparent; border: none; color: %2; }")
            .arg(comboBg, accent, comboBorder));
        layout->addWidget(edit, 0, Qt::AlignVCenter | Qt::AlignRight);
        return row;
    };

    shortcutLayout->addWidget(makeShortcutRow("播放 / 暂停", "播放或暂停当前歌曲", m_scPlayPause, QKeySequence("Ctrl+P")));
    shortcutLayout->addWidget(makeSeparator());
    shortcutLayout->addWidget(makeShortcutRow("上一首", "切换到上一首歌曲", m_scPrev, QKeySequence("Ctrl+Left")));
    shortcutLayout->addWidget(makeSeparator());
    shortcutLayout->addWidget(makeShortcutRow("下一首", "切换到下一首歌曲", m_scNext, QKeySequence("Ctrl+Right")));
    shortcutLayout->addWidget(makeSeparator());
    shortcutLayout->addWidget(makeShortcutRow("音量增加", "提高播放音量", m_scVolUp, QKeySequence("Ctrl+Up")));
    shortcutLayout->addWidget(makeSeparator());
    shortcutLayout->addWidget(makeShortcutRow("音量减少", "降低播放音量", m_scVolDown, QKeySequence("Ctrl+Down")));
    shortcutLayout->addWidget(makeSeparator());

    QLabel *shortcutHint = new QLabel("提示：点击快捷键输入框后按下想要的组合键即可设置。快捷键在应用内全局生效。");
    shortcutHint->setStyleSheet("color: " + txtSub + "; font-size: 12px; padding: 8px 24px;");
    shortcutHint->setWordWrap(true);
    shortcutLayout->addWidget(shortcutHint);
    shortcutLayout->addStretch();
    contentStack->addWidget(shortcutScroll);

    splitLayout->addWidget(contentStack, 1);
    mainLayout->addLayout(splitLayout, 1);

    connect(navList, &QListWidget::currentRowChanged, contentStack, &QStackedWidget::setCurrentIndex);

    // Bottom button bar
    QWidget *btnBar = new QWidget;
    btnBar->setFixedHeight(56);
    btnBar->setStyleSheet("background: " + barBg + "; border-top: 1px solid " + barBorder + ";");
    QHBoxLayout *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(20, 0, 20, 0);
    btnLayout->addStretch();
    QPushButton *cancelBtn = new QPushButton("取消");
    cancelBtn->setObjectName("cancelBtn");
    QPushButton *okBtn = new QPushButton("确定");
    okBtn->setObjectName("okBtn");
    okBtn->setDefault(true);
    okBtn->setMinimumWidth(100);
    okBtn->setMinimumHeight(38);
    cancelBtn->setMinimumWidth(100);
    cancelBtn->setMinimumHeight(38);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    mainLayout->addWidget(btnBar);
}

// ========== Getters ==========

QString SettingsDialog::updateUrl() const
{
    return m_updateUrlEdit ? m_updateUrlEdit->text().trimmed() : QString();
}

bool SettingsDialog::autoUpdate() const
{
    return m_autoUpdateCheck ? m_autoUpdateCheck->isChecked() : true;
}

QString SettingsDialog::theme() const
{
    return m_darkRadio->isChecked() ? "dark" : "light";
}

int SettingsDialog::fontSize() const
{
    QString t = m_fontSizeCombo->currentText();
    return t.remove("px").toInt();
}

QString SettingsDialog::closeBehavior() const
{
    return m_closeTrayRadio->isChecked() ? "tray" : "quit";
}

bool SettingsDialog::startupRestore() const
{
    return m_startupRestoreCheck->isChecked();
}

bool SettingsDialog::autoSave() const
{
    return m_autoSaveCheck->isChecked();
}

int SettingsDialog::autoSaveInterval() const
{
    QString t = m_autoSaveIntervalCombo->currentText();
    return t.remove(" 分钟").toInt();
}

QString SettingsDialog::thumbnailSize() const
{
    if (m_thumbSmallRadio->isChecked()) return "small";
    if (m_thumbLargeRadio->isChecked()) return "large";
    return "medium";
}

// ========== Setters ==========

void SettingsDialog::setTheme(const QString &theme)
{
    if (theme == "light")
        m_lightRadio->setChecked(true);
    else
        m_darkRadio->setChecked(true);
}

void SettingsDialog::setFontSize(int size)
{
    m_fontSizeCombo->setCurrentText(QString("%1px").arg(size));
}

void SettingsDialog::setCloseBehavior(const QString &behavior)
{
    if (behavior == "quit")
        m_closeQuitRadio->setChecked(true);
    else
        m_closeTrayRadio->setChecked(true);
}

void SettingsDialog::setStartupRestore(bool restore)
{
    m_startupRestoreCheck->setChecked(restore);
}

void SettingsDialog::setAutoSave(bool enabled)
{
    m_autoSaveCheck->setChecked(enabled);
}

void SettingsDialog::setAutoSaveInterval(int minutes)
{
    m_autoSaveIntervalCombo->setCurrentText(QString("%1 分钟").arg(minutes));
}

void SettingsDialog::setThumbnailSize(const QString &size)
{
    if (size == "small") m_thumbSmallRadio->setChecked(true);
    else if (size == "large") m_thumbLargeRadio->setChecked(true);
    else m_thumbMediumRadio->setChecked(true);
}

// ========== Advanced getters ==========

int SettingsDialog::uiScale() const
{
    QString t = m_uiScaleCombo->currentText();
    return t.remove("%").toInt();
}

QString SettingsDialog::language() const
{
    return m_langZhRadio->isChecked() ? "zh" : "en";
}

bool SettingsDialog::hardwareAcceleration() const
{
    return m_hwAccelCheck->isChecked();
}

QString SettingsDialog::materialDir() const
{
    return m_materialDirEdit->text().trimmed();
}

QString SettingsDialog::booksDir() const
{
    return m_booksDirEdit->text().trimmed();
}

QString SettingsDialog::musicDir() const
{
    return m_musicDirEdit->text().trimmed();
}

// ========== Advanced setters ==========

void SettingsDialog::setUiScale(int scale)
{
    m_uiScaleCombo->setCurrentText(QString("%1%").arg(scale));
}

void SettingsDialog::setLanguage(const QString &lang)
{
    if (lang == "en") m_langEnRadio->setChecked(true);
    else m_langZhRadio->setChecked(true);
}

void SettingsDialog::setHardwareAcceleration(bool enabled)
{
    m_hwAccelCheck->setChecked(enabled);
}

void SettingsDialog::setMaterialDir(const QString &dir)
{
    m_materialDirEdit->setText(dir);
}

void SettingsDialog::setBooksDir(const QString &dir)
{
    m_booksDirEdit->setText(dir);
}

void SettingsDialog::setMusicDir(const QString &dir)
{
    m_musicDirEdit->setText(dir);
}

bool SettingsDialog::singleInstance() const
{
    return m_singleInstanceCheck ? m_singleInstanceCheck->isChecked() : false;
}

void SettingsDialog::setSingleInstance(bool enabled)
{
    if (m_singleInstanceCheck) m_singleInstanceCheck->setChecked(enabled);
}

// ========== Music shortcut getters ==========

QString SettingsDialog::shortcutPlayPause() const
{
    return m_scPlayPause ? m_scPlayPause->keySequence().toString() : "Ctrl+P";
}

QString SettingsDialog::shortcutPrev() const
{
    return m_scPrev ? m_scPrev->keySequence().toString() : "Ctrl+Left";
}

QString SettingsDialog::shortcutNext() const
{
    return m_scNext ? m_scNext->keySequence().toString() : "Ctrl+Right";
}

QString SettingsDialog::shortcutVolumeUp() const
{
    return m_scVolUp ? m_scVolUp->keySequence().toString() : "Ctrl+Up";
}

QString SettingsDialog::shortcutVolumeDown() const
{
    return m_scVolDown ? m_scVolDown->keySequence().toString() : "Ctrl+Down";
}

void SettingsDialog::setShortcutPlayPause(const QString &key)
{
    if (m_scPlayPause) m_scPlayPause->setKeySequence(QKeySequence(key));
}

void SettingsDialog::setShortcutPrev(const QString &key)
{
    if (m_scPrev) m_scPrev->setKeySequence(QKeySequence(key));
}

void SettingsDialog::setShortcutNext(const QString &key)
{
    if (m_scNext) m_scNext->setKeySequence(QKeySequence(key));
}

void SettingsDialog::setShortcutVolumeUp(const QString &key)
{
    if (m_scVolUp) m_scVolUp->setKeySequence(QKeySequence(key));
}

void SettingsDialog::setShortcutVolumeDown(const QString &key)
{
    if (m_scVolDown) m_scVolDown->setKeySequence(QKeySequence(key));
}

// ========== Timer shutdown getters/setters ==========

bool SettingsDialog::timerShutdownEnabled() const
{
    return m_timerShutdownCheck ? m_timerShutdownCheck->isChecked() : false;
}

int SettingsDialog::timerShutdownHours() const
{
    return m_timerHoursCombo ? m_timerHoursCombo->currentText().toInt() : 0;
}

int SettingsDialog::timerShutdownMinutes() const
{
    return m_timerMinutesCombo ? m_timerMinutesCombo->currentText().toInt() : 0;
}

bool SettingsDialog::shutdownPCEnabled() const
{
    return m_shutdownPCCheck ? m_shutdownPCCheck->isChecked() : false;
}

void SettingsDialog::setTimerShutdownEnabled(bool enabled)
{
    if (m_timerShutdownCheck) m_timerShutdownCheck->setChecked(enabled);
}

void SettingsDialog::setTimerShutdownHours(int hours)
{
    if (m_timerHoursCombo) m_timerHoursCombo->setCurrentText(QString::number(qBound(0, hours, 23)));
}

void SettingsDialog::setTimerShutdownMinutes(int minutes)
{
    if (m_timerMinutesCombo) m_timerMinutesCombo->setCurrentText(QString::number(qBound(0, minutes, 59)));
}

void SettingsDialog::setShutdownPCEnabled(bool enabled)
{
    if (m_shutdownPCCheck) m_shutdownPCCheck->setChecked(enabled);
}

// ========== Color helper ==========

void SettingsDialog::updateColorBtn(QPushButton *btn, const QColor &c)
{
    btn->setStyleSheet(QString(
        "QPushButton { background-color: %1; border: 1px solid #555; border-radius: 4px; }"
        "QPushButton:hover { border: 1px solid #66c0f4; }"
    ).arg(c.name()));
    btn->setText(c.name());
}

// ========== Note & Reader color getters ==========

QColor SettingsDialog::noteBgColor() const { return m_noteBg; }
QColor SettingsDialog::readerBgColor() const { return m_readerBg; }
QColor SettingsDialog::readerTextColor() const { return m_readerText; }

// ========== Note & Reader color setters ==========

void SettingsDialog::setNoteBgColor(const QColor &c)
{
    m_noteBg = c;
    if (m_noteBgBtn) updateColorBtn(m_noteBgBtn, c);
}

void SettingsDialog::setReaderBgColor(const QColor &c)
{
    m_readerBg = c;
    if (m_readerBgBtn) updateColorBtn(m_readerBgBtn, c);
}

void SettingsDialog::setReaderTextColor(const QColor &c)
{
    m_readerText = c;
    if (m_readerTextBtn) updateColorBtn(m_readerTextBtn, c);
}

// ========== Background image ==========

QString SettingsDialog::bgImagePath() const { return m_bgImagePath; }

void SettingsDialog::setBgImagePath(const QString &path)
{
    m_bgImagePath = path;
    if (m_bgImageEdit) m_bgImageEdit->setText(path);
}

bool SettingsDialog::acrylicEnabled() const { return m_acrylicCheck ? m_acrylicCheck->isChecked() : false; }

void SettingsDialog::setAcrylicEnabled(bool enabled)
{
    if (m_acrylicCheck) m_acrylicCheck->setChecked(enabled);
}

int SettingsDialog::acrylicOpacity() const { return m_acrylicOpacitySlider ? m_acrylicOpacitySlider->value() : 73; }

void SettingsDialog::setAcrylicOpacity(int opacity)
{
    if (m_acrylicOpacitySlider) {
        m_acrylicOpacitySlider->setValue(qBound(30, opacity, 100));
        m_acrylicOpacityLabel->setText(QString("%1%").arg(qBound(30, opacity, 100)));
    }
}

bool SettingsDialog::menuBarTransparent() const { return m_menuBarTransparentCheck ? m_menuBarTransparentCheck->isChecked() : true; }
void SettingsDialog::setMenuBarTransparent(bool v) { if (m_menuBarTransparentCheck) m_menuBarTransparentCheck->setChecked(v); }
bool SettingsDialog::menuBarAutoHide() const { return m_menuBarAutoHideCheck ? m_menuBarAutoHideCheck->isChecked() : false; }
void SettingsDialog::setMenuBarAutoHide(bool v) { if (m_menuBarAutoHideCheck) m_menuBarAutoHideCheck->setChecked(v); }
bool SettingsDialog::navToolbarTransparent() const { return m_navToolbarTransparentCheck ? m_navToolbarTransparentCheck->isChecked() : true; }
void SettingsDialog::setNavToolbarTransparent(bool v) { if (m_navToolbarTransparentCheck) m_navToolbarTransparentCheck->setChecked(v); }
bool SettingsDialog::libraryStatusBarTransparent() const { return m_libraryStatusBarTransparentCheck ? m_libraryStatusBarTransparentCheck->isChecked() : true; }
void SettingsDialog::setLibraryStatusBarTransparent(bool v) { if (m_libraryStatusBarTransparentCheck) m_libraryStatusBarTransparentCheck->setChecked(v); }
bool SettingsDialog::libraryGridTransparent() const { return m_libraryGridTransparentCheck ? m_libraryGridTransparentCheck->isChecked() : true; }
void SettingsDialog::setLibraryGridTransparent(bool v) { if (m_libraryGridTransparentCheck) m_libraryGridTransparentCheck->setChecked(v); }
bool SettingsDialog::contentBrowserTransparent() const { return m_contentBrowserTransparentCheck ? m_contentBrowserTransparentCheck->isChecked() : true; }
void SettingsDialog::setContentBrowserTransparent(bool v) { if (m_contentBrowserTransparentCheck) m_contentBrowserTransparentCheck->setChecked(v); }
bool SettingsDialog::musicPanelsTransparent() const { return m_musicPanelsTransparentCheck ? m_musicPanelsTransparentCheck->isChecked() : true; }
void SettingsDialog::setMusicPanelsTransparent(bool v) { if (m_musicPanelsTransparentCheck) m_musicPanelsTransparentCheck->setChecked(v); }
bool SettingsDialog::notePanelsTransparent() const { return m_notePanelsTransparentCheck ? m_notePanelsTransparentCheck->isChecked() : true; }
void SettingsDialog::setNotePanelsTransparent(bool v) { if (m_notePanelsTransparentCheck) m_notePanelsTransparentCheck->setChecked(v); }
bool SettingsDialog::readerPanelsTransparent() const { return m_readerPanelsTransparentCheck ? m_readerPanelsTransparentCheck->isChecked() : true; }
void SettingsDialog::setReaderPanelsTransparent(bool v) { if (m_readerPanelsTransparentCheck) m_readerPanelsTransparentCheck->setChecked(v); }

QString SettingsDialog::readerShelfBgPath() const
{
    return m_readerShelfBgPath;
}

void SettingsDialog::setReaderShelfBgPath(const QString &path)
{
    m_readerShelfBgPath = path;
    if (m_readerShelfBgEdit) m_readerShelfBgEdit->setText(path);
}

QString SettingsDialog::readerBgPath() const { return m_readerBgPath; }
void SettingsDialog::setReaderBgPath(const QString &path)
{
    m_readerBgPath = path;
    if (m_readerBgImgEdit) m_readerBgImgEdit->setText(path);
}

QString SettingsDialog::canvasBgPath() const { return m_canvasBgPath; }
void SettingsDialog::setCanvasBgPath(const QString &path) {
    m_canvasBgPath = path; if (m_canvasBgImgEdit) m_canvasBgImgEdit->setText(path);
}
QString SettingsDialog::noteBgPath() const { return m_noteBgPath; }
void SettingsDialog::setNoteBgPath(const QString &path) {
    m_noteBgPath = path; if (m_noteBgImgEdit) m_noteBgImgEdit->setText(path);
}
QString SettingsDialog::libraryBgPath() const { return m_libraryBgPath; }
void SettingsDialog::setLibraryBgPath(const QString &path) {
    m_libraryBgPath = path; if (m_libraryBgImgEdit) m_libraryBgImgEdit->setText(path);
}
QString SettingsDialog::musicBgPath() const { return m_musicBgPath; }
void SettingsDialog::setMusicBgPath(const QString &path) {
    m_musicBgPath = path; if (m_musicBgImgEdit) m_musicBgImgEdit->setText(path);
}

int SettingsDialog::minimapOpacity() const { return m_minimapOpacitySlider ? m_minimapOpacitySlider->value() : 70; }
void SettingsDialog::setMinimapOpacity(int opacity)
{
    const int v = qBound(30, opacity, 200);
    if (m_minimapOpacitySlider) {
        m_minimapOpacitySlider->setValue(v);
        m_minimapOpacityLabel->setText(QString("%1%").arg(v));
    }
}

int SettingsDialog::zoomBarOpacity() const { return m_zoomBarOpacitySlider ? m_zoomBarOpacitySlider->value() : 85; }
void SettingsDialog::setZoomBarOpacity(int opacity)
{
    const int v = qBound(20, opacity, 100);
    if (m_zoomBarOpacitySlider) {
        m_zoomBarOpacitySlider->setValue(v);
        m_zoomBarOpacityLabel->setText(QString("%1%").arg(v));
    }
}

bool SettingsDialog::minimapEnabled() const { return m_minimapShowCheck ? m_minimapShowCheck->isChecked() : true; }
void SettingsDialog::setMinimapEnabled(bool enabled) { if (m_minimapShowCheck) m_minimapShowCheck->setChecked(enabled); }

QString SettingsDialog::minimapSize() const
{
    return m_minimapSizeCombo ? m_minimapSizeCombo->currentData().toString() : QString("medium");
}
void SettingsDialog::setMinimapSize(const QString &size)
{
    if (!m_minimapSizeCombo) return;
    const int idx = m_minimapSizeCombo->findData(size);
    m_minimapSizeCombo->setCurrentIndex(idx >= 0 ? idx : 1);
}

bool SettingsDialog::minimapThrottle() const { return m_minimapThrottleCheck ? m_minimapThrottleCheck->isChecked() : true; }
void SettingsDialog::setMinimapThrottle(bool enabled) { if (m_minimapThrottleCheck) m_minimapThrottleCheck->setChecked(enabled); }

// ========== Editor settings ==========

QString SettingsDialog::editorFont() const
{
    return m_editorFontCombo->currentText();
}

int SettingsDialog::editorFontSize() const
{
    QString t = m_editorFontSizeCombo->currentText();
    return t.remove("px").toInt();
}

void SettingsDialog::setEditorFont(const QString &font)
{
    int idx = m_editorFontCombo->findText(font);
    if (idx >= 0) m_editorFontCombo->setCurrentIndex(idx);
    else m_editorFontCombo->setCurrentText(font);
}

void SettingsDialog::setEditorFontSize(int size)
{
    m_editorFontSizeCombo->setCurrentText(QString("%1px").arg(size));
}


// ========== New settings getters/setters ==========

// General - startup page
QString SettingsDialog::startupPage() const {
    if (!m_startupPageCombo) return "last";
    QStringList keys = {"last","canvas","note","library","reader","music"};
    return keys.value(m_startupPageCombo->currentIndex(), "last");
}
void SettingsDialog::setStartupPage(const QString &page) {
    if (!m_startupPageCombo) return;
    QStringList keys = {"last","canvas","note","library","reader","music"};
    int idx = keys.indexOf(page);
    if (idx >= 0) m_startupPageCombo->setCurrentIndex(idx);
}

// Library
QString SettingsDialog::defaultSort() const {
    if (!m_defaultSortCombo) return "name";
    QStringList keys = {"name","date","size","type"};
    return keys.value(m_defaultSortCombo->currentIndex(), "name");
}
void SettingsDialog::setDefaultSort(const QString &sort) {
    if (!m_defaultSortCombo) return;
    QStringList keys = {"name","date","size","type"};
    int idx = keys.indexOf(sort);
    if (idx >= 0) m_defaultSortCombo->setCurrentIndex(idx);
}
bool SettingsDialog::showFileExtension() const {
    return m_showFileExtCheck ? m_showFileExtCheck->isChecked() : true;
}
void SettingsDialog::setShowFileExtension(bool show) {
    if (m_showFileExtCheck) m_showFileExtCheck->setChecked(show);
}
QString SettingsDialog::doubleClickBehavior() const {
    if (!m_doubleClickCombo) return "preview";
    QStringList keys = {"preview","external","canvas"};
    return keys.value(m_doubleClickCombo->currentIndex(), "preview");
}
void SettingsDialog::setDoubleClickBehavior(const QString &behavior) {
    if (!m_doubleClickCombo) return;
    QStringList keys = {"preview","external","canvas"};
    int idx = keys.indexOf(behavior);
    if (idx >= 0) m_doubleClickCombo->setCurrentIndex(idx);
}

// Reader
QString SettingsDialog::readerTheme() const {
    if (!m_readerThemeCombo) return "light";
    QStringList keys = {"light","dark","eye","parchment"};
    return keys.value(m_readerThemeCombo->currentIndex(), "light");
}
void SettingsDialog::setReaderTheme(const QString &theme) {
    if (!m_readerThemeCombo) return;
    QStringList keys = {"light","dark","eye","parchment"};
    int idx = keys.indexOf(theme);
    if (idx >= 0) m_readerThemeCombo->setCurrentIndex(idx);
}
QString SettingsDialog::readerFont() const {
    if (!m_readerFontCombo) return "系统默认";
    return m_readerFontCombo->currentText();
}
void SettingsDialog::setReaderFont(const QString &font) {
    if (m_readerFontCombo) m_readerFontCombo->setCurrentText(font);
}
qreal SettingsDialog::readerLineSpacing() const {
    if (!m_readerLineSpacingCombo) return 1.5;
    return m_readerLineSpacingCombo->currentText().toDouble();
}
void SettingsDialog::setReaderLineSpacing(qreal spacing) {
    if (m_readerLineSpacingCombo) m_readerLineSpacingCombo->setCurrentText(QString::number(spacing));
}
int SettingsDialog::readerFontSize() const {
    if (!m_readerFontSizeCombo) return 16;
    return m_readerFontSizeCombo->currentText().replace(" px","").toInt();
}
void SettingsDialog::setReaderFontSize(int size) {
    if (m_readerFontSizeCombo) m_readerFontSizeCombo->setCurrentText(QString::number(size) + " px");
}
QString SettingsDialog::readerMargin() const {
    if (!m_readerMarginCombo) return "medium";
    QStringList keys = {"narrow","medium","wide"};
    return keys.value(m_readerMarginCombo->currentIndex(), "medium");
}
void SettingsDialog::setReaderMargin(const QString &margin) {
    if (!m_readerMarginCombo) return;
    QStringList keys = {"narrow","medium","wide"};
    int idx = keys.indexOf(margin);
    if (idx >= 0) m_readerMarginCombo->setCurrentIndex(idx);
}

// Music
int SettingsDialog::defaultVolume() const {
    if (!m_defaultVolumeCombo) return 70;
    return m_defaultVolumeCombo->currentText().replace(" %","").toInt();
}
void SettingsDialog::setDefaultVolume(int vol) {
    if (m_defaultVolumeCombo) m_defaultVolumeCombo->setCurrentText(QString::number(vol) + " %");
}
QString SettingsDialog::defaultPlayMode() const {
    if (!m_defaultPlayModeCombo) return "sequential";
    QStringList keys = {"sequential","loopOne","loopAll","shuffle"};
    return keys.value(m_defaultPlayModeCombo->currentIndex(), "sequential");
}
void SettingsDialog::setDefaultPlayMode(const QString &mode) {
    if (!m_defaultPlayModeCombo) return;
    QStringList keys = {"sequential","loopOne","loopAll","shuffle"};
    int idx = keys.indexOf(mode);
    if (idx >= 0) m_defaultPlayModeCombo->setCurrentIndex(idx);
}
bool SettingsDialog::showLyrics() const {
    return m_showLyricsCheck ? m_showLyricsCheck->isChecked() : false;
}
void SettingsDialog::setShowLyrics(bool show) {
    if (m_showLyricsCheck) m_showLyricsCheck->setChecked(show);
}

