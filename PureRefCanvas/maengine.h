#ifndef MAENGINE_H
#define MAENGINE_H

// miniaudio-based gapless audio engine. Single-header lib (miniaudio.h) links
// directly to WASAPI on Windows; no Qt Multimedia backend involved.
//
// Gapless: when the current decoder reaches EOF, the data callback immediately
// starts pulling from the next decoder in the queue with no silence.

#include <QObject>
#include <QUrl>
#include <QList>
#include <QString>

struct ma_decoder;
struct ma_device;

class MaEngine : public QObject
{
    Q_OBJECT
public:
    explicit MaEngine(QObject *parent = nullptr);
    ~MaEngine() override;

    void play(const QList<QUrl> &urls);
    void stop();
    void setPaused(bool paused);
    void setVolume(float v);  // 0..1
    bool isPlaying() const;
    void seekTo(qint64 ms);
    qint64 positionMs() const;
    qint64 durationMs() const;

signals:
    void trackStarted(const QUrl &url);
    void positionChanged(qint64 ms);
    void queueFinished();

private:
    void advance();
    static void dataCallback(ma_device *device, void *output, const void *input, unsigned frameCount);

    ma_decoder *m_decoder = nullptr;
    ma_device *m_device = nullptr;
    QList<QUrl> m_queue;
    int m_queueIdx = -1;
    float m_volume = 0.8f;
    bool m_paused = false;
    qint64 m_positionFrames = 0;
    qint64 m_durationFrames = 0;
    int m_sampleRate = 0;
    int m_channels = 0;
    bool m_started = false;
};

#endif // MAENGINE_H
