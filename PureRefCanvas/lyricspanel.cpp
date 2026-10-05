#include "lyricspanel.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QAbstractItemView>

LyricsPanel::LyricsPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void LyricsPanel::setupUI()
{
    setFixedWidth(300);
    setStyleSheet("background: #212225; border-left: 1px solid #17181b;");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(8);

    QLabel *title = new QLabel("歌词");
    title->setStyleSheet("color: #c20c0c; font-size: 14px; font-weight: bold; padding-bottom: 8px;");
    layout->addWidget(title);

    m_list = new QListWidget;
    m_list->setStyleSheet(R"(
        QListWidget { background: transparent; border: none; outline: none; }
        QListWidget::item { color: #6f6f72; padding: 8px 4px; font-size: 13px; text-align: center; border: none; }
        QListWidget::item:selected { background: transparent; color: #c20c0c; font-weight: bold; }
    )");
    m_list->setSpacing(2);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_list, &QListWidget::itemClicked, this, &LyricsPanel::onItemClicked);
    layout->addWidget(m_list, 1);
}

void LyricsPanel::parseLrc(const QString &lrcPath)
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

void LyricsPanel::parseLrcText(const QString &content)
{
    clear();

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
            int row = m_list->count();
            QListWidgetItem *item = new QListWidgetItem(text.isEmpty() ? "♪" : text, m_list);
            item->setTextAlignment(Qt::AlignCenter);
            m_timing.append(qMakePair(t, row));
        }
    }

    if (m_list->count() == 0) {
        QListWidgetItem *item = new QListWidgetItem("暂无歌词", m_list);
        item->setTextAlignment(Qt::AlignCenter);
    }
}

void LyricsPanel::update(qint64 pos)
{
    if (m_timing.isEmpty()) { m_currentIndex = -1; return; }

    int idx = m_currentIndex;
    if (idx >= 0 && idx < m_timing.size() && m_timing[idx].first <= pos) {
        // Position advanced: walk forward from the cached index (O(1) per tick)
        while (idx + 1 < m_timing.size() && m_timing[idx + 1].first <= pos)
            ++idx;
    } else {
        // First call or backward seek: rescan
        idx = -1;
        for (int i = m_timing.size() - 1; i >= 0; --i) {
            if (m_timing[i].first <= pos) { idx = i; break; }
        }
    }
    m_currentIndex = idx;

    if (idx >= 0) {
        int row = m_timing[idx].second;
        if (row >= 0 && row < m_list->count()) {
            m_list->setCurrentRow(row);
            m_list->scrollToItem(m_list->item(row), QAbstractItemView::PositionAtCenter);
        }
    }
}

void LyricsPanel::clear()
{
    m_list->clear();
    m_timing.clear();
    m_currentIndex = -1;
}

QVector<QPair<qint64, QString>> LyricsPanel::lyricsData() const
{
    QVector<QPair<qint64, QString>> result;
    if (!m_list) return result;
    for (int i = 0; i < m_list->count(); ++i) {
        qint64 time = 0;
        for (const auto &t : m_timing) if (t.second == i) { time = t.first; break; }
        result.append(qMakePair(time, m_list->item(i)->text()));
    }
    return result;
}

void LyricsPanel::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;
    const int row = m_list->row(item);
    for (const auto &t : m_timing) {
        if (t.second == row) {
            emit lyricClicked(t.first);
            m_currentIndex = row;
            return;
        }
    }
    // Row without a timestamp: nothing to seek
}
