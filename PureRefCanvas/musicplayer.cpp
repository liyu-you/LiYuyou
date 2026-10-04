#include "musicplayer.h"
#include "miniplayer.h"
#include "fullscreenplayer.h"
#include "songmetadata.h"
#include "musiclibrarydb.h"
#include "coverart.h"

#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <cmath>
#include <QPainterPath>
#include <QPixmap>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>

#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QSet>
#include <QtMath>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QFile>
#include <QFileSystemWatcher>
#include <QTextStream>
#include <QRegularExpression>
#include <QScrollBar>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QDesktopServices>
#include <QShortcut>
#include <algorithm>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QTextEdit>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDateTime>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QtMath>
#include <QRandomGenerator>
#include <QDebug>

MusicPlayerWidget::MusicPlayerWidget(const QString &libraryRoot, QWidget *parent)
    : QWidget(parent), m_libraryRoot(libraryRoot)
{
    QDir().mkpath(m_libraryRoot);

    // SQLite library index (ported from Mineradio local-library-store.js).
    m_db = new MusicLibraryDb(m_libraryRoot + "/music/library.db");
    m_db->open();

    setupUI();

    // Create media player after UI is set up
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.7);

    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPlayerWidget::onPositionChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MusicPlayerWidget::updateLyrics);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MusicPlayerWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayerWidget::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, &MusicPlayerWidget::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::errorOccurred, this, &MusicPlayerWidget::onErrorOccurred);

    // Debounce song list search so typing doesn't rebuild the whole table on
    // every keystroke (table only refreshes 200ms after the user pauses)
    m_searchDebounceTimer = new QTimer(this);
    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(200);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, [this]() { refreshSongTable(); });

    // Network manager for NetEase playlist import
    m_netManager = new QNetworkAccessManager(this);
    m_pendingReply = nullptr;
    m_fullScreenPlayer = nullptr;

    // Keyboard shortcuts
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this, [this]() {
        m_player->setPosition(qMax(0LL, m_player->position() - 5000));
    });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right), this, [this]() {
        m_player->setPosition(qMin(m_player->duration(), m_player->position() + 5000));
    });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up), this, [this]() {
        m_audioOutput->setVolume(qMin(1.0, m_audioOutput->volume() + 0.05));
    });
    new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down), this, [this]() {
        m_audioOutput->setVolume(qMax(0.0, m_audioOutput->volume() - 0.05));
    });

    // Defer library loading — staggered to avoid main window freeze on show
    QTimer::singleShot(350, this, &MusicPlayerWidget::loadLibrary);

    setupMusicWatcher();
}

MusicPlayerWidget::~MusicPlayerWidget()
{
    // Flush listen time for the currently-playing track on exit.
    if (m_db && !m_currentSongPath.isEmpty()) {
        const qint64 listenedMs = QDateTime::currentMSecsSinceEpoch() - m_songPlayStartedAt;
        if (listenedMs > 3000)
            m_db->bumpPlay(m_currentSongPath, listenedMs);
    }
    saveLibrary();
    if (m_db) { m_db->close(); delete m_db; m_db = nullptr; }
}

void MusicPlayerWidget::hideSidebar()
{
    if (m_sidebar) {
        m_sidebar->setMaximumWidth(0);
        m_sidebar->setMinimumWidth(0);
        m_sidebar->hide();
    }
    if (m_splitter) m_splitter->setHandleWidth(0);
}

