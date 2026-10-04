#include "maengine.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <QDebug>
#include <QFileInfo>

MaEngine::MaEngine(QObject *parent)
    : QObject(parent)
{
}

MaEngine::~MaEngine()
{
    stop();
}

void MaEngine::advance()
{
    if (m_decoder) {
        ma_decoder_uninit(m_decoder);
        delete m_decoder;
        m_decoder = nullptr;
    }
    m_positionFrames = 0;
    if (m_queueIdx + 1 >= m_queue.size()) {
        emit queueFinished();
        return;
    }
    m_queueIdx++;
    const QString path = m_queue.at(m_queueIdx).toLocalFile();
    m_decoder = new ma_decoder;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, m_channels > 0 ? m_channels : 2, m_sampleRate > 0 ? m_sampleRate : 44100);
    if (ma_decoder_init_file(path.toUtf8().constData(), &cfg, m_decoder) != MA_SUCCESS) {
        qWarning() << "ma_decoder_initFile failed:" << path;
        delete m_decoder; m_decoder = nullptr;
        advance();
        return;
    }
    ma_decoder_get_length_in_pcm_frames(m_decoder, (ma_uint64 *)&m_durationFrames);
    emit trackStarted(m_queue.at(m_queueIdx));
}

void MaEngine::dataCallback(ma_device *device, void *output, const void *input, unsigned frameCount)
{
    Q_UNUSED(input);
    MaEngine *self = static_cast<MaEngine *>(device->pUserData);
    if (!self || !self->m_decoder || self->m_paused) {
        memset(output, 0, frameCount * self->m_channels * sizeof(float));
        return;
    }
    float *out = static_cast<float *>(output);
    unsigned remaining = frameCount;
    float *cursor = out;
    while (remaining > 0) {
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(self->m_decoder, cursor, remaining, &framesRead);
        if (framesRead == 0) {
            // EOF on current track; advance to next (gapless).
            QMetaObject::invokeMethod(self, [self]() { self->advance(); }, Qt::QueuedConnection);
            break;
        }
        // Apply volume.
        for (ma_uint64 i = 0; i < framesRead * self->m_channels; ++i)
            cursor[i] *= self->m_volume;
        cursor += framesRead * self->m_channels;
        self->m_positionFrames += framesRead;
        remaining -= (unsigned)framesRead;
    }
    // Zero-fill any remainder.
    if (remaining > 0)
        memset(cursor, 0, remaining * self->m_channels * sizeof(float));
}

void MaEngine::play(const QList<QUrl> &urls)
{
    stop();
    m_queue = urls;
    m_queueIdx = -1;

    // Start with default format; we'll re-init decoder after device starts.
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = 44100;
    cfg.dataCallback = &MaEngine::dataCallback;
    cfg.pUserData = this;

    m_device = new ma_device;
    if (ma_device_init(nullptr, &cfg, m_device) != MA_SUCCESS) {
        qWarning() << "ma_device_init failed";
        delete m_device; m_device = nullptr;
        return;
    }
    m_channels = (int)m_device->playback.channels;
    m_sampleRate = (int)m_device->sampleRate;

    advance();
    if (!m_decoder) return;

    if (ma_device_start(m_device) != MA_SUCCESS) {
        qWarning() << "ma_device_start failed";
        return;
    }
    m_started = true;
}

void MaEngine::stop()
{
    if (m_device) {
        ma_device_stop(m_device);
        ma_device_uninit(m_device);
        delete m_device;
        m_device = nullptr;
    }
    if (m_decoder) {
        ma_decoder_uninit(m_decoder);
        delete m_decoder;
        m_decoder = nullptr;
    }
    m_queue.clear();
    m_queueIdx = -1;
    m_started = false;
    m_positionFrames = 0;
}

void MaEngine::setPaused(bool paused)
{
    m_paused = paused;
}

void MaEngine::setVolume(float v)
{
    m_volume = v;
}

bool MaEngine::isPlaying() const
{
    return m_started && !m_paused && m_decoder;
}

void MaEngine::seekTo(qint64 ms)
{
    if (!m_decoder || m_sampleRate <= 0) return;
    const ma_uint64 frame = (ma_uint64)(ms * m_sampleRate / 1000);
    ma_decoder_seek_to_pcm_frame(m_decoder, frame);
    m_positionFrames = frame;
}

qint64 MaEngine::positionMs() const
{
    return m_sampleRate > 0 ? m_positionFrames * 1000 / m_sampleRate : 0;
}

qint64 MaEngine::durationMs() const
{
    return m_sampleRate > 0 ? m_durationFrames * 1000 / m_sampleRate : 0;
}
