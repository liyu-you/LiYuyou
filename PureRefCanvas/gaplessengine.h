#ifndef GAPLESSENGINE_H
#define GAPLESSENGINE_H

// Minimal gapless playback engine on top of QtMultimedia:
//   QAudioDecoder -> PCM chunks -> QAudioSink (continuous output).
//
// When the current track finishes decoding, the next queued track starts
// immediately with no silence, because QAudioSink keeps running and we keep
// writing into its output device.
//
// This is intentionally much simpler than mpz's full engine (no LRU eviction,
// no prebuffer policy) but delivers the core gapless behavior.

#include <QObject>
#include <QUrl>
#include <QList>
#include <QAudioFormat>

class QAudioSink;
class QAudioDecoder;
class QIODevice;

class GaplessEngine : public QObject
{
    Q_OBJECT
public:
    explicit GaplessEngine(QObject *parent = nullptr);
    ~GaplessEngine() override;

    // Start playing the given queue. Cancels anything currently playing.
    void play(const QList<QUrl> &urls);
    void stop();
    void setPaused(bool paused);
    void setVolume(float v);
    bool isPlaying() const;
    void seekTo(qint64 ms);  // seek within current track
    qint64 positionMs() const;
    qint64 currentTrackDurationMs() const;

signals:
    // Emitted when a new track starts (first PCM chunk decoded).
    void trackStarted(const QUrl &url);
    // Emitted when all queued tracks have finished.
    void queueFinished();
    void positionChanged(qint64 ms);

private slots:
    void onBufferReady();
    void onDecoderFinished();
    void onDecoderError(const QString &msg);
    void onFormatChanged(const QAudioFormat &fmt);

private:
    void startNext();
    void ensureSink(const QAudioFormat &fmt);

    QAudioSink *m_sink = nullptr;
    QIODevice *m_out = nullptr;
    QAudioDecoder *m_decoder = nullptr;
    QAudioFormat m_format;

    QList<QUrl> m_queue;
    int m_queueIdx = -1;

    qint64 m_currentTrackStartMs = 0;
    qint64 m_writtenFramesThisTrack = 0;
    qint64 m_totalFramesThisTrack = 0;
    qint64 m_seekTargetFrames = 0;  // >0: discard this many frames before writing
    bool m_eof = false;
};

#endif // GAPLESSENGINE_H