void MusicPlayerWidget::setBackgroundImage(const QString &path)
{
    if (!m_mainArea) return;
    m_bgPath = path;
    bool hasBg = !path.isEmpty() && QFile::exists(path);

    if (hasBg) {
        // Main area: background image
        m_mainArea->setObjectName("musicMainArea");
        m_mainArea->setStyleSheet("QWidget#musicMainArea { border-image: url(" + path + ") 0 0 0 0 stretch stretch; }");

        // Song table: transparent so background shows through
        if (m_songTable) {
            const QString headerBg = m_panelsTransparent ? "transparent" : "rgba(25,26,29,0.7)";
            m_songTable->setStyleSheet(QString(R"(
                QTableWidget { background: transparent; border: none; font-size: 13px; color: #e8e8e8; alternate-background-color: rgba(255,255,255,0.06); outline: none; }
                QTableWidget::item { padding: 8px; border: none; }
                QTableWidget::item:selected { background: rgba(194,12,12,0.35); color: #ffffff; }
                QTableWidget::item:hover { background: rgba(255,255,255,0.08); }
                QHeaderView::section { background: %1; color: #d0d0d0; border: none; border-bottom: 1px solid rgba(255,255,255,0.12); padding: 8px; font-weight: normal; font-size: 12px; }
            )").arg(headerBg));
            m_songTable->viewport()->setAutoFillBackground(false);
        }

        // Sidebar: transparent (or semi-transparent when toggle off)
        if (m_sidebar) {
            m_sidebar->setStyleSheet(m_panelsTransparent
                ? "background: transparent;"
                : "background: rgba(25,26,29,0.8);");
        }

        // Player bar: transparent (or semi-transparent blur-like)
        if (m_playerBar) {
            m_playerBar->setStyleSheet(m_panelsTransparent
                ? "background: transparent; border-top: 1px solid rgba(255,255,255,0.1);"
                : "background: rgba(32,33,37,0.85); border-top: 1px solid rgba(255,255,255,0.1);");
        }

        // Lyrics panel: transparent
        if (m_lyricsPanel) {
            m_lyricsPanel->setStyleSheet(m_panelsTransparent
                ? "background: transparent; border-left: 1px solid rgba(255,255,255,0.1);"
                : "background: rgba(25,26,29,0.8); border-left: 1px solid rgba(255,255,255,0.1);");
        }

        // Empty label: brighter text for bg image readability
        if (m_emptyLabel) {
            m_emptyLabel->setStyleSheet("color: rgba(255,255,255,0.6); font-size: 14px;");
        }
    } else {
        // No background: restore solid dark theme
        m_mainArea->setObjectName("");
        m_mainArea->setStyleSheet("background: #1b1c1f;");

        if (m_songTable) {
            m_songTable->setStyleSheet(R"(
                QTableWidget { background: #232428; border: none; font-size: 13px; color: #d0d0d0; alternate-background-color: rgba(255,255,255,0.03); outline: none; }
                QTableWidget::item { padding: 8px; border: none; }
                QTableWidget::item:selected { background: #34353a; color: #ffffff; }
                QHeaderView::section { background: #1b1c1f; color: #8a8a8a; border: none; border-bottom: 1px solid #2c2d32; padding: 8px; font-weight: normal; font-size: 12px; }
            )");
            m_songTable->viewport()->setAutoFillBackground(true);
        }

        if (m_sidebar) {
            m_sidebar->setStyleSheet("background: #212225;");
        }

        if (m_playerBar) {
            m_playerBar->setStyleSheet("background: #2b2c2f; border-top: 1px solid #17181b;");
        }

        if (m_lyricsPanel) {
            m_lyricsPanel->setStyleSheet("background: #212225; border-left: 1px solid #17181b;");
        }

        if (m_emptyLabel) {
            m_emptyLabel->setStyleSheet("color: #6f6f72; font-size: 14px;");
        }
    }
}

void MusicPlayerWidget::setPanelsTransparent(bool transparent)
{
    m_panelsTransparent = transparent;
    // Re-apply styles so sidebar / player bar / lyrics / table header follow the toggle
    setBackgroundImage(m_bgPath);
}

void MusicPlayerWidget::setupUI()
{
    // Top bar
    setupTopBar();

    // Content area: sidebar + content stack
    m_splitter = new QSplitter(Qt::Horizontal);

    setupSidebar();
    setupSongTable();
    setupDiscoveryPage();
    setupMusicShelfPage();
    setupLyricsPanel();

    m_contentStack = new QStackedWidget;
    m_contentStack->addWidget(m_musicShelfPage); // index 0 — light card shelf (default)
    m_contentStack->addWidget(m_discoveryPage); // index 1
    m_contentStack->addWidget(m_mainArea);      // index 2

    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_contentStack);
    m_splitter->addWidget(m_lyricsPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_sidebar->setMinimumWidth(0);
    m_sidebar->setMaximumWidth(16777215);
    m_sidebar->hide();  // 新书架页自带浅色侧栏
    m_splitter->setSizes({0, 900, 0});
    m_lyricsPanel->hide();

    setupPlayerBar();

    // Play-queue panel floats above the player bar (positioned in resizeEvent)
    m_queuePanel = new QueuePanel(this);
    m_queuePanel->hide();
    connect(m_queuePanel, &QueuePanel::playRequested, this, [this](int idx) { playSongAt(idx); });
    connect(m_queuePanel, &QueuePanel::modeRequested, this, &MusicPlayerWidget::cyclePlayMode);
    connect(m_queuePanel, &QueuePanel::closed, this, [this]() {
        m_queuePanel->hide();
        if (m_queueBtn) m_queueBtn->setChecked(false);
    });
    connect(m_queuePanel, &QueuePanel::miniRequested, this, [this]() {
        m_queuePanel->hide();
        m_queueBtn->setChecked(false);
        toggleMiniPlayer();
    });
    connect(m_queuePanel, &QueuePanel::clearRequested, this, [this]() {
        // Only created playlists can be emptied; the library/favorites views are
        // derived from disk and have no queue to clear.
        bool isPlaylist = false;
        for (const auto &pl : m_playlists) if (pl.name == m_currentNav) isPlaylist = true;
        if (!isPlaylist) {
            QMessageBox::information(this, "提示", "当前视图的播放队列不可清空。");
            return;
        }
        for (auto &pl : m_playlists)
            if (pl.name == m_currentNav) pl.songPaths.clear();
        saveLibrary();
        refreshSongTable();
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    // Top search bar removed per request; bottom mini-player bar removed per request.
    mainLayout->addWidget(m_splitter, 1);
}

void MusicPlayerWidget::setupTopBar()
{
    m_topBar = new QWidget;
    m_topBar->setFixedHeight(48);
    m_topBar->setStyleSheet("background: #242529; border-bottom: 1px solid #17181b;");

    QHBoxLayout *layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(10);

    // Top search
    m_topSearch = new QLineEdit;
    m_topSearch->setPlaceholderText("🔍 搜索音乐、歌手、歌词");
    m_topSearch->setFixedWidth(320);
    m_topSearch->setStyleSheet(R"(
        QLineEdit { background: #1b1c1f; color: #dcdcdc; border: 1px solid #34353a; border-radius: 16px; padding: 6px 16px; font-size: 13px; }
        QLineEdit:focus { border: 1px solid #c20c0c; }
    )");
    connect(m_topSearch, &QLineEdit::returnPressed, this, [this]() {
        m_searchText = m_topSearch->text().trimmed();
        if (m_contentStack) m_contentStack->setCurrentIndex(2);
        refreshSongTable();
    });
    layout->addWidget(m_topSearch);
    layout->addStretch();

    // Right icons
    QPushButton *emailBtn = new QPushButton("✉");
    emailBtn->setFixedSize(32, 32);
    emailBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0a0; border: none; font-size: 16px; } QPushButton:hover { color: #c20c0c; }");
    QPushButton *skinBtn = new QPushButton("🎨");
    skinBtn->setFixedSize(32, 32);
    skinBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0a0; border: none; font-size: 16px; } QPushButton:hover { color: #c20c0c; }");
    QPushButton *settingsBtn = new QPushButton("⚙");
    settingsBtn->setFixedSize(32, 32);
    settingsBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0a0; border: none; font-size: 16px; } QPushButton:hover { color: #c20c0c; }");

    // Tech-info button (audio/video engineering challenges)
    QPushButton *infoBtn = new QPushButton("ℹ");
    infoBtn->setFixedSize(32, 32);
    infoBtn->setToolTip("音视频技术挑战");
    infoBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0a0; border: none; font-size: 16px; } QPushButton:hover { color: #c20c0c; }");
    connect(infoBtn, &QPushButton::clicked, this, &MusicPlayerWidget::showTechInfo);

    layout->addWidget(emailBtn);
    layout->addWidget(skinBtn);
    layout->addWidget(settingsBtn);
    layout->addWidget(infoBtn);
    layout->addSpacing(8);
}

void MusicPlayerWidget::setupDiscoveryPage()
{
    m_discoveryPage = new QWidget;
    m_discoveryPage->setStyleSheet("background: #1b1c1f;");
    QVBoxLayout *layout = new QVBoxLayout(m_discoveryPage);
    layout->setContentsMargins(30, 24, 30, 20);
    layout->setSpacing(16);

    // Banner area
    QWidget *banner = new QWidget;
    banner->setFixedHeight(160);
    banner->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3a3b40, stop:1 #26272b); border-radius: 8px;");
    QHBoxLayout *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(30, 0, 30, 0);
    QLabel *bannerTitle = new QLabel("发现好音乐");
    bannerTitle->setStyleSheet("color: #ffffff; font-size: 28px; font-weight: bold;");
    QLabel *bannerSub = new QLabel("每日推荐 · 私人FM · 精选歌单");
    bannerSub->setStyleSheet("color: rgba(255,255,255,0.7); font-size: 14px;");
    QVBoxLayout *bannerText = new QVBoxLayout;
    bannerText->addWidget(bannerTitle);
    bannerText->addWidget(bannerSub);
    bannerLayout->addLayout(bannerText);
    bannerLayout->addStretch();
    QLabel *bannerIcon = new QLabel("🎵");
    bannerIcon->setStyleSheet("font-size: 64px;");
    bannerLayout->addWidget(bannerIcon);
    layout->addWidget(banner);

    // Section title
    QHBoxLayout *sectionHeader = new QHBoxLayout;
    QLabel *sectionTitle = new QLabel("推荐歌单");
    sectionTitle->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold;");
    QPushButton *moreBtn = new QPushButton("更多 ›");
    moreBtn->setStyleSheet("QPushButton { background: transparent; color: #a0a0a0; border: none; font-size: 13px; } QPushButton:hover { color: #c20c0c; }");
    sectionHeader->addWidget(sectionTitle);
    sectionHeader->addStretch();
    sectionHeader->addWidget(moreBtn);
    layout->addLayout(sectionHeader);

    // Playlist card grid
    QWidget *gridWidget = new QWidget;
    m_playlistGrid = new QGridLayout(gridWidget);
    m_playlistGrid->setSpacing(20);
    m_playlistGrid->setContentsMargins(0, 0, 0, 0);

    // Sample playlist cards
    QStringList playlistNames = {
        "每日推荐", "私人FM", "华语流行", "欧美经典",
        "轻音乐", "电子舞曲", "摇滚殿堂", "睡前音乐"
    };
    QStringList playlistColors = {
        "#c0392b", "#2980b9", "#27ae60", "#8e44ad",
        "#f39c12", "#16a085", "#d35400", "#34495e"
    };
    QStringList playCounts = {"328万", "156万", "89万", "234万", "67万", "45万", "123万", "89万"};

    for (int i = 0; i < 8; ++i) {
        QPushButton *card = new QPushButton;
        card->setFixedWidth(160);
        card->setCursor(Qt::PointingHandCursor);
        card->setStyleSheet("QPushButton { background: transparent; border: none; text-align: left; } QPushButton:hover { background: transparent; }");
        QVBoxLayout *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(6);

        // Cover
        QWidget *cover = new QWidget;
        cover->setFixedSize(160, 160);
        cover->setStyleSheet(QString("background: %1; border-radius: 6px;").arg(playlistColors[i]));
        QVBoxLayout *coverLayout = new QVBoxLayout(cover);
        coverLayout->setContentsMargins(0, 0, 8, 8);
        QLabel *playCount = new QLabel("▶ " + playCounts[i]);
        playCount->setStyleSheet("color: rgba(255,255,255,0.9); font-size: 12px; background: rgba(0,0,0,0.3); border-radius: 8px; padding: 2px 8px;");
        playCount->setAlignment(Qt::AlignRight | Qt::AlignBottom);
        coverLayout->addStretch();
        coverLayout->addWidget(playCount, 0, Qt::AlignRight);

        QLabel *nameLabel = new QLabel(playlistNames[i]);
        nameLabel->setStyleSheet("color: #d0d0d0; font-size: 13px;");
        nameLabel->setWordWrap(true);
        nameLabel->setFixedHeight(40);

        cardLayout->addWidget(cover);
        cardLayout->addWidget(nameLabel);

        QString plName = playlistNames[i];
        connect(card, &QPushButton::clicked, this, [this, plName]() {
            // Switch to song list page and show this playlist
            if (m_contentStack) m_contentStack->setCurrentIndex(2);
            m_currentNav = plName;
            m_playlistTitle->setText(plName);
            // Create playlist if not exists
            bool exists = false;
            for (const auto &pl : m_playlists) if (pl.name == plName) { exists = true; break; }
            if (!exists) {
                PlaylistInfo pl;
                pl.name = plName;
                m_playlists << pl;
                saveLibrary();
            }
            m_currentIndex = -1;
            refreshSongTable();
        });

        int row = i / 4;
        int col = i % 4;
        m_playlistGrid->addWidget(card, row, col);
    }

    layout->addWidget(gridWidget);
    layout->addStretch();
}

void MusicPlayerWidget::setupSidebar()
{
    m_sidebar = new QWidget;
    m_sidebar->setFixedWidth(200);
    m_sidebar->setStyleSheet("background: #212225;");
    QVBoxLayout *layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 12, 0, 0);
    layout->setSpacing(0);

    m_navList = new QListWidget;
    m_navList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; outline: none; }
        QListWidget::item { color: #a0a0a0; padding: 9px 20px; font-size: 13px; border: none; }
        QListWidget::item:selected { background: #34353a; color: #ffffff; }
        QListWidget::item:hover:!selected { background: #2c2d32; color: #e0e0e0; }
    )");
    m_navList->setCursor(Qt::PointingHandCursor);
    connect(m_navList, &QListWidget::currentItemChanged, this, &MusicPlayerWidget::onNavChanged);

    // Section: 在线音乐
    QLabel *onlineLabel = new QLabel("  在线音乐");
    onlineLabel->setStyleSheet("color: #6f6f72; font-size: 11px; font-weight: 600; padding: 8px 20px 4px;");
    layout->addWidget(onlineLabel);
    m_navList->addItem("  发现音乐");
    m_navList->addItem("  私人FM");
    m_navList->addItem("  视频");
    m_navList->addItem("  朋友");

    // Section: 我的音乐
    QLabel *myLabel = new QLabel("  我的音乐");
    myLabel->setStyleSheet("color: #6f6f72; font-size: 11px; font-weight: 600; padding: 12px 20px 4px;");
    layout->addWidget(myLabel);
    m_navList->addItem("  本地音乐");
    m_navList->addItem("  最近播放");
    m_navList->addItem("  最常播放");
    m_navList->addItem("  下载管理");
    m_navList->addItem("  我的音乐云盘");
    m_navList->addItem("  我的收藏");

    layout->addWidget(m_navList, 1);

    // Playlist section
    QWidget *playlistHeader = new QWidget;
    QHBoxLayout *phLayout = new QHBoxLayout(playlistHeader);
    phLayout->setContentsMargins(20, 8, 12, 8);
    QLabel *plLabel = new QLabel("创建的歌单");
    plLabel->setStyleSheet("color: #6f6f72; font-size: 12px;");
    m_addPlaylistBtn = new QPushButton("+");
    m_addPlaylistBtn->setFixedSize(20, 20);
    m_addPlaylistBtn->setStyleSheet("QPushButton { background: transparent; color: #6f6f72; border: none; font-size: 16px; } QPushButton:hover { color: #c20c0c; }");
    connect(m_addPlaylistBtn, &QPushButton::clicked, this, &MusicPlayerWidget::addPlaylist);
    phLayout->addWidget(plLabel);
    phLayout->addStretch();
    phLayout->addWidget(m_addPlaylistBtn);
    layout->addWidget(playlistHeader);

    // Default: select 发现音乐
    m_currentNav = "discovery";
    m_navList->blockSignals(true);
    m_navList->setCurrentRow(0);
    m_navList->blockSignals(false);

    // Bottom "now playing" card (NetEase sidebar style)
    setupNowPlayingCard();
}

// Sidebar bottom card: cover + title/artist + like, matching the classic
// NetEase client. The cover opens the full-screen playing page (event filter,
// same as before); an expand button does the same.
void MusicPlayerWidget::setupNowPlayingCard()
{
    m_nowCard = new QWidget;
    m_nowCard->setFixedHeight(74);
    m_nowCard->setStyleSheet("QWidget { background: #212225; border-top: 1px solid #17181b; }");

    QHBoxLayout *lay = new QHBoxLayout(m_nowCard);
    lay->setContentsMargins(12, 10, 10, 10);
    lay->setSpacing(9);

    m_coverLabel = new QLabel;
    m_coverLabel->setFixedSize(48, 48);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setText("♫");
    m_coverLabel->setCursor(Qt::PointingHandCursor);
    m_coverLabel->setStyleSheet("background: #34353a; border-radius: 6px; color: #a0a0a0; font-size: 20px;");
    m_coverLabel->installEventFilter(this);
    lay->addWidget(m_coverLabel);

    QVBoxLayout *info = new QVBoxLayout;
    info->setContentsMargins(0, 2, 0, 2);
    info->setSpacing(3);
    m_titleLabel = new QLabel("未播放");
    m_titleLabel->setStyleSheet("color: #e6e6e6; font-size: 12.5px; font-weight: 500; background: transparent;");
    m_titleLabel->setWordWrap(false);
    QFontMetrics fm(m_titleLabel->font());
    m_titleLabel->setFixedWidth(92);
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_artistLabel = new QLabel("—");
    m_artistLabel->setStyleSheet("color: #8a8a8a; font-size: 11px; background: transparent;");
    m_artistLabel->setFixedWidth(92);
    info->addWidget(m_titleLabel);
    info->addWidget(m_artistLabel);
    info->addStretch();
    lay->addLayout(info);

    QVBoxLayout *right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(2);
    m_likeBtn = new QPushButton("♡");
    m_likeBtn->setFixedSize(26, 26);
    m_likeBtn->setCursor(Qt::PointingHandCursor);
    m_likeBtn->setFocusPolicy(Qt::NoFocus);
    m_likeBtn->setStyleSheet("QPushButton { background: transparent; border: none; font-size: 17px; color: #a0a0a0; } QPushButton:hover { color: #c20c0c; }");
    connect(m_likeBtn, &QPushButton::clicked, this, &MusicPlayerWidget::toggleLike);
    NCButton *expandBtn = new NCButton(NCIcons::Expand);
    expandBtn->setFixedSize(26, 26);
    expandBtn->setPadding(5);
    expandBtn->setBaseColor(QColor("#a0a0a0"));
    expandBtn->setToolTip("展开播放页");
    connect(expandBtn, &QPushButton::clicked, this, &MusicPlayerWidget::openFullScreenPlayer);
    right->addWidget(m_likeBtn);
    right->addWidget(expandBtn);
    lay->addLayout(right);

    qobject_cast<QVBoxLayout*>(m_sidebar->layout())->addWidget(m_nowCard);
}

void MusicPlayerWidget::setupSongTable()
{
    m_mainArea = new QWidget;
    m_mainArea->setStyleSheet("background: #1b1c1f;");
    QVBoxLayout *layout = new QVBoxLayout(m_mainArea);
    layout->setContentsMargins(30, 24, 30, 16);
    layout->setSpacing(12);

    // Header
    QHBoxLayout *header = new QHBoxLayout;
    m_playlistTitle = new QLabel("本地音乐");
    m_playlistTitle->setStyleSheet("font-size: 22px; font-weight: bold; color: #f0f0f0;");
    m_songCountLabel = new QLabel("0 首");
    m_songCountLabel->setStyleSheet("color: #8a8a8a; font-size: 13px;");
    header->addWidget(m_playlistTitle);
    header->addSpacing(12);
    header->addWidget(m_songCountLabel);
    header->addStretch();

    m_playAllBtn = new QPushButton("  播放全部  ");
    m_playAllBtn->setStyleSheet("QPushButton { background: #c20c0c; color: #ffffff; border: none; border-radius: 18px; padding: 8px 20px; font-size: 13px; font-weight: bold; } QPushButton:hover { background: #d61010; }");
    connect(m_playAllBtn, &QPushButton::clicked, this, &MusicPlayerWidget::playAll);
    header->addWidget(m_playAllBtn);

    QPushButton *importFolderBtn = new QPushButton("📁 导入文件夹");
    importFolderBtn->setStyleSheet("QPushButton { background: #2c2d32; color: #d0d0d0; border: 1px solid #3a3b40; border-radius: 18px; padding: 8px 16px; font-size: 13px; } QPushButton:hover { background: #34353a; color: #ffffff; }");
    connect(importFolderBtn, &QPushButton::clicked, this, &MusicPlayerWidget::importFolder);
    header->addWidget(importFolderBtn);

    QPushButton *importPlaylistBtn = new QPushButton("☁ 导入歌单");
    importPlaylistBtn->setStyleSheet("QPushButton { background: #2c2d32; color: #d0d0d0; border: 1px solid #3a3b40; border-radius: 18px; padding: 8px 16px; font-size: 13px; } QPushButton:hover { background: #34353a; color: #ffffff; }");
    connect(importPlaylistBtn, &QPushButton::clicked, this, &MusicPlayerWidget::importPlaylist);
    header->addWidget(importPlaylistBtn);

    header->addSpacing(8);

    layout->addLayout(header);

    // Song table
    m_songTable = new QTableWidget;
    m_songTable->setColumnCount(5);
    m_songTable->setHorizontalHeaderLabels({"#", "标题", "歌手", "专辑", "时长"});
    m_songTable->verticalHeader()->setVisible(false);
    m_songTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_songTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_songTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_songTable->setShowGrid(false);
    m_songTable->setAlternatingRowColors(true);
    m_songTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_songTable->setStyleSheet(R"(
        QTableWidget { background: #232428; border: none; font-size: 13px; color: #d0d0d0; alternate-background-color: rgba(255,255,255,0.03); outline: none; }
        QTableWidget::item { padding: 8px; border: none; }
        QTableWidget::item:selected { background: #34353a; color: #ffffff; }
        QHeaderView::section { background: #1b1c1f; color: #8a8a8a; border: none; border-bottom: 1px solid #2c2d32; padding: 8px; font-weight: normal; font-size: 12px; }
    )");
    m_songTable->horizontalHeader()->setStretchLastSection(false);
    m_songTable->setColumnWidth(0, 40);
    m_songTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_songTable->setColumnWidth(2, 150);
    m_songTable->setColumnWidth(3, 150);
    m_songTable->setColumnWidth(4, 80);
    connect(m_songTable, &QTableWidget::cellDoubleClicked, this, &MusicPlayerWidget::onSongDoubleClicked);
    connect(m_songTable, &QTableWidget::customContextMenuRequested, this, &MusicPlayerWidget::onSongMenu);
    layout->addWidget(m_songTable, 1);

    // Empty state placeholder
    m_emptyLabel = new QLabel;
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #6f6f72; font-size: 14px;");
    m_emptyLabel->setText(
        "<div style='font-size:46px; margin-bottom:10px;'>🎵</div>"
        "<div style='color:#d0d0d0; font-size:14px; font-weight:600; margin-bottom:4px;'>歌单还是空的</div>"
        "<div style='color:#8a8a8a; font-size:12px;'>点击右上角「导入音乐」添加歌曲，<br>或把音频文件放进音乐文件夹自动导入</div>");
    layout->addWidget(m_emptyLabel, 1);
    m_emptyLabel->hide();
}

// ---- Light card delegate for the music shelf (own code, mirrors reader shelf) ----
enum {
    SongArtistRole = Qt::UserRole + 1,
    SongAlbumRole,
    SongDurationRole,
    SongLikedRole,
    SongPathRole,
    SongPlayingRole,
    SongCoverPathRole   // absolute path to cached embedded cover image
};

class SongCardDelegate : public QStyledItemDelegate
{
public:
    explicit SongCardDelegate(QObject *p = nullptr) : QStyledItemDelegate(p) {}
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    { return QSize(360, 156); }
    void paint(QPainter *painter, const QStyleOptionViewItem &opt,
               const QModelIndex &idx) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect card = opt.rect.adjusted(4, 4, -4, -8);
        const bool playing = idx.data(SongPlayingRole).toBool();
        const bool hovered = opt.state & QStyle::State_MouseOver;
        painter->setPen(QPen(QColor(playing ? "#2FA66A" : "#E5E7EB"), playing ? 1.2 : 0.8));
        painter->setBrush(QColor(playing ? "#F0FBF5" : (hovered ? "#FAFBFC" : "#FFFFFF")));
        painter->drawRoundedRect(card, 16, 16);

        // Cover tile 88x88 (square album art), rounded 10
        const QRect cover(card.left() + 20, card.top() + 34, 88, 88);
        const QString coverPath = idx.data(SongCoverPathRole).toString();
        QPixmap coverPix;
        if (!coverPath.isEmpty() && QFile::exists(coverPath)) {
            coverPix.load(coverPath);
        }
        painter->setPen(Qt::NoPen);
        if (!coverPix.isNull()) {
            painter->setBrush(QColor("#E5E7EB"));
            painter->drawRoundedRect(cover, 10, 10);
            // Clip rounded rect and draw cover scaled to fill.
            painter->save();
            QPainterPath clip;
            clip.addRoundedRect(cover, 10, 10);
            painter->setClipPath(clip);
            QPixmap scaled = coverPix.scaled(cover.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            painter->drawPixmap(cover, scaled, QRect(QPoint(0,0), cover.size()));
            painter->restore();
        } else {
            QLinearGradient cg(cover.topLeft(), cover.bottomRight());
            cg.setColorAt(0, playing ? QColor("#2FA66A") : QColor("#6D9DD6"));
            cg.setColorAt(1, playing ? QColor("#7CC79B") : QColor("#8E7CC3"));
            painter->setBrush(cg);
            painter->drawRoundedRect(cover, 10, 10);
            painter->setPen(QColor(255,255,255,235));
            QFont mf("Microsoft YaHei", 26, QFont::Bold);
            painter->setFont(mf);
            painter->drawText(cover, Qt::AlignCenter, QStringLiteral("♪"));
        }

        const int textX = card.left() + 124;
        const int right = card.right() - 20;
        const int pillW = 80;
        const int textRight = right - pillW - 16;

        // Title
        QFont titleF("Microsoft YaHei", 11, QFont::Medium);
        painter->setFont(titleF);
        painter->setPen(QColor(playing ? "#1F7A4B" : "#111827"));
        QFontMetrics tm(titleF);
        painter->drawText(QRect(textX, card.top()+28, textRight-textX, 22),
                          Qt::AlignLeft|Qt::AlignVCenter,
                          tm.elidedText(idx.data(Qt::DisplayRole).toString(), Qt::ElideRight, textRight-textX));

        // Artist · album
        QFont bodyF("Microsoft YaHei", 9);
        painter->setFont(bodyF);
        painter->setPen(QColor("#9CA3AF"));
        QFontMetrics bm(bodyF);
        QString sub = idx.data(SongArtistRole).toString();
        if (sub.isEmpty()) sub = QStringLiteral("未知歌手");
        const QString album = idx.data(SongAlbumRole).toString();
        if (!album.isEmpty()) sub += QStringLiteral(" · ") + album;
        painter->drawText(QRect(textX, card.top()+56, textRight-textX, 18),
                          Qt::AlignLeft|Qt::AlignVCenter,
                          bm.elidedText(sub, Qt::ElideRight, textRight-textX));

        // Duration / status line
        qint64 dur = idx.data(SongDurationRole).toLongLong();
        QString meta;
        if (dur > 0) {
            qint64 sec = dur/1000;
            meta = QStringLiteral("时长 %1:%2").arg(sec/60).arg(sec%60,2,10,QChar('0'));
        }
        if (playing) {
            painter->setPen(QColor("#2FA66A"));
            painter->setFont(QFont("Microsoft YaHei", 9, QFont::Medium));
            painter->drawText(QRect(textX, card.top()+82, textRight-textX, 18),
                              Qt::AlignLeft|Qt::AlignVCenter,
                              (meta.isEmpty() ? QStringLiteral("正在播放") : meta + QStringLiteral(" · 正在播放")));
        } else if (!meta.isEmpty()) {
            painter->setPen(QColor("#9CA3AF"));
            painter->setFont(bodyF);
            painter->drawText(QRect(textX, card.top()+82, textRight-textX, 18),
                              Qt::AlignLeft|Qt::AlignVCenter, meta);
        }

        // Three-dot menu (top-right of card, like book cards)
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#D1D5DB"));
        const int menuY = card.top() + 14;
        painter->drawEllipse(QPoint(right - 14, menuY), 2, 2);
        painter->drawEllipse(QPoint(right - 7, menuY), 2, 2);
        painter->drawEllipse(QPoint(right, menuY), 2, 2);

        // Right pill: ▶ 播放 / ★ 已喜欢
        QRect pill(right-pillW, card.top()+62, pillW, 32);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#EAF3DE"));
        painter->drawRoundedRect(pill, 16, 16);
        painter->setPen(QColor("#27500A"));
        painter->setFont(QFont("Microsoft YaHei", 9, QFont::Medium));
        QString pillText = idx.data(SongLikedRole).toBool() ? QStringLiteral("★ 已喜欢") : QStringLiteral("▶ 播放");
        painter->drawText(pill, Qt::AlignCenter, pillText);
        painter->restore();
    }
};

void MusicPlayerWidget::setupMusicShelfPage()
{
    m_musicShelfPage = new QWidget;
    m_musicShelfPage->setObjectName("musicShelfPage");
    m_musicShelfPage->setStyleSheet(R"(
        QWidget#musicShelfPage { background: #F5F7FA; }
        QLabel { background: transparent; }
    )");

    QHBoxLayout *root = new QHBoxLayout(m_musicShelfPage);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // ---- Left sidebar (content browser) ----
    QWidget *sb = new QWidget;
    sb->setFixedWidth(220);
    sb->setStyleSheet("background:#FFFFFF; border-right:1px solid #E5E7EB;");
    QVBoxLayout *sbl = new QVBoxLayout(sb);
    sbl->setContentsMargins(16,20,16,20);
    sbl->setSpacing(18);

    QLabel *browserLabel = new QLabel("内容浏览器");
    browserLabel->setStyleSheet("font-size:13px; font-weight:500; color:#6B7280;");
    sbl->addWidget(browserLabel);

    // Active row "音乐"
    QWidget *activeRow = new QWidget;
    activeRow->setStyleSheet("background:#EAF3DE; border-radius:10px;");
    QHBoxLayout *al = new QHBoxLayout(activeRow);
    al->setContentsMargins(12,8,8,8); al->setSpacing(8);
    QLabel *ai = new QLabel("▮"); ai->setStyleSheet("color:#27500A; font-size:14px;");
    QLabel *at = new QLabel("音乐"); at->setStyleSheet("color:#27500A; font-size:14px; font-weight:500;");
    al->addWidget(ai); al->addWidget(at); al->addStretch();
    QLabel *ch = new QLabel("›"); ch->setStyleSheet("color:#27500A; font-size:16px;");
    al->addWidget(ch);
    sbl->addWidget(activeRow);

    // Stats: total songs + liked
    QHBoxLayout *stats = new QHBoxLayout; stats->setSpacing(12);
    QWidget *sc1 = new QWidget; sc1->setStyleSheet("background:#F3F4F6; border-radius:10px;");
    QVBoxLayout *sc1l = new QVBoxLayout(sc1); sc1l->setContentsMargins(12,12,12,12);
    m_sidebarSongCount = new QLabel("0 首");
    m_sidebarSongCount->setAlignment(Qt::AlignCenter);
    m_sidebarSongCount->setStyleSheet("font-size:14px; font-weight:600; color:#111827;");
    sc1l->addWidget(m_sidebarSongCount); stats->addWidget(sc1,1);
    QWidget *sc2 = new QWidget; sc2->setStyleSheet("background:#F3F4F6; border-radius:10px;");
    QVBoxLayout *sc2l = new QVBoxLayout(sc2); sc2l->setContentsMargins(12,12,12,12);
    m_sidebarLikedCount = new QLabel("0 喜欢");
    m_sidebarLikedCount->setAlignment(Qt::AlignCenter);
    m_sidebarLikedCount->setStyleSheet("font-size:14px; font-weight:600; color:#111827;");
    sc2l->addWidget(m_sidebarLikedCount); stats->addWidget(sc2,1);
    sbl->addLayout(stats);

    QLabel *favLabel = new QLabel("我的喜欢");
    favLabel->setStyleSheet("font-size:13px; font-weight:500; color:#6B7280;");
    sbl->addWidget(favLabel);
    QWidget *favItem = new QWidget;
    favItem->setStyleSheet("background:#F9FAFB; border-radius:10px;");
    QVBoxLayout *fl = new QVBoxLayout(favItem);
    fl->setContentsMargins(12,10,12,10); fl->setSpacing(2);
    m_sidebarRecentTitle = new QLabel("暂无喜欢");
    m_sidebarRecentTitle->setStyleSheet("font-size:13px; font-weight:500; color:#111827;");
    m_sidebarRecentTime = new QLabel("点赞歌曲后显示");
    m_sidebarRecentTime->setStyleSheet("font-size:11px; color:#9CA3AF;");
    fl->addWidget(m_sidebarRecentTitle); fl->addWidget(m_sidebarRecentTime);
    sbl->addWidget(favItem);
    sbl->addStretch();
    QPushButton *newGroup = new QPushButton("+ 新建歌单");
    newGroup->setCursor(Qt::PointingHandCursor);
    newGroup->setStyleSheet("QPushButton{background:transparent; color:#6B7280; border:1px dashed #D1D5DB; border-radius:10px; padding:10px; font-size:13px;}"
                            "QPushButton:hover{border-color:#2FA66A; color:#2FA66A;}");
    connect(newGroup, &QPushButton::clicked, this, &MusicPlayerWidget::addPlaylist);
    sbl->addWidget(newGroup);

    root->addWidget(sb);

    // ---- Content area ----
    QWidget *content = new QWidget;
    content->setStyleSheet("background:transparent;");
    QVBoxLayout *cl = new QVBoxLayout(content);
    cl->setContentsMargins(24,22,24,22); cl->setSpacing(16);

    QHBoxLayout *header = new QHBoxLayout; header->setSpacing(12);
    QVBoxLayout *tb = new QVBoxLayout; tb->setSpacing(2);
    QLabel *title = new QLabel("音乐");
    title->setStyleSheet("font-size:22px; font-weight:500; color:#111827;");
    m_shelfSongCount = new QLabel("0 首歌曲");
    m_shelfSongCount->setStyleSheet("font-size:13px; color:#6B7280;");
    tb->addWidget(title); tb->addWidget(m_shelfSongCount);
    m_musicImportBtn = new QPushButton("导入音乐");
    m_musicImportBtn->setCursor(Qt::PointingHandCursor);
    m_musicImportBtn->setFixedSize(112,36);
    m_musicImportBtn->setStyleSheet("QPushButton{background:#2FA66A; color:#fff; border:none; border-radius:18px; padding:0 18px; font-size:13px; font-weight:500;}"
                                    "QPushButton:hover{background:#27945D;} QPushButton:pressed{background:#1F7A4B;}");
    connect(m_musicImportBtn, &QPushButton::clicked, this, &MusicPlayerWidget::importMusic);
    header->addLayout(tb); header->addStretch(); header->addWidget(m_musicImportBtn,0,Qt::AlignTop);
    cl->addLayout(header);

    m_songCardList = new QListWidget;
    m_songCardList->setObjectName("musicSongCards");
    m_songCardList->setViewMode(QListView::ListMode);
    m_songCardList->setResizeMode(QListView::Adjust);
    m_songCardList->setMovement(QListView::Static);
    m_songCardList->setSpacing(0);
    m_songCardList->setUniformItemSizes(true);
    m_songCardList->setItemDelegate(new SongCardDelegate(m_songCardList));
    m_songCardList->setStyleSheet(R"(
        QListWidget#musicSongCards{background:transparent; border:none; outline:none; padding:0;}
        QListWidget#musicSongCards::item{background:transparent; border:none;}
        QScrollBar:vertical{background:transparent; width:10px; border:none;}
        QScrollBar::handle:vertical{background:#D1D5DB; border-radius:5px; min-height:32px; margin:2px;}
        QScrollBar::handle:vertical:hover{background:#9CA3AF;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
    )");
    connect(m_songCardList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it){
        const QString path = it->data(SongPathRole).toString();
        const QList<SongInfo> songs = currentSongList();
        for (int i = 0; i < songs.size(); ++i) {
            if (songs.at(i).path == path) { playSongAt(i); return; }
        }
    });
    cl->addWidget(m_songCardList, 1);

    // Bottom two cards
    QHBoxLayout *bottom = new QHBoxLayout; bottom->setSpacing(16);
    QWidget *card1 = new QWidget; card1->setFixedHeight(120);
    card1->setStyleSheet("background:#FFFFFF; border:1px solid #E5E7EB; border-radius:16px;");
    QHBoxLayout *c1l = new QHBoxLayout(card1); c1l->setContentsMargins(20,18,20,18); c1l->setSpacing(16);
    QLabel *c1i = new QLabel("♥"); c1i->setFixedSize(36,36); c1i->setAlignment(Qt::AlignCenter);
    c1i->setStyleSheet("background:#FDE7EF; color:#C2185B; border-radius:18px; font-size:16px;");
    c1l->addWidget(c1i,0,Qt::AlignTop);
    QVBoxLayout *c1t = new QVBoxLayout; c1t->setSpacing(6);
    QLabel *c1h = new QLabel("我的喜欢"); c1h->setStyleSheet("font-size:14px; font-weight:500; color:#111827;");
    m_favCardCount = new QLabel("还没有喜欢的歌曲");
    m_favCardCount->setStyleSheet("font-size:12px; color:#6B7280;");
    QLabel *c1b = new QLabel("♡ 点赞收藏");
    c1b->setAlignment(Qt::AlignCenter); c1b->setFixedSize(86,24);
    c1b->setStyleSheet("background:#FDE7EF; color:#C2185B; border-radius:12px; font-size:12px; font-weight:500;");
    c1t->addWidget(c1h); c1t->addWidget(m_favCardCount); c1t->addWidget(c1b,0,Qt::AlignLeft);
    c1l->addLayout(c1t,1);
    bottom->addWidget(card1,1);

    QWidget *card2 = new QWidget; card2->setFixedHeight(120);
    card2->setStyleSheet("background:#FFFFFF; border:1px solid #E5E7EB; border-radius:16px;");
    QHBoxLayout *c2l = new QHBoxLayout(card2); c2l->setContentsMargins(20,18,20,18); c2l->setSpacing(16);
    QLabel *c2i = new QLabel("♫"); c2i->setFixedSize(36,36); c2i->setAlignment(Qt::AlignCenter);
    c2i->setStyleSheet("background:#E7EEFD; color:#2F5FB0; border-radius:18px; font-size:16px;");
    c2l->addWidget(c2i,0,Qt::AlignTop);
    QVBoxLayout *c2t = new QVBoxLayout; c2t->setSpacing(6);
    QLabel *c2h = new QLabel("最近播放"); c2h->setStyleSheet("font-size:14px; font-weight:500; color:#111827;");
    m_recentSongLabel = new QLabel("暂无播放记录");
    m_recentSongLabel->setStyleSheet("font-size:12px; color:#111827; font-weight:500;");
    QLabel *c2b = new QLabel("▶ 继续播放");
    c2b->setAlignment(Qt::AlignCenter); c2b->setFixedSize(86,24);
    c2b->setStyleSheet("background:#E7EEFD; color:#2F5FB0; border-radius:12px; font-size:12px; font-weight:500;");
    c2t->addWidget(c2h); c2t->addWidget(m_recentSongLabel); c2t->addWidget(c2b,0,Qt::AlignLeft);
    c2l->addLayout(c2t,1);
    bottom->addWidget(card2,1);
    cl->addLayout(bottom);

    root->addWidget(content, 1);
}

void MusicPlayerWidget::refreshMusicShelf()
{
    if (!m_songCardList) return;
    m_songCardList->clear();
    const QList<SongInfo> songs = currentSongList();

    // Cover cache dir: <cache>/LiyouMusicCovers
    const QDir coverDir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/LiyouMusicCovers");
    coverDir.mkpath(".");

    for (int i = 0; i < songs.size(); ++i) {
        const SongInfo &s = songs.at(i);
        QListWidgetItem *it = new QListWidgetItem(s.title.isEmpty() ? QFileInfo(s.path).completeBaseName() : s.title);
        it->setData(SongArtistRole, s.artist);
        it->setData(SongAlbumRole, s.album);
        it->setData(SongDurationRole, s.duration);
        it->setData(SongPathRole, s.path);
        it->setData(SongLikedRole, m_favorites.contains(s.path));
        it->setData(SongPlayingRole, (i == m_currentIndex));

        // Cache embedded cover bytes to disk once.
        QString coverPath;
        if (!s.embeddedCover.isEmpty()) {
            const QString ext = s.embeddedCoverMime.contains("png") ? "png" : "jpg";
            const QByteArray key = QCryptographicHash::hash(s.path.toUtf8(), QCryptographicHash::Sha1).toHex();
            coverPath = coverDir.filePath(key + "." + ext);
            if (!QFile::exists(coverPath)) {
                QFile cf(coverPath);
                if (cf.open(QIODevice::WriteOnly)) cf.write(s.embeddedCover);
            }
        }
        it->setData(SongCoverPathRole, coverPath);
        it->setSizeHint(QSize(360, 156));
        m_songCardList->addItem(it);
    }
    const int total = songs.size();
    if (m_shelfSongCount) m_shelfSongCount->setText(QStringLiteral("%1 首歌曲").arg(total));
    if (m_sidebarSongCount) m_sidebarSongCount->setText(QStringLiteral("%1 首").arg(total));
    if (m_sidebarLikedCount) m_sidebarLikedCount->setText(QStringLiteral("%1 喜欢").arg(m_favorites.size()));
    if (m_favCardCount) m_favCardCount->setText(QStringLiteral("共 %1 首喜欢的歌曲").arg(m_favorites.size()));
    // sidebar recent liked
    if (m_sidebarRecentTitle) {
        if (!m_favorites.isEmpty()) {
            for (const SongInfo &s : m_allSongs) {
                if (m_favorites.contains(s.path)) {
                    m_sidebarRecentTitle->setText(s.title);
                    m_sidebarRecentTime->setText(QStringLiteral("已加入喜欢"));
                    break;
                }
            }
        } else {
            m_sidebarRecentTitle->setText("暂无喜欢");
            m_sidebarRecentTime->setText("点赞歌曲后显示");
        }
    }
    // recent playing
    if (m_recentSongLabel && m_currentIndex >= 0 && m_currentIndex < m_allSongs.size()) {
        const SongInfo &s = m_allSongs.at(m_currentIndex);
        m_recentSongLabel->setText(s.title.isEmpty() ? QFileInfo(s.path).completeBaseName() : s.title);
    }
}

void MusicPlayerWidget::setupPlayerBar()
{
    m_playerBar = new QWidget;
    m_playerBar->setFixedHeight(66);
    m_playerBar->setStyleSheet("background: #2b2c2f; border-top: 1px solid #17181b;");
    QHBoxLayout *layout = new QHBoxLayout(m_playerBar);
    layout->setContentsMargins(16, 0, 14, 0);
    layout->setSpacing(8);

    // ---- transport: [prev][play/pause][next] ----
    m_prevBtn = new CircleTransportButton(true);
    m_prevBtn->setToolTip("上一首");
    m_playBtn = new PlayPauseButton;
    m_playBtn->setOutlineStyle(true);
    m_playBtn->setFixedSize(42, 42);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setFocusPolicy(Qt::NoFocus);
    m_nextBtn = new CircleTransportButton(false);
    m_nextBtn->setToolTip("下一首");
    connect(m_prevBtn, &QPushButton::clicked, this, &MusicPlayerWidget::playPrev);
    connect(m_playBtn, &QPushButton::clicked, this, &MusicPlayerWidget::togglePlay);
    connect(m_nextBtn, &QPushButton::clicked, this, &MusicPlayerWidget::playNext);
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_playBtn);
    layout->addWidget(m_nextBtn);
    layout->addSpacing(12);

    // ---- progress: current time, bar, total time ----
    m_curTimeLabel = new QLabel("00:00");
    m_curTimeLabel->setStyleSheet("color: #8a8a8a; font-size: 11px;");
    m_curTimeLabel->setFixedWidth(38);
    m_curTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_seekBar = new ThinSlider;
    m_seekBar->setRange(0, 1000);
    m_seekBar->setLive(false);
    m_totalTimeLabel = new QLabel("00:00");
    m_totalTimeLabel->setStyleSheet("color: #8a8a8a; font-size: 11px;");
    m_totalTimeLabel->setFixedWidth(38);
    m_totalTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(m_seekBar, &ThinSlider::seekFinished, this, [this](int v) {
        const qint64 dur = m_player->duration();
        if (dur > 0) m_player->setPosition(dur * v / 1000);
    });
    // While dragging, preview the time label without touching playback.
    connect(m_seekBar, &ThinSlider::valueChanged, this, [this](int v) {
        const qint64 dur = m_player->duration();
        m_curTimeLabel->setText(formatTime(dur * v / 1000));
    });
    layout->addWidget(m_curTimeLabel);
    layout->addWidget(m_seekBar, 1);
    layout->addWidget(m_totalTimeLabel);

    layout->addStretch();

    // ---- volume ----
    m_muteBtn = new NCButton(NCIcons::VolumeHigh);
    m_muteBtn->setFixedSize(28, 28);
    m_muteBtn->setPadding(4);
    m_muteBtn->setToolTip("静音");
    connect(m_muteBtn, &QPushButton::clicked, this, &MusicPlayerWidget::toggleMute);
    layout->addWidget(m_muteBtn);

    m_volumeBar = new ThinSlider;
    m_volumeBar->setRange(0, 100);
    m_volumeBar->setFixedWidth(88);
    m_volumeBar->setLive(true);
    m_volumeBar->setValue(70);
    connect(m_volumeBar, &ThinSlider::valueChanged, this,
            QOverload<int>::of(&MusicPlayerWidget::setVolume));
    layout->addWidget(m_volumeBar);
    layout->addSpacing(6);

    // ---- mode, queue, lyrics ----
    m_modeBtn = new NCButton(NCIcons::Repeat);
    m_modeBtn->setFixedSize(30, 30);
    m_modeBtn->setPadding(5);
    m_modeBtn->setToolTip("列表循环");
    connect(m_modeBtn, &QPushButton::clicked, this, &MusicPlayerWidget::cyclePlayMode);
    layout->addWidget(m_modeBtn);

    m_queueBtn = new QueueButton;
    m_queueBtn->setToolTip("播放队列");
    connect(m_queueBtn, &QPushButton::clicked, this, [this]() { toggleQueuePanel(); });
    layout->addWidget(m_queueBtn);

    m_lyricsBtn = new LyricsToggleButton;
    connect(m_lyricsBtn, &QPushButton::clicked, this, &MusicPlayerWidget::toggleLyrics);
    layout->addWidget(m_lyricsBtn);
}

void MusicPlayerWidget::loadLibrary()
{
    QFile file(m_libraryRoot + "/library.json");
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    m_allSongs.clear();
    QJsonArray songsArr = root["songs"].toArray();
    for (const auto &v : songsArr) {
        QJsonObject obj = v.toObject();
        SongInfo song;
        song.path = obj["path"].toString();
        song.title = obj["title"].toString();
        song.artist = obj["artist"].toString();
        song.album = obj["album"].toString();
        song.duration = (qint64)obj["duration"].toDouble();
        if (QFile::exists(song.path)) m_allSongs << song;
    }

    m_playlists.clear();
    QJsonArray plArr = root["playlists"].toArray();
    for (const auto &v : plArr) {
        QJsonObject obj = v.toObject();
        PlaylistInfo pl;
        pl.name = obj["name"].toString();
        pl.songPaths = obj["songs"].toVariant().toStringList();
        m_playlists << pl;
    }

    m_favorites = root["favorites"].toVariant().toStringList();

    // One-time metadata refresh for songs imported before tag parsing existed.
    // Only touched when the stored entry still shows placeholder artist/album.
    bool metaChanged = false;
    for (auto &song : m_allSongs) {
        if (song.artist == "未知歌手" && song.album == "未知专辑") {
            const SongInfo fresh = SongMetadata::read(song.path);
            if (fresh.title != song.title || fresh.artist != song.artist ||
                fresh.album != song.album || (fresh.duration > 0 && fresh.duration != song.duration)) {
                song.title = fresh.title;
                song.artist = fresh.artist;
                song.album = fresh.album;
                if (fresh.duration > 0) song.duration = fresh.duration;
                metaChanged = true;
            }
        }
    }
    if (metaChanged) saveLibrary();

    // Refresh nav with playlists (keep the 8 fixed items above)
    while (m_navList->count() > 8) m_navList->takeItem(8);
    for (const auto &pl : m_playlists) {
        m_navList->addItem("  " + pl.name);
    }

    refreshSongTable();
}

void MusicPlayerWidget::setupMusicWatcher()
{
    m_musicWatcher = new QFileSystemWatcher(this);
    m_musicScanTimer = new QTimer(this);
    m_musicScanTimer->setSingleShot(true);
    m_musicScanTimer->setInterval(400);
    connect(m_musicScanTimer, &QTimer::timeout, this, &MusicPlayerWidget::scanMusicFolder);
    connect(m_musicWatcher, &QFileSystemWatcher::directoryChanged, this, &MusicPlayerWidget::onMusicDirChanged);
    m_musicWatcher->addPath(m_libraryRoot);
}

void MusicPlayerWidget::onMusicDirChanged(const QString &path)
{
    Q_UNUSED(path);
    m_musicScanTimer->start();
}

// Auto-register audio files dropped directly into the music folder (non-destructive add).
void MusicPlayerWidget::scanMusicFolder()
{
    static const QStringList supportedExts = {"mp3","wav","flac","ogg","m4a","aac","wma","opus","ape","wv","mpc","tta"};
    QSet<QString> existing;
    existing.reserve(m_allSongs.size());
    for (const auto &s : m_allSongs) existing.insert(s.path);

    bool added = false;
    QDir d(m_libraryRoot);
    const QFileInfoList entries = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString ext = fi.suffix().toLower();
        if (!supportedExts.contains(ext)) continue;
        const QString f = fi.absoluteFilePath();
        if (existing.contains(f)) continue;
        // Fingerprint-based cache: skip re-parsing when file unchanged.
        const QString fp = MusicLibraryDb::makeFingerprint(fi.size(), fi.lastModified().toMSecsSinceEpoch());
        SongInfo song;
        if (!m_db || !m_db->loadCached(f, fp, song)) {
            song = SongMetadata::read(f);
            if (m_db) m_db->upsertSong(song, fi.size(), fi.lastModified().toMSecsSinceEpoch());
        }
        m_allSongs << song;
        existing.insert(f);
        added = true;
    }
    if (added) {
        saveLibrary();
        refreshSongTable();
    }
}

void MusicPlayerWidget::saveLibrary()
{
    QJsonObject root;

    QJsonArray songsArr;
    for (const auto &song : m_allSongs) {
        QJsonObject obj;
        obj["path"] = song.path;
        obj["title"] = song.title;
        obj["artist"] = song.artist;
        obj["album"] = song.album;
        obj["duration"] = (double)song.duration;
        songsArr << obj;
    }
    root["songs"] = songsArr;

    QJsonArray plArr;
    for (const auto &pl : m_playlists) {
        QJsonObject obj;
        obj["name"] = pl.name;
        obj["songs"] = QJsonArray::fromStringList(pl.songPaths);
        plArr << obj;
    }
    root["playlists"] = plArr;
    root["favorites"] = QJsonArray::fromStringList(m_favorites);

    QFile file(m_libraryRoot + "/library.json");
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson());
}

void MusicPlayerWidget::refreshSongTable()
{
    refreshMusicShelf();  // keep the light card shelf in sync
    const QList<SongInfo> full = currentSongList();
    QList<SongInfo> filtered;
    const QList<SongInfo> *songs = &full;

    // Apply search filter
    if (!m_searchText.isEmpty()) {
        filtered.reserve(full.size());
        for (const auto &s : full) {
            if (s.title.contains(m_searchText, Qt::CaseInsensitive) ||
                s.artist.contains(m_searchText, Qt::CaseInsensitive) ||
                s.album.contains(m_searchText, Qt::CaseInsensitive)) {
                filtered << s;
            }
        }
        songs = &filtered;
    }
    m_songTable->setRowCount(songs->size());
    m_songCountLabel->setText(QString("%1 首").arg(songs->size()));

    // Show empty state when no songs
    if (songs->isEmpty()) {
        m_songTable->hide();
        m_emptyLabel->show();
        m_rowByPath.clear();
        m_highlightedRow = -1;
        refreshQueuePanel();
        return;
    } else {
        m_songTable->show();
        m_emptyLabel->hide();
    }

    // Which displayed row is the playing song? With a search filter active that is
    // not m_currentIndex, so the song is located by path instead.
    int playingRow = -1;
    if (m_currentIndex >= 0 && m_currentIndex < full.size()) {
        const QString currentPath = full.at(m_currentIndex).path;
        for (int i = 0; i < songs->size(); ++i) {
            if (songs->at(i).path == currentPath) { playingRow = i; break; }
        }
    }

    m_rowByPath.clear();
    m_rowByPath.reserve(songs->size());
    for (int i = 0; i < songs->size(); ++i) {
        const SongInfo &song = songs->at(i);
        QTableWidgetItem *numItem = new QTableWidgetItem(QString::number(i + 1));
        numItem->setTextAlignment(Qt::AlignCenter);
        numItem->setForeground(QColor("#999"));

        QString titleText = song.title;
        if (song.isOnline) titleText = "☁ " + titleText;
        QTableWidgetItem *titleItem = new QTableWidgetItem(titleText);
        QTableWidgetItem *artistItem = new QTableWidgetItem(song.artist);
        QTableWidgetItem *albumItem = new QTableWidgetItem(song.album);
        QTableWidgetItem *durItem = new QTableWidgetItem(formatTime(song.duration));
        durItem->setTextAlignment(Qt::AlignCenter);

        // Highlight current playing
        if (i == playingRow) {
            QColor red("#c20c0c");
            numItem->setForeground(red);
            titleItem->setForeground(red);
        }

        m_songTable->setItem(i, 0, numItem);
        m_songTable->setItem(i, 1, titleItem);
        m_songTable->setItem(i, 2, artistItem);
        m_songTable->setItem(i, 3, albumItem);
        m_songTable->setItem(i, 4, durItem);
        m_rowByPath.insert(song.path, i);
    }
    m_highlightedRow = playingRow;
    refreshQueuePanel();
}

QList<SongInfo> MusicPlayerWidget::currentSongList() const
{
    if (m_currentNav == "local") return m_allSongs;
    if (m_currentNav == "favorites") {
        QList<SongInfo> result;
        for (const auto &song : m_allSongs)
            if (m_favorites.contains(song.path)) result << song;
        return result;
    }
    if (m_currentNav == "recent" || m_currentNav == "top") {
        QStringList paths = m_db
            ? (m_currentNav == "recent" ? m_db->recentPaths(50) : m_db->topPlayedPaths(50))
            : QStringList();
        QList<SongInfo> result;
        for (const auto &path : paths) {
            for (const auto &song : m_allSongs) {
                if (song.path == path) { result << song; break; }
            }
        }
        return result;
    }
    // Playlist
    for (const auto &pl : m_playlists) {
        if (pl.name == m_currentNav) {
            QList<SongInfo> result;
            for (const auto &path : pl.songPaths) {
                bool isNeteaseId = QRegularExpression("^\\d+$").match(path).hasMatch();
                for (const auto &song : m_allSongs) {
                    if (isNeteaseId) {
                        if (song.neteaseId == path.toLongLong()) { result << song; break; }
                    } else {
                        if (song.path == path) { result << song; break; }
                    }
                }
            }
            return result;
        }
    }
    return m_allSongs;
}

void MusicPlayerWidget::onNavChanged(QListWidgetItem *item)
{
    if (!item) return;
    QString text = item->text().trimmed();

    if (text == "发现音乐") {
        m_currentNav = "discovery";
        if (m_contentStack) m_contentStack->setCurrentIndex(1);
        return;
    }

    // All other items show song list page
    if (m_contentStack) m_contentStack->setCurrentIndex(2);

    if (text == "本地音乐") {
        m_currentNav = "local";
        m_playlistTitle->setText("本地音乐");
    } else if (text == "最近播放") {
        m_currentNav = "recent";
        m_playlistTitle->setText("最近播放");
    } else if (text == "最常播放") {
        m_currentNav = "top";
        m_playlistTitle->setText("最常播放");
    } else if (text == "我的收藏" || text == "我喜欢的音乐") {
        m_currentNav = "favorites";
        m_playlistTitle->setText("我的收藏");
    } else if (text == "私人FM") {
        // Shuffle all songs and play
        m_currentNav = "local";
        m_playlistTitle->setText("私人FM");
        if (!m_allSongs.isEmpty()) {
            QList<SongInfo> shuffled = m_allSongs;
            std::shuffle(shuffled.begin(), shuffled.end(), *QRandomGenerator::global());
            m_currentIndex = 0;
            // Temporarily replace allSongs order for FM
            m_allSongs = shuffled;
            playSongAt(0);
        }
    } else if (text == "视频" || text == "朋友" || text == "下载管理" || text == "我的音乐云盘") {
        m_currentNav = "local";
        m_playlistTitle->setText(text + "（敬请期待）");
    } else {
        m_currentNav = text;
        m_playlistTitle->setText(text);
    }
    m_currentIndex = -1;
    refreshSongTable();
}

void MusicPlayerWidget::addPlaylist()
{
    bool ok;
    QString name = QInputDialog::getText(this, "新建歌单", "歌单名称：", QLineEdit::Normal, "新歌单", &ok);
    if (!ok || name.isEmpty()) return;
    for (const auto &pl : m_playlists)
        if (pl.name == name) { QMessageBox::warning(this, "提示", "歌单已存在"); return; }
    PlaylistInfo pl; pl.name = name;
    m_playlists << pl;
    m_navList->addItem("  " + name);
    saveLibrary();
}

void MusicPlayerWidget::deletePlaylist()
{
    // Not used directly, could add context menu
}

void MusicPlayerWidget::onSongDoubleClicked(int row, int col)
{
    Q_UNUSED(col);
    playSongAt(row);
}

void MusicPlayerWidget::playAll()
{
    QList<SongInfo> songs = currentSongList();
    if (songs.isEmpty()) return;
    playSongAt(0);
}

void MusicPlayerWidget::importMusic()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "导入音乐", QString(),
        "音频文件 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma *.opus *.ape *.wv *.mpc *.tta);;所有文件 (*.*)");
    int added = 0, skipped = 0, unsupported = 0;
    static const QStringList supportedExts = {"mp3","wav","flac","ogg","m4a","aac","wma","opus","ape","wv","mpc","tta"};
    // O(1) duplicate lookup instead of scanning the whole library per file
    QSet<QString> existingPaths;
    existingPaths.reserve(m_allSongs.size());
    for (const auto &s : m_allSongs) existingPaths.insert(s.path);
    for (const QString &f : files) {
        QString ext = QFileInfo(f).suffix().toLower();
        if (ext == "ncm") { unsupported++; continue; }
        if (!supportedExts.contains(ext)) { unsupported++; continue; }
        if (!existingPaths.contains(f)) {
            SongInfo song = SongMetadata::read(f);
            m_allSongs << song;
            existingPaths.insert(f);
            added++;
        } else {
            skipped++;
        }
    }
    if (added > 0 || skipped > 0 || unsupported > 0) {
        QString msg = QString("导入完成：新增 %1 首，跳过重复 %2 首").arg(added).arg(skipped);
        if (unsupported > 0) msg += QString("\n不支持格式 %1 首（.ncm 等加密格式需先转换为 mp3/flac）").arg(unsupported);
        QMessageBox::information(this, "导入结果", msg);
    }
    saveLibrary();
    refreshSongTable();
}

void MusicPlayerWidget::importFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择音乐文件夹", QString());
    if (dir.isEmpty()) return;
    static const QStringList supportedExts = {"mp3","wav","flac","ogg","m4a","aac","wma","opus","ape","wv","mpc","tta"};
    QDir d(dir);
    QFileInfoList entries = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    int added = 0, skipped = 0, unsupported = 0;
    // O(1) duplicate lookup instead of scanning the whole library per file
    QSet<QString> existingPaths;
    existingPaths.reserve(m_allSongs.size());
    for (const auto &s : m_allSongs) existingPaths.insert(s.path);
    for (const QFileInfo &fi : entries) {
        QString ext = fi.suffix().toLower();
        if (ext == "ncm") { unsupported++; continue; }
        if (!supportedExts.contains(ext)) continue;
        QString f = fi.absoluteFilePath();
        if (!existingPaths.contains(f)) {
            SongInfo song = SongMetadata::read(f);
            m_allSongs << song;
            existingPaths.insert(f);
            added++;
        } else {
            skipped++;
        }
    }
    QString msg = QString("文件夹导入完成：新增 %1 首，跳过重复 %2 首").arg(added).arg(skipped);
    if (unsupported > 0) msg += QString("\n.ncm 加密格式 %1 首（需先转换为 mp3/flac）").arg(unsupported);
    QMessageBox::information(this, "导入结果", msg);
    saveLibrary();
    refreshSongTable();
}

void MusicPlayerWidget::importPlaylist()
{
    bool ok;
    QString input = QInputDialog::getText(this, "导入网易云歌单",
        "粘贴歌单链接或歌单ID：", QLineEdit::Normal, "", &ok);
    if (!ok || input.trimmed().isEmpty()) return;

    // Extract playlist ID from URL or raw ID
    QString id;
    QRegularExpression re("id=(\\d+)");
    QRegularExpressionMatch match = re.match(input);
    if (match.hasMatch()) {
        id = match.captured(1);
    } else {
        QRegularExpression re2("/(\\d+)/?");
        QRegularExpressionMatch match2 = re2.match(input);
        if (match2.hasMatch()) id = match2.captured(1);
        else id = input.trimmed();
    }

    if (id.isEmpty() || !QRegularExpression("^\\d+$").match(id).hasMatch()) {
        QMessageBox::warning(this, "导入失败", "无法识别歌单ID，请粘贴完整的网易云歌单链接。");
        return;
    }

    // Fetch playlist from NetEase API
    QUrl url(QString("https://music.163.com/api/playlist/detail?id=%1").arg(id));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    request.setRawHeader("Referer", "https://music.163.com/");
    request.setRawHeader("Cookie", "appver=2.0.2");

    if (m_pendingReply) { m_pendingReply->abort(); m_pendingReply = nullptr; }
    m_pendingReply = m_netManager->get(request);
    connect(m_pendingReply, &QNetworkReply::finished, this, &MusicPlayerWidget::onPlaylistFetched);

    QMessageBox::information(this, "正在导入", "正在获取歌单信息，请稍候...");
}

void MusicPlayerWidget::onPlaylistFetched()
{
    if (!m_pendingReply) return;
    QNetworkReply *reply = m_pendingReply;
    m_pendingReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "导入失败",
            QString("网络请求失败：%1\n\n可能原因：\n1. 歌单ID错误\n2. 网络连接问题\n3. 网易云API限制").arg(reply->errorString()));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "导入失败", "解析歌单数据失败，API返回格式异常。");
        return;
    }

    QJsonObject root = doc.object();
    if (root.value("code").toInt() != 200) {
        QMessageBox::warning(this, "导入失败",
            QString("网易云返回错误码：%1").arg(root.value("code").toInt()));
        return;
    }

    QJsonObject result = root.value("result").toObject();
    QString playlistName = result.value("name").toString("未知歌单");
    QJsonArray tracks = result.value("tracks").toArray();

    if (tracks.isEmpty()) {
        QMessageBox::warning(this, "导入失败", "歌单为空或无法获取歌曲列表。");
        return;
    }

    // Add songs as online entries
    int added = 0, matched = 0;
    for (const QJsonValue &v : tracks) {
        QJsonObject t = v.toObject();
        SongInfo song;
        song.title = t.value("name").toString("未知");
        song.neteaseId = t.value("id").toVariant().toLongLong();
        song.duration = t.value("dt").toInt(0);

        // Artist
        QJsonArray ar = t.value("ar").toArray();
        QStringList artists;
        for (const QJsonValue &a : ar) artists << a.toObject().value("name").toString();
        song.artist = artists.join(" / ");

        // Album
        song.album = t.value("al").toObject().value("name").toString("");

        // Try to match local file by title + artist
        song.isOnline = true;
        song.path = "";
        for (const auto &local : m_allSongs) {
            if (local.title == song.title && !local.isOnline) {
                song.path = local.path;
                song.isOnline = false;
                matched++;
                break;
            }
        }

        // Avoid duplicates
        bool exists = false;
        for (const auto &s : m_allSongs) {
            if (s.neteaseId == song.neteaseId && song.neteaseId > 0) { exists = true; break; }
        }
        if (!exists) {
            m_allSongs << song;
            added++;
        }
    }

    // Create a playlist for these songs
    PlaylistInfo pl;
    pl.name = "☁ " + playlistName;
    for (const auto &s : m_allSongs) {
        if (s.neteaseId > 0) pl.songPaths << QString::number(s.neteaseId);
    }
    // Avoid duplicate playlist
    bool plExists = false;
    for (const auto &p : m_playlists) if (p.name == pl.name) { plExists = true; break; }
    if (!plExists) m_playlists << pl;

    saveLibrary();
    refreshSongTable();
    setupSidebar(); // refresh playlist list

    QString msg = QString("歌单「%1」导入成功！\n新增 %2 首，本地匹配 %3 首").arg(playlistName).arg(added).arg(matched);
    if (added - matched > 0) msg += QString("\n\n%1 首为在线歌曲，点击即可在线播放（需联网；受版权或 VIP 限制的歌曲可能无法播放）。").arg(added - matched);
    QMessageBox::information(this, "导入成功", msg);
}

void MusicPlayerWidget::onSongMenu(const QPoint &pos)
{
    QTableWidgetItem *item = m_songTable->itemAt(pos);
    if (!item) return;
    int row = item->row();
    QList<SongInfo> songs = currentSongList();
    if (row >= songs.size()) return;
    QString path = songs[row].path;

    QMenu menu;
    menu.setStyleSheet("QMenu { background: #2b2c2f; color: #e0e0e0; border: 1px solid #3a3b40; } QMenu::item { padding: 8px 24px; } QMenu::item:selected { background: #34353a; color: #c20c0c; }");
    menu.addAction("播放", this, [this, row]() { playSongAt(row); });
    bool isFav = m_favorites.contains(path);
    menu.addAction(isFav ? "取消喜欢" : "添加到我喜欢", this, [this, path, isFav]() {
        if (isFav) m_favorites.removeAll(path); else m_favorites << path;
        if (m_db) m_db->setFavorite(path, !isFav);
        saveLibrary();
        if (m_currentNav == "favorites") refreshSongTable();
        updateNowPlaying();
    });

    if (!m_playlists.isEmpty()) {
        QMenu *addToMenu = menu.addMenu("添加到歌单");
        for (const auto &pl : m_playlists) {
            addToMenu->addAction(pl.name, this, [this, path, pl]() {
                for (auto &p : m_playlists) {
                    if (p.name == pl.name && !p.songPaths.contains(path)) {
                        p.songPaths << path;
                        break;
                    }
                }
                saveLibrary();
            });
        }
    }

    menu.addAction("删除", this, [this, path]() {
        for (int i = 0; i < m_allSongs.size(); ++i)
            if (m_allSongs[i].path == path) { m_allSongs.removeAt(i); break; }
        m_favorites.removeAll(path);
        for (auto &pl : m_playlists) pl.songPaths.removeAll(path);
        saveLibrary();
        refreshSongTable();
    });

    if (!path.isEmpty() && QFile::exists(path)) {
        menu.addSeparator();
        menu.addAction("📂 在文件夹中显示", this, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        menu.addAction("ℹ 歌曲详情", this, [this, path, row]() {
            QList<SongInfo> songs = currentSongList();
            if (row >= songs.size()) return;
            const SongInfo &s = songs[row];
            QFileInfo fi(path);
            QString sizeStr;
            qint64 size = fi.size();
            if (size > 1048576) sizeStr = QString::number(size / 1048576.0, 'f', 1) + " MB";
            else if (size > 1024) sizeStr = QString::number(size / 1024.0, 'f', 1) + " KB";
            else sizeStr = QString::number(size) + " B";
            QString detail = QString(
                "标题：%1\n"
                "歌手：%2\n"
                "专辑：%3\n"
                "时长：%4\n"
                "格式：%5\n"
                "大小：%6\n"
                "路径：%7"
            ).arg(s.title, s.artist, s.album, formatTime(s.duration),
                 fi.suffix().toUpper(), sizeStr, path);
            QMessageBox::information(this, "歌曲详情", detail);
        });
    }

    menu.exec(m_songTable->viewport()->mapToGlobal(pos));
}

void MusicPlayerWidget::togglePlay()
{
    // If no song is loaded, auto-play the first song in the current list
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex < 0 || m_currentIndex >= songs.size()) {
        if (!songs.isEmpty()) {
            playSongAt(0);
        }
        return;
    }
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void MusicPlayerWidget::playPrev()
{
    QList<SongInfo> songs = currentSongList();
    if (songs.isEmpty()) return;
    int idx = m_currentIndex;
    if (m_playMode == Shuffle) {
        idx = QRandomGenerator::global()->bounded(songs.size());
    } else {
        idx = (idx - 1 + songs.size()) % songs.size();
    }
    playSongAt(idx);
}

void MusicPlayerWidget::playNext()
{
    QList<SongInfo> songs = currentSongList();
    if (songs.isEmpty()) return;
    int idx = m_currentIndex;
    if (m_playMode == Shuffle) {
        idx = QRandomGenerator::global()->bounded(songs.size());
    } else if (m_playMode == LoopOne) {
        playSongAt(m_currentIndex);
        return;
    } else {
        idx = (idx + 1) % songs.size();
    }
    playSongAt(idx);
}

void MusicPlayerWidget::playSong(const QString &path)
{
    QList<SongInfo> songs = currentSongList();
    for (int i = 0; i < songs.size(); ++i) {
        if (songs[i].path == path) { playSongAt(i); return; }
    }
}

void MusicPlayerWidget::playSongAt(int index)
{
    QList<SongInfo> songs = currentSongList();
    if (index < 0 || index >= songs.size()) return;
    m_currentIndex = index;
    const SongInfo &song = songs[index];
    if (song.isOnline || song.path.isEmpty()) {
        if (song.neteaseId > 0) {
            playOnlineSong(song);
            return;
        }
        QMessageBox::information(this, "无法播放",
            QString("歌曲「%1 - %2」为在线歌曲，未找到本地文件。\n\n请先从网易云下载该歌曲（mp3/flac格式），然后用「导入音乐」或「导入文件夹」添加到本地库。").arg(song.artist).arg(song.title));
        m_currentIndex = -1;
        return;
    }
    m_player->setSource(QUrl::fromLocalFile(song.path));
    m_player->play();
    // ReplayGain: apply volume normalization (ported from mpz gain.h).
    if (m_audioOutput) {
        double db = 0.0;
        double peak = 0.0;
        if (song.hasTrackGain)      { db = song.trackGainDb; peak = song.trackPeak; }
        else if (song.hasAlbumGain) { db = song.albumGainDb; peak = song.albumPeak; }
        double linear = std::pow(10.0, db / 20.0);
        // Anti-clip: don't exceed 0 dBFS when peak known.
        if (peak > 0.0 && linear * peak > 1.0) linear = 1.0 / peak;
        linear = qBound(0.0, linear, 2.0);
        m_audioOutput->setVolume(static_cast<float>(linear));
    }
    // Persist play stats for the song we're leaving (Mineradio-style).
    if (!m_currentSongPath.isEmpty() && m_currentSongPath != song.path) {
        const qint64 listenedMs = QDateTime::currentMSecsSinceEpoch() - m_songPlayStartedAt;
        if (m_db && listenedMs > 3000)
            m_db->bumpPlay(m_currentSongPath, listenedMs);
    }
    m_currentSongPath = song.path;
    m_songPlayStartedAt = QDateTime::currentMSecsSinceEpoch();
    updateNowPlaying();
    updatePlayingRow();
    // Load lyrics deferred to avoid blocking playback start
    QString lrcPath = song.path;
    int dot = lrcPath.lastIndexOf('.');
    if (dot > 0) lrcPath = lrcPath.left(dot) + ".lrc";
    QTimer::singleShot(100, this, [this, lrcPath]() { parseLrc(lrcPath); });
}

// ==================== Online playback (NetEase Cloud) ====================

void MusicPlayerWidget::playOnlineSong(const SongInfo &song)
{
    // Abort any in-flight playlist/stream request
    if (m_pendingReply) { m_pendingReply->abort(); m_pendingReply->deleteLater(); m_pendingReply = nullptr; }

    m_titleLabel->setText(song.title);
    m_artistLabel->setText(song.artist + " · 加载中…");
    updateCoverArt(song);
    clearLyrics();

    // Resolve the real stream URL through NetEase's public redirect endpoint
    resolveStreamUrl(QString("https://music.163.com/song/media/outer/url?id=%1.mp3").arg(song.neteaseId), 0);
}

void MusicPlayerWidget::resolveStreamUrl(const QString &url, int hops)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://music.163.com/");
    req.setRawHeader("Cookie", "appver=2.0.2");
    // Do not auto-follow; we resolve the redirect chain manually
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, hops]() {
        reply->deleteLater();
        const QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (redirect.isValid()) {
            if (hops < 5) {
                // Follow the redirect chain manually
                const QUrl resolved = redirect.isRelative() ? reply->url().resolved(redirect) : redirect;
                resolveStreamUrl(resolved.toString(), hops + 1);
            } else {
                startStreaming(redirect.isRelative() ? reply->url().resolved(redirect) : redirect);
            }
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            onlinePlayFailed(QString("获取音频地址失败：%1").arg(reply->errorString()));
            return;
        }
        // No redirect: maybe an error JSON page (copyright / VIP restricted)
        onlinePlayFailed("该歌曲暂不支持在线播放（可能受版权或 VIP 限制），请尝试本地下载后导入。");
    });
}

void MusicPlayerWidget::startStreaming(const QUrl &url)
{
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex < 0 || m_currentIndex >= songs.size()) return;
    const SongInfo &song = songs[m_currentIndex];

    m_titleLabel->setText(song.title);
    m_artistLabel->setText(song.artist + " · 在线播放");
    updateCoverArt(song);

    m_player->setSource(url);
    m_player->play();
    updatePlayingRow();
    updateNowPlaying();

    // Fetch online lyrics (NetEase API)
    fetchOnlineLyrics(song.neteaseId);
}

void MusicPlayerWidget::fetchOnlineLyrics(qint64 neteaseId)
{
    QUrl url(QString("https://music.163.com/api/song/lyric?id=%1&lv=1&kv=1&tv=-1").arg(neteaseId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://music.163.com/");
    QNetworkReply *reply = m_netManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, neteaseId]() {
        reply->deleteLater();
        // Ignore stale replies: only apply if this is still the current online song
        QList<SongInfo> songs = currentSongList();
        if (m_currentIndex < 0 || m_currentIndex >= songs.size()) return;
        const SongInfo &cur = songs[m_currentIndex];
        if (cur.neteaseId != neteaseId) return;
        if (reply->error() != QNetworkReply::NoError) { parseLrcText(QString()); return; }
        const QByteArray data = reply->readAll();
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) { parseLrcText(QString()); return; }
        const QString lyric = doc.object().value("lrc").toObject().value("lyric").toString();
        parseLrcText(lyric);
    });
}

void MusicPlayerWidget::onlinePlayFailed(const QString &reason)
{
    m_titleLabel->setText("在线播放失败");
    m_artistLabel->setText(reason.left(40));
    m_currentIndex = -1;
    QMessageBox::warning(this, "在线播放失败", reason);
}

// Row of the currently playing song inside the (possibly filtered) table.
int MusicPlayerWidget::rowOfCurrentSong() const
{
    if (m_currentIndex < 0) return -1;
    const QList<SongInfo> full = currentSongList();
    if (m_currentIndex >= full.size()) return -1;
    return m_rowByPath.value(full.at(m_currentIndex).path, -1);
}

void MusicPlayerWidget::updatePlayingRow()
{
    if (!m_songTable) return;

    const int playingRow = rowOfCurrentSong();
    if (playingRow == m_highlightedRow) return;

    // Only the two rows whose colour actually changes are touched. The previous
    // version repainted every row on each track change (measured at 11 ms for
    // 20k songs and 26 ms for 50k), which is a visible hitch when skipping.
    const auto paintRow = [this](int row, bool playing) {
        if (row < 0 || row >= m_songTable->rowCount()) return;
        QTableWidgetItem *num = m_songTable->item(row, 0);
        QTableWidget *title = m_songTable;
        QTableWidgetItem *titleItem = title->item(row, 1);
        if (num) num->setForeground(playing ? QColor("#c20c0c") : QColor("#999"));
        if (titleItem) titleItem->setForeground(playing ? QColor("#c20c0c") : QColor("#d0d0d0"));
    };

    paintRow(m_highlightedRow, false);
    paintRow(playingRow, true);
    m_highlightedRow = playingRow;
}

// Sidebar card cover: real art cropped to a rounded square when available,
// otherwise a title-tinted rounded tile. Decoded once per cover.
void MusicPlayerWidget::updateCoverArt(const SongInfo &song)
{
    if (!m_coverLabel) return;

    const auto paintTile = [this](const QPixmap &art, const SongInfo &s) {
        QPixmap tile(m_coverLabel->size());
        tile.fill(Qt::transparent);
        QPainter p(&tile);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(tile.rect()), 6, 6);
        p.setClipPath(clip);
        if (!art.isNull()) {
            p.drawPixmap(0, 0, art);
        } else {
            const int hue = s.title.isEmpty() ? 205 : int(qHash(s.title) % 360);
            p.fillRect(tile.rect(), QColor::fromHsl(hue, 130, 95));
            p.setPen(QColor(230, 230, 230));
            QFont f = p.font();
            f.setBold(true);
            f.setPointSize(14);
            p.setFont(f);
            p.drawText(tile.rect(), Qt::AlignCenter,
                       s.title.isEmpty() ? QStringLiteral("♫") : s.title.left(2));
        }
        p.setClipping(false);
        m_coverLabel->setText(QString());
        m_coverLabel->setPixmap(tile);
    };

    const QString path = CoverArt::find(song.path);
    if (!path.isEmpty()) {
        if (path != m_coverArtPath) {
            QPixmap art(path);
            m_coverArtPath = path;
            m_coverArtPix = art.isNull()
                ? QPixmap()
                : art.scaled(m_coverLabel->size(), Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation);
        }
        if (!m_coverArtPix.isNull()) {
            paintTile(m_coverArtPix, song);
            return;
        }
    }
    m_coverArtPath.clear();
    m_coverArtPix = QPixmap();
    paintTile(QPixmap(), song);
}

void MusicPlayerWidget::updateNowPlaying()
{
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex < 0 || m_currentIndex >= songs.size()) {
        m_titleLabel->setText("未播放");
        m_artistLabel->setText("—");
        m_likeBtn->setText("♡");
        if (m_miniPlayer) m_miniPlayer->setSong("未播放", "");
        refreshQueuePanel();
        return;
    }
    const SongInfo &song = songs[m_currentIndex];
    const QFontMetrics tfm(m_titleLabel->font());
    const QFontMetrics afm(m_artistLabel->font());
    m_titleLabel->setText(tfm.elidedText(song.title, Qt::ElideRight, m_titleLabel->width()));
    m_titleLabel->setToolTip(song.title);
    m_artistLabel->setText(afm.elidedText(song.artist, Qt::ElideRight, m_artistLabel->width()));
    m_artistLabel->setToolTip(song.artist);
    updateCoverArt(song);
    if (m_miniPlayer) m_miniPlayer->setSong(song.title, song.artist, CoverArt::find(song.path));
    const bool isFav = m_favorites.contains(song.path);
    m_likeBtn->setText(isFav ? "♥" : "♡");
    m_likeBtn->setStyleSheet(isFav
        ? "QPushButton { background: transparent; border: none; font-size: 17px; color: #c20c0c; }"
        : "QPushButton { background: transparent; border: none; font-size: 17px; color: #a0a0a0; } QPushButton:hover { color: #c20c0c; }");
    refreshQueuePanel();
}

