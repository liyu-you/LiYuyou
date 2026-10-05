#ifndef LYRICSPANEL_H
#define LYRICSPANEL_H

#include <QWidget>
#include <QList>
#include <QPair>

class QListWidget;
class QListWidgetItem;

/**
 * @brief 歌词面板 — 负责 LRC 解析、显示、同步高亮。
 *
 * 从 MusicPlayerWidget 抽出，独立封装 UI 和解析逻辑。
 * 点击歌词行时发射 lyricClicked(qint64 pos) 信号，由外层连接到播放器 seek。
 */
class LyricsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LyricsPanel(QWidget *parent = nullptr);

    /// 从 .lrc 文件加载歌词
    void parseLrc(const QString &lrcPath);
    /// 从文本内容解析歌词
    void parseLrcText(const QString &content);
    /// 根据当前播放位置更新高亮（每帧调用）
    void update(qint64 pos);
    /// 清空所有歌词
    void clear();

    /// 导出所有歌词行（时间戳 + 文本），给全屏播放器用
    QVector<QPair<qint64, QString>> lyricsData() const;

signals:
    /// 用户点击了某行歌词，外层应 seek 到该时间点
    void lyricClicked(qint64 pos);

private:
    void setupUI();
    void onItemClicked(QListWidgetItem *item);

    QListWidget *m_list = nullptr;
    QList<QPair<qint64, int>> m_timing; // time ms -> row index
    int m_currentIndex = -1;
};

#endif // LYRICSPANEL_H
