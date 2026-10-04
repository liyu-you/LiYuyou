#ifndef MUSICPLAYERWIDGET_H
#define MUSICPLAYERWIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QPushButton>
#include "playpausebutton.h"
#include "playerwidgets.h"
#include "miniplayer.h"
#include "fullscreenplayer.h"
class QAudioOutput;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;
class QTableWidgetItem;
class QSplitter;
class QTextEdit;
class QTimer;
class QFileSystemWatcher;
class MusicLibraryDb;

#include "songinfo.h"

class MusicPlayerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MusicPlayerWidget(const QString &libraryRoot, QWidget *parent = nullptr);
    ~MusicPlayerWidget();
    void hideSidebar();
    void setBackgroundImage(const QString &path);
    void setPanelsTransparent(bool transparent);
    // Apply the persisted default play mode ("sequential"/"loopOne"/"loopAll"/"shuffle").
    void setInitialPlayMode(const QString &mode);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void togglePlay();
    void playPrev();
    void playNext();
    void volumeUp();
    void volumeDown();
    void setVolume(int vol);

private slots:
    // Navigation
    void onNavChanged(QListWidgetItem *item);
    void addPlaylist();
    void deletePlaylist();

    // Song list
    void onSongDoubleClicked(int row, int col);
    void playAll();
    void importMusic();
    void importFolder();
    void importPlaylist();
    void onPlaylistFetched();
    void openFullScreenPlayer();
    void onSearchTextChanged(const QString &text);
    void onSongMenu(const QPoint &pos);

    // Player controls
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onVolumeChanged(int value);
    void toggleMute();
    void cyclePlayMode();
    void toggleLike();
    void showPlaylist();
    void toggleQueuePanel();
    void refreshQueuePanel();
    void toggleLyrics();
    void updateLyrics(qint64 pos);
    void onLyricsItemClicked(QListWidgetItem *item);
    void toggleMiniPlayer();

private:
    void setupUI();
    void setupTopBar();
    void setupDiscoveryPage();
    void setupSidebar();
    void setupNowPlayingCard();
    void setupSongTable();
    void setupPlayerBar();
    void setupLyricsPanel();
    // Light card-style music workspace (mirrors the reader shelf, own code)
    void setupMusicShelfPage();
    void refreshMusicShelf();
    void loadLibrary();
    void saveLibrary();
    void refreshSongTable();
    void playSong(const QString &path);
    void playSongAt(int index);
    void updateNowPlaying();
    void updatePlayingRow();
    // Cover art for the player bar: real image when the song has one, otherwise a
    // per-song tile so tracks remain distinguishable.
    void updateCoverArt(const SongInfo &song);
    // Row of the currently playing song in the (possibly filtered) table
    int rowOfCurrentSong() const;
    QString formatTime(qint64 ms) const;
    void setupMusicWatcher();
    void onMusicDirChanged(const QString &path);
    void scanMusicFolder();
    QList<SongInfo> currentSongList() const;
    void parseLrc(const QString &lrcPath);
    void parseLrcText(const QString &content);
    void clearLyrics();
    void showTechInfo();
    void playOnlineSong(const SongInfo &song);
    void resolveStreamUrl(const QString &url, int hops);
    void startStreaming(const QUrl &url);
    void fetchOnlineLyrics(qint64 neteaseId);
    void onlinePlayFailed(const QString &reason);

    enum NavSection { LocalMusic, Favorites, PlaylistStart };
    enum PlayMode { Sequential, LoopOne, LoopAll, Shuffle };

    QString m_libraryRoot;
    QList<SongInfo> m_allSongs;
    MusicLibraryDb *m_db = nullptr;
    QFileSystemWatcher *m_musicWatcher = nullptr;
    QTimer *m_musicScanTimer = nullptr;
    QList<PlaylistInfo> m_playlists;
    QStringList m_favorites;
    QString m_currentNav; // "local", "favorites", or playlist name
    int m_currentIndex = -1;
    PlayMode m_playMode = LoopAll;
    int m_savedVolume = 70;

    // Media
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QTimer *m_searchDebounceTimer;
    class QNetworkAccessManager *m_netManager;
    class QNetworkReply *m_pendingReply;
    FullScreenPlayer *m_fullScreenPlayer;
    MiniPlayer *m_miniPlayer = nullptr;

    // UI
    QSplitter *m_splitter;
    QWidget *m_sidebar;
    QListWidget *m_navList;
    QPushButton *m_addPlaylistBtn;

    // Top bar
    QWidget *m_topBar;
    class QLineEdit *m_topSearch;

    // Content stack
    class QStackedWidget *m_contentStack;
    QWidget *m_discoveryPage;
    class QGridLayout *m_playlistGrid;

    // Light card-style music shelf page
    QWidget *m_musicShelfPage = nullptr;
    QListWidget *m_songCardList = nullptr;
    QPushButton *m_musicImportBtn = nullptr;
    QLabel *m_shelfSongCount = nullptr;     // header "N 首歌曲"
    QLabel *m_sidebarSongCount = nullptr;   // sidebar stat card
    QLabel *m_sidebarLikedCount = nullptr;  // sidebar stat card (我的喜欢)
    QLabel *m_sidebarRecentTitle = nullptr;
    QLabel *m_sidebarRecentTime = nullptr;
    QLabel *m_favCardCount = nullptr;       // bottom "我的喜欢" card
    QLabel *m_recentSongLabel = nullptr;    // bottom "最近播放" card

    QWidget *m_mainArea;
    QLabel *m_playlistTitle;
    QLabel *m_songCountLabel;
    QPushButton *m_playAllBtn;
    QPushButton *m_importBtn;
    QString m_searchText;
    QTableWidget *m_songTable;
    QLabel *m_emptyLabel;
    // Song path -> table row, so the playing row can be found without scanning
    int m_highlightedRow = -1; // row currently painted as "playing"
    QHash<QString, int> m_rowByPath;
    QString m_coverArtPath;    // cover the bar's pixmap was built from
    QPixmap m_coverArtPix;     // scaled cover for the bar (decoded once per cover)

    QWidget *m_playerBar;
    QWidget *m_nowCard;
    QueuePanel *m_queuePanel;
    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    QLabel *m_artistLabel;
    QPushButton *m_likeBtn;
    CircleTransportButton *m_prevBtn;
    PlayPauseButton *m_playBtn;
    CircleTransportButton *m_nextBtn;
    ThinSlider *m_seekBar;
    QLabel *m_curTimeLabel;
    QLabel *m_totalTimeLabel;
    NCButton *m_modeBtn;
    NCButton *m_muteBtn;
    ThinSlider *m_volumeBar;
    QueueButton *m_queueBtn;
    LyricsToggleButton *m_lyricsBtn;

    // Lyrics
    QWidget *m_lyricsPanel = nullptr;
    QListWidget *m_lyricsList = nullptr;
    QList<QPair<qint64, int>> m_lyricsTiming; // time -> row index
    bool m_lyricsVisible = false;
    int m_currentLyricIndex = -1; // cached index into m_lyricsTiming

    // Panel transparency (透出背景图)
    QString m_bgPath;
    bool m_panelsTransparent = true;
};

#endif