void MusicPlayerWidget::onPositionChanged(qint64 pos)
{
    const qint64 dur = m_player->duration();
    if (!m_seekBar->isSliding() && dur > 0)
        m_seekBar->setValue(int(pos * 1000 / dur));
    m_curTimeLabel->setText(formatTime(pos));
    m_totalTimeLabel->setText(formatTime(dur));
    // Gapless-ish handoff: within 2s of end, pre-resolve next track so
    // EndOfMedia fires an immediate setSource+play with zero list rebuild.
    if (dur > 5000 && pos > dur - 2000 && m_playMode != LoopOne) {
        if (m_nextPreCachedPath.isEmpty()) {
            QList<SongInfo> songs = currentSongList();
            if (!songs.isEmpty()) {
                int nextIdx;
                if (m_playMode == Shuffle) {
                    nextIdx = QRandomGenerator::global()->bounded(songs.size());
                } else {
                    nextIdx = (m_currentIndex + 1) % songs.size();
                }
                m_nextPreCachedPath = songs[nextIdx].path;
                m_nextPreCachedIndex = nextIdx;
            }
        }
    } else if (pos < dur - 2500) {
        m_nextPreCachedPath.clear();
        m_nextPreCachedIndex = -1;
    }
    if (m_fullScreenPlayer && m_fullScreenPlayer->isVisible()) {
        m_fullScreenPlayer->updateProgress(pos, dur);
        m_fullScreenPlayer->updateLyricLine(pos);
    }
    if (m_miniPlayer && m_miniPlayer->isVisible())
        m_miniPlayer->setProgress(pos, dur);
}

