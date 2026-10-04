#include "gaplessengine.h"

#include <QAudioSink>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>

GaplessEngine::GaplessEngine(QObject *parent)
    : QObject(parent)
{
    m_decoder = new QAudioDecoder(this);
    connect(m_decoder, &QAudioDecoder::bufferReady, this, &GaplessEngine::onBufferReady);
    connect(m_decoder, &QAudioDecoder::finished, this, &GaplessEngine::onDecoderFinished);
    connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
            this, [this](QAudioDecoder::Error){ onDecoderError(m_decoder->errorString()); });
    connect(m_decoder, &QAudioDecoder::formatChanged, this, &GaplessEngine::onFormatChanged);
}

GaplessEngine::~GaplessEngine()
{
    stop();
}

void GaplessEngine::ensureSink(const QAudioFormat &fmt)
{
    if (m_sink && m_format == fmt) return;
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
        m_out = nullptr;
    }
    m_format = fmt;
    m_sink = new QAudioSink(fmt, this);
    m_sink->setBufferSize(64 * 1024);  // 64ms-ish at 44.1k stereo s16
    m_out = m_sink->start();
}

void GaplessEngine::play(const QList<QUrl> &urls)
{
    stop();
    m_queue = urls;
    m_queueIdx = -1;
    startNext();
}

void GaplessEngine::startNext()
{
    if (m_queueIdx + 1 >= m_queue.size()) {
        emit queueFinished();
        return;
    }
    m_queueIdx++;
    m_writtenFramesThisTrack = 0;
    m_totalFramesThisTrack = 0;
    m_eof = false;
    m_currentTrackStartMs = 0;
    const QUrl url = m_queue.at(m_queueIdx);
    emit trackStarted(url);
    m_decoder->setSource(url);
}

void GaplessEngine::onFormatChanged(const QAudioFormat &fmt)
{
    ensureSink(fmt);
}

void GaplessEngine::onBufferReady()
{
    if (!m_decoder) return;
    while (m_decoder->bufferAvailable()) {
        QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) continue;
        if (!m_sink) ensureSink(buf.format());
        if (!m_out) continue;
        const qint64 bytes = buf.byteCount();
        const char *data = buf.constData<char>();
        qint64 written = 0;
        while (written < bytes) {
            const qint64 w = m_out->write(data + written, bytes - written);
            if (w <= 0) break;
            written += w;
        }
        m_writtenFramesThisTrack += buf.frameCount();
        m_totalFramesThisTrack += buf.frameCount();
        emit positionChanged(positionMs());
    }
}

void GaplessEngine::onDecoderFinished()
{
    // Let the sink drain, then start next track immediately.
    m_eof = true;
    // Give the sink ~100ms to flush remaining buffered audio, then advance.
    QTimer::singleShot(80, this, [this]() {
        startNext();
    });
}

void GaplessEngine::onDecoderError(const QString &msg)
{
    qWarning() << "GaplessEngine decode error:" << msg;
    // Skip bad track, continue queue.
    startNext();
}

void GaplessEngine::stop()
{
    m_queue.clear();
    m_queueIdx = -1;
    m_decoder->stop();
    if (m_sink) {
        m_sink->stop();
    }
}

void GaplessEngine::setPaused(bool paused)
{
    if (!m_sink) return;
    if (paused) m_sink->suspend();
    else m_sink->resume();
}

void GaplessEngine::setVolume(float v)
{
    if (m_sink) m_sink->setVolume(v);
}

bool GaplessEngine::isPlaying() const
{
    return m_sink && m_sink->state() == QAudio::ActiveState;
}

qint64 GaplessEngine::positionMs() const
{
    if (!m_format.isValid() || m_writtenFramesThisTrack == 0) return 0;
    const qint64 sr = m_format.sampleRate();
    if (sr <= 0) return 0;
    // Frames written minus frames still sitting in the sink buffer.
    qint64 sinkUs = 0;
    if (m_sink) sinkUs = m_sink->processedUSecs();
    // Approximate: frames written at format sample rate.
    return m_writtenFramesThisTrack * 1000 / sr;
}

qint64 GaplessEngine::currentTrackDurationMs() const
{
    if (!m_format.isValid()) return 0;
    return m_totalFramesThisTrack * 1000 / m_format.sampleRate();
}