void MusicPlayerWidget::onDurationChanged(qint64 dur)
{
    Q_UNUSED(dur);
}

void MusicPlayerWidget::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        if (m_playMode == LoopOne) {
            m_player->setPosition(0);
            m_player->play();
        } else if (!m_nextPreCachedPath.isEmpty() && m_nextPreCachedIndex >= 0) {
            // Fast handoff: skip currentSongList() rebuild, go straight to cached next.
            const int idx = m_nextPreCachedIndex;
            m_nextPreCachedPath.clear();
            m_nextPreCachedIndex = -1;
            playSongAt(idx);
        } else {
            playNext();
        }
    } else if (status == QMediaPlayer::LoadedMedia) {
        // Read metadata if not available
        QList<SongInfo> songs = currentSongList();
        if (m_currentIndex >= 0 && m_currentIndex < songs.size()) {
            QString path = songs[m_currentIndex].path;
            for (auto &song : m_allSongs) {
                if (song.path == path) {
                    if (song.title.isEmpty() || song.title == QFileInfo(song.path).baseName()) {
                        QString title = m_player->metaData().value(QMediaMetaData::Title).toString();
                        QString artist = m_player->metaData().value(QMediaMetaData::Author).toString();
                        if (artist.isEmpty()) artist = m_player->metaData().value(QMediaMetaData::ContributingArtist).toString();
                        QString album = m_player->metaData().value(QMediaMetaData::AlbumTitle).toString();
                        if (!title.isEmpty()) song.title = title;
                        if (!artist.isEmpty()) song.artist = artist;
                        if (!album.isEmpty()) song.album = album;
                        if (song.duration == 0) song.duration = m_player->duration();
                        saveLibrary();
                        updateNowPlaying();
                    }
                    break;
                }
            }
        }
    }
}

void MusicPlayerWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    const bool playing = state == QMediaPlayer::PlayingState;
    if (m_playBtn) m_playBtn->setPlaying(playing);
    if (m_fullScreenPlayer && m_fullScreenPlayer->isVisible()) {
        m_fullScreenPlayer->setPlaying(playing);
    }
    if (m_miniPlayer) m_miniPlayer->setPlaying(playing);
}

void MusicPlayerWidget::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);
    qWarning() << "MediaPlayer error:" << errorString;
    m_titleLabel->setText("播放出错");
    m_artistLabel->setText(errorString.left(40));
}

void MusicPlayerWidget::onVolumeChanged(int value)
{
    m_audioOutput->setVolume(value / 100.0);
    if (m_volumeBar) m_volumeBar->setValue(value);
    if (m_muteBtn)
        m_muteBtn->setIconType(value == 0 ? NCIcons::Mute
                            : value < 50 ? NCIcons::VolumeLow
                                         : NCIcons::VolumeHigh);
}

void MusicPlayerWidget::toggleMute()
{
    if (m_audioOutput->volume() > 0) {
        m_savedVolume = qBound(1, int(m_audioOutput->volume() * 100), 100);
        m_audioOutput->setVolume(0);
        m_volumeBar->setValue(0);
        m_muteBtn->setIconType(NCIcons::Mute);
    } else {
        const int v = m_savedVolume > 0 ? m_savedVolume : 70;
        m_audioOutput->setVolume(v / 100.0);
        m_volumeBar->setValue(v);
        m_muteBtn->setIconType(v < 50 ? NCIcons::VolumeLow : NCIcons::VolumeHigh);
    }
}

void MusicPlayerWidget::setInitialPlayMode(const QString &mode)
{
    if (mode == "loopOne") m_playMode = LoopOne;
    else if (mode == "loopAll") m_playMode = LoopAll;
    else if (mode == "shuffle") m_playMode = Shuffle;
    else m_playMode = Sequential;

    NCIcons::IconType t = NCIcons::Repeat;
    QString tip;
    switch (m_playMode) {
    case Sequential: t = NCIcons::Sequential; tip = "顺序播放"; break;
    case LoopOne:    t = NCIcons::RepeatOne;  tip = "单曲循环"; break;
    case LoopAll:    t = NCIcons::Repeat;     tip = "列表循环"; break;
    case Shuffle:    t = NCIcons::Shuffle;    tip = "随机播放"; break;
    }
    if (m_modeBtn) {
        m_modeBtn->setIconType(t);
        m_modeBtn->setToolTip(tip);
    }
    if (m_queuePanel) m_queuePanel->setModeIcon(t);
    if (m_miniPlayer) m_miniPlayer->setPlayMode(m_playMode);
    if (m_fullScreenPlayer) m_fullScreenPlayer->setPlayMode(m_playMode);
}

void MusicPlayerWidget::cyclePlayMode()
{
    m_playMode = (PlayMode)((m_playMode + 1) % 4);
    NCIcons::IconType t = NCIcons::Repeat;
    QString tip;
    switch (m_playMode) {
    case Sequential: t = NCIcons::Sequential; tip = "顺序播放"; break;
    case LoopOne:    t = NCIcons::RepeatOne;  tip = "单曲循环"; break;
    case LoopAll:    t = NCIcons::Repeat;     tip = "列表循环"; break;
    case Shuffle:    t = NCIcons::Shuffle;    tip = "随机播放"; break;
    }
    m_modeBtn->setIconType(t);
    m_modeBtn->setToolTip(tip);
    if (m_queuePanel) m_queuePanel->setModeIcon(t);
    if (m_miniPlayer) m_miniPlayer->setPlayMode(m_playMode);
    if (m_fullScreenPlayer) m_fullScreenPlayer->setPlayMode(m_playMode);
}

void MusicPlayerWidget::volumeUp()
{
    if (m_volumeBar) m_volumeBar->setValue(qMin(100, m_volumeBar->value() + 5));
}

void MusicPlayerWidget::volumeDown()
{
    if (m_volumeBar) m_volumeBar->setValue(qMax(0, m_volumeBar->value() - 5));
}

void MusicPlayerWidget::setVolume(int vol)
{
    if (m_volumeBar) {
        m_volumeBar->blockSignals(true);
        m_volumeBar->setValue(qBound(0, vol, 100));
        m_volumeBar->blockSignals(false);
    }
    m_audioOutput->setVolume(qBound(0, vol, 100) / 100.0);
    if (m_muteBtn)
        m_muteBtn->setIconType(vol == 0 ? NCIcons::Mute
                            : vol < 50 ? NCIcons::VolumeLow
                                       : NCIcons::VolumeHigh);
}

void MusicPlayerWidget::toggleLike()
{
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex < 0 || m_currentIndex >= songs.size()) return;
    QString path = songs[m_currentIndex].path;
    const bool wasFav = m_favorites.contains(path);
    if (wasFav) m_favorites.removeAll(path);
    else m_favorites << path;
    if (m_db) m_db->setFavorite(path, !wasFav);
    saveLibrary();
    updateNowPlaying();
    if (m_currentNav == "favorites") refreshSongTable();
}

void MusicPlayerWidget::showPlaylist()
{
    toggleQueuePanel();
}

void MusicPlayerWidget::refreshQueuePanel()
{
    if (!m_queuePanel || !m_queueBtn) return;
    const QList<SongInfo> songs = currentSongList();
    m_queuePanel->refresh(songs, m_currentIndex);
    m_queueBtn->setCount(songs.size());
    // Keep the panel's mode glyph in sync with the bar.
    NCIcons::IconType t = NCIcons::Repeat;
    switch (m_playMode) {
    case Sequential: t = NCIcons::Sequential; break;
    case LoopOne:    t = NCIcons::RepeatOne;  break;
    case LoopAll:    t = NCIcons::Repeat;     break;
    case Shuffle:    t = NCIcons::Shuffle;    break;
    }
    m_queuePanel->setModeIcon(t);
}

void MusicPlayerWidget::toggleQueuePanel()
{
    if (!m_queuePanel) return;
    if (m_queuePanel->isVisible()) {
        m_queuePanel->hide();
        m_queueBtn->setChecked(false);
        return;
    }
    refreshQueuePanel();
    // Position above the bar at the right edge.
    const int panelW = 440;
    const int panelH = qMin(420, m_playerBar->y() - 8);
    m_queuePanel->setFixedSize(panelW, qMax(160, panelH));
    m_queuePanel->move(qMax(8, width() - panelW - 12),
                       m_playerBar->y() - m_queuePanel->height() - 2);
    m_queuePanel->show();
    m_queuePanel->raise();
    m_queueBtn->setChecked(true);
}

// ========== Lyrics ==========

void MusicPlayerWidget::setupLyricsPanel()
{
    m_lyricsPanel = new QWidget;
    m_lyricsPanel->setFixedWidth(300);
    m_lyricsPanel->setStyleSheet("background: #212225; border-left: 1px solid #17181b;");

    QVBoxLayout *layout = new QVBoxLayout(m_lyricsPanel);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(8);

    QLabel *title = new QLabel("歌词");
    title->setStyleSheet("color: #c20c0c; font-size: 14px; font-weight: bold; padding-bottom: 8px;");
    layout->addWidget(title);

    m_lyricsList = new QListWidget;
    m_lyricsList->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; outline: none; }
        QListWidget::item { color: #6f6f72; padding: 8px 4px; font-size: 13px; text-align: center; border: none; }
        QListWidget::item:selected { background: transparent; color: #c20c0c; font-weight: bold; }
    )");
    m_lyricsList->setSpacing(2);
    m_lyricsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_lyricsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_lyricsList, &QListWidget::itemClicked, this, &MusicPlayerWidget::onLyricsItemClicked);
    layout->addWidget(m_lyricsList, 1);
}

void MusicPlayerWidget::onLyricsItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const int row = m_lyricsList->row(item);
    // Find the timestamp for the clicked lyric row and seek there
    for (const auto &t : m_lyricsTiming) {
        if (t.second == row) {
            m_player->setPosition(t.first);
            m_currentLyricIndex = row;
            return;
        }
    }
    // Row without a timestamp (e.g. "暂无歌词" / "♪"): nothing to seek
}

void MusicPlayerWidget::toggleLyrics()
{
    m_lyricsVisible = !m_lyricsVisible;
    m_lyricsPanel->setVisible(m_lyricsVisible);
    m_lyricsBtn->setChecked(m_lyricsVisible);
}
void MusicPlayerWidget::parseLrc(const QString &lrcPath)
{
    QFile file(lrcPath);
    QString content;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        content = in.readAll();
        file.close();
    }
    parseLrcText(content);
}

void MusicPlayerWidget::parseLrcText(const QString &content)
{
    clearLyrics();

    QRegularExpression timeRe("\\[(\\d{2}):(\\d{2})\\.(\\d{2,3})\\]");
    const QStringList lines = content.split('\n');
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        // Extract all timestamps on this line
        QList<qint64> times;
        QRegularExpressionMatchIterator it = timeRe.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int min = m.captured(1).toInt();
            int sec = m.captured(2).toInt();
            int ms = m.captured(3).toInt();
            if (m.captured(3).length() == 2) ms *= 10;
            times.append((min * 60 + sec) * 1000 + ms);
        }

        // Remove all timestamps to get lyric text
        QString text = line;
        text.remove(timeRe);
        text = text.trimmed();
        if (text.isEmpty() && times.isEmpty()) continue;

        for (qint64 t : times) {
            int row = m_lyricsList->count();
            QListWidgetItem *item = new QListWidgetItem(text.isEmpty() ? "♪" : text, m_lyricsList);
            item->setTextAlignment(Qt::AlignCenter);
            m_lyricsTiming.append(qMakePair(t, row));
        }
    }

    if (m_lyricsList->count() == 0) {
        QListWidgetItem *item = new QListWidgetItem("暂无歌词", m_lyricsList);
        item->setTextAlignment(Qt::AlignCenter);
    }
}

void MusicPlayerWidget::updateLyrics(qint64 pos)
{
    if (m_lyricsTiming.isEmpty()) { m_currentLyricIndex = -1; return; }

    int idx = m_currentLyricIndex;
    if (idx >= 0 && idx < m_lyricsTiming.size() && m_lyricsTiming[idx].first <= pos) {
        // Position advanced: walk forward from the cached index (O(1) per tick)
        while (idx + 1 < m_lyricsTiming.size() && m_lyricsTiming[idx + 1].first <= pos)
            ++idx;
    } else {
        // First call or backward seek: rescan (same as before)
        idx = -1;
        for (int i = m_lyricsTiming.size() - 1; i >= 0; --i) {
            if (m_lyricsTiming[i].first <= pos) { idx = i; break; }
        }
    }
    m_currentLyricIndex = idx;

    if (idx >= 0) {
        int row = m_lyricsTiming[idx].second;
        if (row >= 0 && row < m_lyricsList->count()) {
            m_lyricsList->setCurrentRow(row);
            m_lyricsList->scrollToItem(m_lyricsList->item(row), QAbstractItemView::PositionAtCenter);
        }
    }
}

void MusicPlayerWidget::clearLyrics()
{
    m_lyricsList->clear();
    m_lyricsTiming.clear();
    m_currentLyricIndex = -1;
}

void MusicPlayerWidget::toggleMiniPlayer()
{
    if (!m_miniPlayer) {
        m_miniPlayer = new MiniPlayer;
        connect(m_miniPlayer, &MiniPlayer::playPauseClicked, this, &MusicPlayerWidget::togglePlay);
        connect(m_miniPlayer, &MiniPlayer::prevClicked, this, &MusicPlayerWidget::playPrev);
        connect(m_miniPlayer, &MiniPlayer::nextClicked, this, &MusicPlayerWidget::playNext);
        connect(m_miniPlayer, &MiniPlayer::modeClicked, this, &MusicPlayerWidget::cyclePlayMode);
        connect(m_miniPlayer, &MiniPlayer::playlistClicked, this, &MusicPlayerWidget::showPlaylist);
        connect(m_miniPlayer, &MiniPlayer::seekRequested, this, [this](qint64 ms) {
            if (ms >= 0) m_player->setPosition(ms);
        });
        connect(m_miniPlayer, &MiniPlayer::closed, this, [this]() {
            if (m_miniPlayer) m_miniPlayer->hide();
        });
    }
    if (m_miniPlayer->isVisible()) {
        m_miniPlayer->hide();
        return;
    }
    // Sync current song / state
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex >= 0 && m_currentIndex < songs.size()) {
        const SongInfo &s = songs[m_currentIndex];
        m_miniPlayer->setSong(s.title, s.artist, CoverArt::find(s.path));
    } else {
        m_miniPlayer->setSong("未播放", "");
    }
    m_miniPlayer->setPlaying(m_player->playbackState() == QMediaPlayer::PlayingState);
    m_miniPlayer->setProgress(m_player->position(), m_player->duration());
    // Position near bottom-right of the current screen
    QScreen *scr = QGuiApplication::primaryScreen();
    QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
    m_miniPlayer->move(avail.right() - m_miniPlayer->width() - 40, avail.bottom() - m_miniPlayer->height() - 60);
    m_miniPlayer->show();
    m_miniPlayer->raise();
}

void MusicPlayerWidget::showTechInfo()
{
    QDialog dlg(this);
    dlg.setWindowTitle("音乐播放器 · 技术说明");
    dlg.setModal(true);
    dlg.resize(620, 520);
    dlg.setStyleSheet("QDialog { background: #1b2838; }");

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(22, 20, 22, 16);
    lay->setSpacing(12);

    QLabel *title = new QLabel("音乐播放器 · 技术说明", &dlg);
    title->setStyleSheet("color: #66c0f4; font-size: 17px; font-weight: 700;");
    lay->addWidget(title);

    QTabWidget *tabs = new QTabWidget(&dlg);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #2a475e; border-radius: 6px; background: #16202d; }"
        "QTabBar::tab { background: #16202d; color: #8f98a0; padding: 8px 22px; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #2a475e; color: #66c0f4; }");
    lay->addWidget(tabs, 1);

    // ---- Tab 1: engineering challenges ----
    QTextEdit *challenges = new QTextEdit;
    challenges->setReadOnly(true);
    challenges->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; font-size: 13px; }");
    challenges->setHtml(QStringLiteral(
        "<h3 style='color:#66c0f4; margin:12px 0 4px 0; font-size:14px;'>1. 弱网环境下的流畅播放</h3>"
        "<p style='margin:0 0 8px 0; line-height:1.6;'>在地铁、电梯等网络不稳定场景下，如何实现智能码率自适应、预测性缓冲和无缝降级（如从无损自动切到标准音质），保证“不卡顿”，是播放器的基本功。</p>"
        "<h3 style='color:#66c0f4; margin:12px 0 4px 0; font-size:14px;'>2. 高解析度音频与音效处理</h3>"
        "<p style='margin:0 0 8px 0; line-height:1.6;'>支持 Hi-Res、杜比全景声等高规格格式，同时要在移动端有限的算力下实时渲染“鲸云音效”等 DSP 效果，且不能导致耗电过快或发热，需要在音质、性能与功耗三者间做极致平衡。</p>"
        "<h3 style='color:#66c0f4; margin:12px 0 4px 0; font-size:14px;'>3. 跨平台一致性</h3>"
        "<p style='margin:0 0 8px 0; line-height:1.6;'>网易云覆盖 iOS、Android、Windows、Mac、Linux 及车机。不同系统的音频 API（AudioUnit、OpenSL ES、WASAPI 等）差异巨大，要保证各端延迟、音量响度、淡入淡出效果的听感一致，维护成本极高。</p>"
        "<h3 style='color:#66c0f4; margin:12px 0 4px 0; font-size:14px;'>4. 直播与长音频融合</h3>"
        "<p style='margin:0 0 8px 0; line-height:1.6;'>现在的播放器不仅是放歌，还集成了 Look 直播、播客。将低延迟直播流与本地音乐播放引擎融合，且不互相抢占音频焦点，架构设计非常复杂。</p>"
    ));
    tabs->addTab(challenges, "技术挑战");

    // ---- Tab 2: layered architecture ----
    QTextEdit *arch = new QTextEdit;
    arch->setReadOnly(true);
    QFont mono = arch->font();
    mono.setFamily("Consolas");
    mono.setPointSize(10);
    arch->setFont(mono);
    arch->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    arch->setPlainText(QStringLiteral(
        "┌─────────────────────────────────────────────┐\n"
        "│              UI 层 (播放控制/进度条/封面)      │\n"
        "├─────────────────────────────────────────────┤\n"
        "│           播放控制层 (PlayController)          │\n"
        "│   play / pause / seek / next / prev / shuffle│\n"
        "├─────────────────────────────────────────────┤\n"
        "│           解码层 (Decoder Pipeline)           │\n"
        "│   FFmpeg / MediaCodec / AVFoundation         │\n"
        "├─────────────────────────────────────────────┤\n"
        "│           音频渲染层 (Audio Renderer)         │\n"
        "│   OpenSL ES(Android) / AudioUnit(iOS)        │\n"
        "│   WASAPI(Win) / CoreAudio(Mac)               │\n"
        "├─────────────────────────────────────────────┤\n"
        "│           DSP 效果链 (Effect Chain)           │\n"
        "│   EQ → 音效 → 响度归一化 → 淡入淡出           │\n"
        "└─────────────────────────────────────────────┘\n"
        "\n"
        "说明：自上而下的数据流 —— UI 层的操作经播放控制层下发，\n"
        "解码层解出 PCM 后交给音频渲染层输出，渲染前经过 DSP 效果链。"
    ));
    tabs->addTab(arch, "分层架构");

    // ---- Tab 3: playback state machine ----
    QTextEdit *stateMachine = new QTextEdit;
    stateMachine->setReadOnly(true);
    QFont smFont = stateMachine->font();
    smFont.setFamily("Consolas");
    smFont.setPointSize(10);
    stateMachine->setFont(smFont);
    stateMachine->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    stateMachine->setPlainText(QStringLiteral(
        "        ┌──────────┐\n"
        "        │  IDLE    │\n"
        "        └────┬─────┘\n"
        "             │ load(url)\n"
        "             ▼\n"
        "        ┌──────────┐    error\n"
        "        │ LOADING  │──────────► ERROR\n"
        "        └────┬─────┘\n"
        "             │ bufferReady\n"
        "             ▼\n"
        "        ┌──────────┐  pause()  ┌─────────┐\n"
        "        │ PLAYING  │──────────►│ PAUSED  │\n"
        "        └────┬─────┘  resume() └─────────┘\n"
        "             │\n"
        "             │ trackEnd / next()\n"
        "             ▼\n"
        "        ┌──────────┐\n"
        "        │ SWITCHING│──► (回到 LOADING 加载下一首)\n"
        "        └──────────┘\n"
        "\n"
        "说明：IDLE 空闲 → load(url) 进入 LOADING；缓冲就绪后进入 PLAYING；\n"
        "pause/resume 在 PLAYING 与 PAUSED 间切换；加载失败进入 ERROR；\n"
        "播放结束或切歌经 SWITCHING 回到 LOADING 加载下一首。"
    ));
    tabs->addTab(stateMachine, "状态机");

    // ---- Tab 4: adaptive bitrate pseudo-code ----
    QTextEdit *abr = new QTextEdit;
    abr->setReadOnly(true);
    QFont abrFont = abr->font();
    abrFont.setFamily("Consolas");
    abrFont.setPointSize(10);
    abr->setFont(abrFont);
    abr->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    abr->setPlainText(QStringLiteral(
        "# 伪代码：码率自适应逻辑\n"
        "def on_buffer_health(current_buffer_ms, network_speed_kbps):\n"
        "    if current_buffer_ms < 5000:  # 缓冲不足5秒\n"
        "        if network_speed_kbps < 200:\n"
        "            switch_to(QUALITY_128K)   # 降到标准音质\n"
        "        elif network_speed_kbps < 500:\n"
        "            switch_to(QUALITY_320K)\n"
        "        # 触发紧急预缓冲\n"
        "        prefetch_next_segment(priority=HIGH)\n"
        "    \n"
        "    if current_buffer_ms > 30000:  # 缓冲充足\n"
        "        # 尝试升级音质（用户设置允许时）\n"
        "        try_upgrade_quality()\n"
        "\n"
        "策略要点：\n"
        "· 缓冲 <5s 且网速低 → 主动降级，保住“不卡顿”\n"
        "· 降级同时 HIGH 优先级预取下一段，争取无缝衔接\n"
        "· 缓冲 >30s → 网络条件允许时尝试回升音质\n"
        "· 升级需尊重用户设置（如“省流量模式”）"
    ));
    tabs->addTab(abr, "码率自适应");

    // ---- Tab 5: lyrics system ----
    QTextEdit *lyrics = new QTextEdit;
    lyrics->setReadOnly(true);
    QFont lyricFont = lyrics->font();
    lyricFont.setFamily("Consolas");
    lyricFont.setPointSize(10);
    lyrics->setFont(lyricFont);
    lyrics->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    lyrics->setPlainText(QStringLiteral(
        "歌词系统\n"
        "\n"
        ".1 歌词解析\n"
        "\n"
        "文本\n"
        "[00:12.34] 第一句歌词\n"
        "[00:17.56] 第二句歌词\n"
        "[00:22.78] 第三句歌词\n"
        "\n"
        "解析流程：\n"
        "请求歌词接口 → 拿到 Base64 编码的 LRC 文件\n"
        "解码 → 正则提取 [mm:ss.xx] 时间戳和文本\n"
        "构建有序数组：List<{timestamp: ms, text: string}>\n"
        "支持双行歌词（翻译歌词）和逐字歌词（卡拉OK模式）\n"
        "\n"
        ".2 歌词滚动与高亮\n"
        "\n"
        "┌────────────────────────────┐\n"
        "│   ○ ○ ○ (上方歌词，半透明)  │\n"
        "│   ○ ○ ○                    │\n"
        "│                            │\n"
        "│ ★ 当前句（放大、高亮、白色）★│  ← 居中对齐\n"
        "│                            │\n"
        "│   ○ ○ ○ (下方歌词，半透明)  │\n"
        "│   ○ ○ ○                    │\n"
        "└────────────────────────────┘\n"
        "\n"
        "滚动算法：使用 RecyclerView(Android) / UICollectionView(iOS)，\n"
        "当前句始终锚定在视口中央，offset = currentIndex * lineHeight - viewportHeight/2。\n"
        "\n"
        "逐字高亮（卡拉OK）：每个字有独立的起止时间，通过 ValueAnimator 或\n"
        "CADisplayLink 逐帧刷新每个字的颜色/宽度，实现“填色”效果。\n"
        "\n"
        "性能优化：仅渲染可视区域 ±5 行，其余复用回收；\n"
        "歌词文本预排版缓存，避免每帧重新测量。\n"
        "\n"
        ".3 歌词与音频同步\n"
        "播放引擎通过回调（每 100~200ms 一次）上报当前播放进度 currentTimeMs。\n"
        "歌词模块用二分查找定位当前句索引。\n"
        "支持歌词偏移校正（用户手动调整 ±0.5s），存入本地数据库。"
    ));
    tabs->addTab(lyrics, "歌词系统");

    // ---- Tab 6: comment system ----
    QTextEdit *comments = new QTextEdit;
    comments->setReadOnly(true);
    QFont cmtFont = comments->font();
    cmtFont.setFamily("Consolas");
    cmtFont.setPointSize(10);
    comments->setFont(cmtFont);
    comments->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    comments->setPlainText(QStringLiteral(
        "评论系统\n"
        "\n"
        ".1 数据加载策略\n"
        "\n"
        "请求热门评论 → 按点赞数排序 → 分页展示（每页20条）\n"
        "请求最新评论 → 按时间倒序 → 下拉加载更多\n"
        "\n"
        "首屏秒开：对热门歌曲（评论 > 10万），服务端预生成“精选评论”缓存，\n"
        "客户端优先读取本地缓存 + 后台静默刷新。\n"
        "\n"
        "评论与播放联动：部分歌曲支持“时间轴评论”（在特定时间点弹出），\n"
        "播放器在 onProgressUpdate 中检查是否有匹配的评论触发事件。\n"
        "\n"
        ".2 评论输入与交互\n"
        "输入框浮层不抢占音频焦点\n"
        "（AudioFocusRequest 设置为 AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK）。\n"
        "\n"
        "点赞/回复采用乐观更新：先本地更新 UI → 异步请求服务端 → 失败则回滚。\n"
        "\n"
        "评论中的 @用户、#话题 通过正则解析为富文本 SpannableString。"
    ));
    tabs->addTab(comments, "评论系统");

    // ---- Tab 7: recommendation system ----
    QTextEdit *recommend = new QTextEdit;
    recommend->setReadOnly(true);
    QFont recFont = recommend->font();
    recFont.setFamily("Consolas");
    recFont.setPointSize(10);
    recommend->setFont(recFont);
    recommend->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    recommend->setPlainText(QStringLiteral(
        "推荐系统（播放器端侧部分）\n"
        "\n"
        "4.1 播放队列管理\n"
        "\n"
        "1播放模式：\n"
        "2  ├── 顺序播放：index++\n"
        "3  ├── 随机播放：Fisher-Yates 洗牌（保证不重复）\n"
        "4  ├── 单曲循环：repeat current\n"
        "5  └── 心动模式（网易云特色）：\n"
        "6       基于当前歌曲的 tag/artist/genre 向量，\n"
        "7       从候选池中实时计算相似度，动态插入队列\n"
        "\n"
        "4.2 私人FM / 每日推荐\n"
        "客户端维护一个本地推荐队列（通常预取 3~5 首）。\n"
        "用户“红心❤️”或“垃圾桶🗑️”操作后，立即上报行为日志\n"
        "（含播放时长、是否跳过、跳过时间点）。\n"
        "服务端实时更新用户画像，下一次拉取推荐时生效。\n"
        "\n"
        "端侧兜底：网络异常时，从本地缓存的“相似歌曲表”中\n"
        "按相似度排序填充队列。\n"
        "\n"
        "4.3 行为埋点\n"
        "（待补充）"
    ));
    tabs->addTab(recommend, "推荐系统");

    // ---- Tab 8: interaction & motion ----
    QTextEdit *motion = new QTextEdit;
    motion->setReadOnly(true);
    QFont motFont = motion->font();
    motFont.setFamily("Consolas");
    motFont.setPointSize(10);
    motion->setFont(motFont);
    motion->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    motion->setPlainText(QStringLiteral(
        "交互与动效\n"
        "\n"
        "黑胶旋转\tObjectAnimator / CABasicAnimation，360°/20s 匀速，暂停时缓停\n"
        "封面切换\t双指缩放切换黑胶↔封面模式，ViewPager + 缩放动画\n"
        "进度条拖拽\t拖拽时暂停进度回调，松手后一次性 seek，避免频繁解码\n"
        "评论上滑\tBottomSheetBehavior / 自定义 PanGesture，与播放控制互不干扰\n"
        "播放/暂停\t图标形变动画（AnimatedVectorDrawable / Lottie），200ms\n"
        "歌词点击\t点击某句歌词 → seek 到对应时间点\n"
        "\n"
        ".3 主题与皮肤\n"
        "播放器背景色从封面主色调提取（Android Palette / iOS VibrantEffect）。\n"
        "支持黑胶主题、全屏沉浸式、极简模式，通过 ThemeManager 统一管理资源映射。"
    ));
    tabs->addTab(motion, "交互动效");

    // ---- Tab 9: synchronized playback (listen together) ----
    QTextEdit *sync = new QTextEdit;
    sync->setReadOnly(true);
    QFont syncFont = sync->font();
    syncFont.setFamily("Consolas");
    syncFont.setPointSize(10);
    sync->setFont(syncFont);
    sync->setStyleSheet("QTextEdit { background: #16202d; color: #c7d5e0; border: none; padding: 12px; }");
    sync->setPlainText(QStringLiteral(
        "┌─────────┐    WebSocket     ┌─────────┐\n"
        "│  用户A   │◄──────────────►│  用户B   │\n"
        "│  播放器  │    信令服务器     │  播放器  │\n"
        "└────┬────┘                  └────┬────┘\n"
        "     │                            │\n"
        "     └──────────┬─────────────────┘\n"
        "                ▼\n"
        "         ┌────────────┐\n"
        "         │  同步服务器  │  (房间状态/进度仲裁)\n"
        "         └────────────┘\n"
        "\n"
        "说明：\n"
        "· 用户A/B 通过 WebSocket 连接信令服务器，交换播放控制信令\n"
        "  （播放/暂停/切歌/拖拽），维持两端动作实时一致。\n"
        "· 同步服务器作为房间状态权威，仲裁播放进度：\n"
        "  以房主进度为基准，辅以各端网络延迟(RTT)补偿，\n"
        "  避免漂移（可通过定期对时/时间戳回放校准）。\n"
        "· 音频由各自端侧解码渲染，信令只传控制与状态，不传音频数据。"
    ));
    tabs->addTab(sync, "同步播放");

    QPushButton *ok = new QPushButton("关闭", &dlg);
    ok->setFixedWidth(80);
    ok->setStyleSheet("QPushButton { background: #66c0f4; color: #10202b; border: none; border-radius: 4px; padding: 6px 16px; font-weight: 600; }");
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(ok, 0, Qt::AlignRight);

    dlg.exec();
}


QString MusicPlayerWidget::formatTime(qint64 ms) const
{
    if (ms <= 0) return "00:00";
    qint64 totalSec = ms / 1000;
    qint64 min = totalSec / 60;
    qint64 sec = totalSec % 60;
    return QString("%1:%2").arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

void MusicPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Keep an open queue panel anchored above the bar at the right edge.
    if (m_queuePanel && m_queuePanel->isVisible() && m_playerBar) {
        const int panelW = m_queuePanel->width();
        const int panelH = qMin(420, m_playerBar->y() - 8);
        m_queuePanel->setFixedHeight(qMax(160, panelH));
        m_queuePanel->move(qMax(8, width() - panelW - 12),
                          m_playerBar->y() - m_queuePanel->height() - 2);
        m_queuePanel->raise();
    }
}

bool MusicPlayerWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_coverLabel && event->type() == QEvent::MouseButtonPress) {
        openFullScreenPlayer();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void MusicPlayerWidget::onSearchTextChanged(const QString &text)
{
    m_searchText = text.trimmed();
    // Deferred refresh: coalesce rapid typing, rebuild only after a pause
    if (m_searchDebounceTimer) m_searchDebounceTimer->start();
}


// ==================== MusicPlayerWidget full screen ====================

void MusicPlayerWidget::openFullScreenPlayer()
{
    if (m_currentIndex < 0) {
        QMessageBox::information(this, "提示", "请先播放一首歌曲");
        return;
    }

    if (!m_fullScreenPlayer) {
        m_fullScreenPlayer = new FullScreenPlayer();
        connect(m_fullScreenPlayer, &FullScreenPlayer::playPauseClicked, this, [this]() {
            if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
            else m_player->play();
        });
        connect(m_fullScreenPlayer, &FullScreenPlayer::prevClicked, this, [this]() {
            QList<SongInfo> songs = currentSongList();
            if (songs.isEmpty()) return;
            int idx = m_currentIndex - 1;
            if (idx < 0) idx = songs.size() - 1;
            playSongAt(idx);
        });
        connect(m_fullScreenPlayer, &FullScreenPlayer::nextClicked, this, [this]() {
            QList<SongInfo> songs = currentSongList();
            if (songs.isEmpty()) return;
            int idx = m_currentIndex + 1;
            if (idx >= songs.size()) idx = 0;
            playSongAt(idx);
        });
        connect(m_fullScreenPlayer, &FullScreenPlayer::volumeChanged, this, [this](int v) {
            m_audioOutput->setVolume(v / 100.0);
        });
        connect(m_fullScreenPlayer, &FullScreenPlayer::modeChanged, this, &MusicPlayerWidget::cyclePlayMode);
        connect(m_fullScreenPlayer, &FullScreenPlayer::seekRequested, this, [this](qint64 ms) {
            if (ms >= 0) m_player->setPosition(ms);
        });
        connect(m_fullScreenPlayer, &FullScreenPlayer::closed, this, [this]() {
            if (m_fullScreenPlayer) { m_fullScreenPlayer->close(); m_fullScreenPlayer->deleteLater(); m_fullScreenPlayer = nullptr; }
        });
    }

    // Sync current song info
    QList<SongInfo> songs = currentSongList();
    if (m_currentIndex >= 0 && m_currentIndex < songs.size()) {
        const SongInfo &song = songs[m_currentIndex];
        // Cover art: same-name image next to the file, else folder art (shared
        // helper, so the bar and the full-screen player agree)
        m_fullScreenPlayer->updateSong(song.title, song.artist, CoverArt::find(song.path));
    }
    m_fullScreenPlayer->setPlaying(m_player->playbackState() == QMediaPlayer::PlayingState);
    m_fullScreenPlayer->updateProgress(m_player->position(), m_player->duration());
    // Extract lyrics from list
    QVector<QPair<qint64, QString>> lyricsData;
    if (m_lyricsList) {
        for (int i = 0; i < m_lyricsList->count(); ++i) {
            qint64 time = 0;
            for (const auto &t : m_lyricsTiming) if (t.second == i) { time = t.first; break; }
            lyricsData.append(qMakePair(time, m_lyricsList->item(i)->text()));
        }
    }
    m_fullScreenPlayer->setLyrics(lyricsData);
    m_fullScreenPlayer->setPlayMode(m_playMode);
    // Fully cover the main window (frame geometry includes the title bar,
    // so the fullscreen player exactly overlays the whole main page)
    QWidget *mainWin = window();
    QRect winGeo = mainWin ? mainWin->frameGeometry() : QRect(100, 100, 1280, 800);
    m_fullScreenPlayer->setFixedSize(winGeo.size());
    m_fullScreenPlayer->move(winGeo.topLeft());
    m_fullScreenPlayer->show();
    m_fullScreenPlayer->raise();
    m_fullScreenPlayer->activateWindow();
}
