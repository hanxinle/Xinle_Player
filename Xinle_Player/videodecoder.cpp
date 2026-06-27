#include "videodecoder.h"

#include <QDebug>
#include <QMediaDevices>
#include <climits>

VideoDecoder::VideoDecoder(QObject *parent) : QThread(parent) {}

VideoDecoder::~VideoDecoder() {
    close();
}

bool VideoDecoder::open(const QString &filePath) {
    close();

    m_filePath = filePath;

    // 1. 打开输入文件。
    if (avformat_open_input(&m_fmtCtx, filePath.toUtf8().constData(), nullptr,
                            nullptr) < 0) {
        emit decodeError(QStringLiteral("无法打开视频文件: %1").arg(filePath));
        return false;
    }

    // 2. 获取流信息。
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        emit decodeError(QStringLiteral("无法获取视频流信息。"));
        return false;
    }

    // 3. 找到最佳视频流。
    m_videoStreamIndex =
        av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        emit decodeError(QStringLiteral("未找到视频流。"));
        return false;
    }

    AVStream *videoStream = m_fmtCtx->streams[m_videoStreamIndex];

    // 4. 查找视频解码器。
    m_videoCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!m_videoCodec) {
        emit decodeError(QStringLiteral("找不到对应的视频解码器。"));
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);
    if (avcodec_parameters_to_context(m_videoCodecCtx, videoStream->codecpar) <
        0) {
        emit decodeError(
            QStringLiteral("无法将视频流参数复制到解码器上下文。"));
        return false;
    }

    if (avcodec_open2(m_videoCodecCtx, m_videoCodec, nullptr) < 0) {
        emit decodeError(QStringLiteral("无法打开视频解码器。"));
        return false;
    }

    m_width = m_videoCodecCtx->width;
    m_height = m_videoCodecCtx->height;
    m_duration = m_fmtCtx->duration > 0
                     ? static_cast<double>(m_fmtCtx->duration) / AV_TIME_BASE
                     : 0.0;
    m_frameRate = av_q2d(videoStream->avg_frame_rate);
    if (m_frameRate <= 0.0) {
        m_frameRate = av_q2d(videoStream->r_frame_rate);
    }

    // 5. 分配视频帧和转换上下文。
    m_videoFrame = av_frame_alloc();
    m_rgbaFrame = av_frame_alloc();

    int bufferSize =
        av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 1);
    m_rgbaBuffer = static_cast<uint8_t *>(av_malloc(bufferSize));
    av_image_fill_arrays(m_rgbaFrame->data, m_rgbaFrame->linesize, m_rgbaBuffer,
                         AV_PIX_FMT_RGBA, m_width, m_height, 1);

    m_swsCtx = sws_getContext(m_width, m_height, m_videoCodecCtx->pix_fmt,
                              m_width, m_height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                              nullptr, nullptr, nullptr);

    // 6. 找到最佳音频流（可选）。
    m_audioStreamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1,
                                             m_videoStreamIndex, nullptr, 0);
    if (m_audioStreamIndex >= 0) {
        AVStream *audioStream = m_fmtCtx->streams[m_audioStreamIndex];

        m_audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
        if (m_audioCodec) {
            m_audioCodecCtx = avcodec_alloc_context3(m_audioCodec);
            if (avcodec_parameters_to_context(m_audioCodecCtx,
                                              audioStream->codecpar) >= 0) {
                if (avcodec_open2(m_audioCodecCtx, m_audioCodec, nullptr) >=
                    0) {
                    m_audioFrame = av_frame_alloc();

                    // 设置重采样输出格式：S16 立体声 48kHz。
                    AVChannelLayout outChLayout;
                    av_channel_layout_default(&outChLayout, m_audioChannels);

                    int ret = swr_alloc_set_opts2(
                        &m_swrCtx, &outChLayout, AV_SAMPLE_FMT_S16,
                        m_audioSampleRate, &m_audioCodecCtx->ch_layout,
                        m_audioCodecCtx->sample_fmt,
                        m_audioCodecCtx->sample_rate, 0, nullptr);
                    if (ret >= 0 && m_swrCtx) {
                        swr_init(m_swrCtx);
                    }

                    av_channel_layout_uninit(&outChLayout);
                }
            }
        }
    }

    initAudioOutput();

    qDebug() << "[VideoDecoder] opened:" << m_width << "x" << m_height
             << "fps:" << m_frameRate << "duration:" << m_duration
             << "audioStream:" << m_audioStreamIndex;
    return true;
}

void VideoDecoder::close() {
    requestInterruption();
    {
        QMutexLocker lock(&m_stateMutex);
        m_playing = false;
        m_running = false;
    }
    wait(2000);

    stopAudioOutput();

    {
        QMutexLocker lock(&m_frameQueueMutex);
        m_frameQueue.clear();
    }

    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    if (m_audioCodecCtx) {
        avcodec_free_context(&m_audioCodecCtx);
    }

    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_rgbaBuffer) {
        av_free(m_rgbaBuffer);
        m_rgbaBuffer = nullptr;
    }
    if (m_rgbaFrame) {
        av_frame_free(&m_rgbaFrame);
    }
    if (m_videoFrame) {
        av_frame_free(&m_videoFrame);
    }
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
    }
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_width = 0;
    m_height = 0;
    m_duration = 0.0;
    m_frameRate = 0.0;
    m_videoCodec = nullptr;
    m_audioCodec = nullptr;
}

void VideoDecoder::play() {
    QMutexLocker lock(&m_stateMutex);
    m_playing = true;
    if (!m_running) {
        m_running = true;
        start();
    }
    if (m_audioSink && m_audioIODevice) {
        m_audioSink->resume();
    }
}

void VideoDecoder::pause() {
    QMutexLocker lock(&m_stateMutex);
    m_playing = false;
    if (m_audioSink && m_audioIODevice) {
        m_audioSink->suspend();
    }
}

void VideoDecoder::seek(double seconds) {
    QMutexLocker lock(&m_stateMutex);
    m_seekRequested = true;
    m_seekTarget = qMax(0.0, seconds);
}

bool VideoDecoder::isPlaying() const {
    QMutexLocker lock(&m_stateMutex);
    return m_playing;
}

bool VideoDecoder::getVideoFrame(QByteArray &rgbaData, int &width,
                                 int &height) {
    QMutexLocker lock(&m_frameQueueMutex);
    if (m_frameQueue.isEmpty()) {
        return false;
    }
    VideoFrame frame = m_frameQueue.dequeue();
    rgbaData = frame.rgbaData;
    width = frame.width;
    height = frame.height;
    return true;
}

int VideoDecoder::frameQueueSize() const {
    QMutexLocker lock(&m_frameQueueMutex);
    return m_frameQueue.size();
}

void VideoDecoder::run() {
    AVPacket packet;
    while (!isInterruptionRequested()) {
        {
            QMutexLocker lock(&m_stateMutex);
            if (!m_running) {
                break;
            }
            if (!m_playing) {
                lock.unlock();
                msleep(10);
                continue;
            }
            if (m_seekRequested) {
                m_seekRequested = false;
                double target = m_seekTarget;
                lock.unlock();

                int64_t ts = static_cast<int64_t>(target * AV_TIME_BASE);
                avformat_seek_file(m_fmtCtx, -1, INT64_MIN, ts, INT64_MAX, 0);
                {
                    QMutexLocker fqLock(&m_frameQueueMutex);
                    m_frameQueue.clear();
                }
                flushVideoDecoder();
                flushAudioDecoder();
                if (m_audioSink && m_audioIODevice) {
                    m_audioSink->reset();
                }
                continue;
            }
        }

        // 音频始终解码；视频队列满时丢弃当前视频包并稍等，避免音频被阻塞。
        int ret = av_read_frame(m_fmtCtx, &packet);
        if (ret < 0) {
            // 文件结束，回到开头循环播放。
            avformat_seek_file(m_fmtCtx, -1, INT64_MIN, 0, INT64_MAX, 0);
            {
                QMutexLocker fqLock(&m_frameQueueMutex);
                m_frameQueue.clear();
            }
            flushVideoDecoder();
            flushAudioDecoder();
            if (m_audioSink && m_audioIODevice) {
                m_audioSink->reset();
            }
            continue;
        }

        if (packet.stream_index == m_videoStreamIndex) {
            if (frameQueueSize() < MAX_FRAME_QUEUE_SIZE) {
                decodeVideoPacket(&packet);
            } else {
                msleep(5);
            }
        } else if (packet.stream_index == m_audioStreamIndex) {
            decodeAudioPacket(&packet);
        }
        av_packet_unref(&packet);
    }
}

bool VideoDecoder::decodeVideoPacket(const AVPacket *packet) {
    int ret = avcodec_send_packet(m_videoCodecCtx, packet);
    if (ret < 0) {
        return false;
    }

    ret = avcodec_receive_frame(m_videoCodecCtx, m_videoFrame);
    if (ret != 0) {
        return false;
    }

    sws_scale(m_swsCtx, m_videoFrame->data, m_videoFrame->linesize, 0, m_height,
              m_rgbaFrame->data, m_rgbaFrame->linesize);

    int bufferSize =
        av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 1);

    VideoFrame frame;
    frame.rgbaData =
        QByteArray(reinterpret_cast<const char *>(m_rgbaBuffer), bufferSize);
    frame.width = m_width;
    frame.height = m_height;

    {
        QMutexLocker lock(&m_frameQueueMutex);
        m_frameQueue.enqueue(frame);
    }

    av_frame_unref(m_videoFrame);
    return true;
}

bool VideoDecoder::decodeAudioPacket(const AVPacket *packet) {
    if (!m_audioCodecCtx || !m_swrCtx || !m_audioIODevice) {
        return false;
    }

    int ret = avcodec_send_packet(m_audioCodecCtx, packet);
    if (ret < 0) {
        return false;
    }

    while (avcodec_receive_frame(m_audioCodecCtx, m_audioFrame) == 0) {
        // 计算重采样后样本数。
        int outSamples =
            swr_get_out_samples(m_swrCtx, m_audioFrame->nb_samples);

        // 分配输出缓冲。
        int outBufferSize = av_samples_get_buffer_size(
            nullptr, m_audioChannels, outSamples, AV_SAMPLE_FMT_S16, 1);
        QByteArray pcmBuffer(outBufferSize, 0);

        uint8_t *outData[1] = {reinterpret_cast<uint8_t *>(pcmBuffer.data())};
        int convertedSamples =
            swr_convert(m_swrCtx, outData, outSamples,
                        const_cast<const uint8_t **>(m_audioFrame->data),
                        m_audioFrame->nb_samples);

        if (convertedSamples > 0) {
            int actualSize = av_samples_get_buffer_size(
                nullptr, m_audioChannels, convertedSamples, AV_SAMPLE_FMT_S16,
                1);

            // 处理 QAudioSink 缓冲满导致的部分写入，避免丢数据。
            int written = 0;
            int retries = 0;
            while (written < actualSize && retries < 100) {
                qint64 n = m_audioIODevice->write(
                    pcmBuffer.constData() + written, actualSize - written);
                if (n > 0) {
                    written += static_cast<int>(n);
                    retries = 0;
                } else {
                    ++retries;
                    msleep(2);
                }
            }
        }

        av_frame_unref(m_audioFrame);
    }

    return true;
}

void VideoDecoder::initAudioOutput() {
    if (m_audioStreamIndex < 0 || !m_audioCodecCtx) {
        return;
    }

    QAudioFormat format;
    format.setSampleRate(m_audioSampleRate);
    format.setChannelCount(m_audioChannels);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isFormatSupported(format)) {
        qWarning()
            << "[VideoDecoder] audio format not supported, using nearest";
        format = device.preferredFormat();
    }

    m_audioSink = new QAudioSink(device, format, this);
    // 增大音频缓冲，减少因解码波动导致的卡顿。
    m_audioSink->setBufferSize(format.bytesForDuration(200000));  // 200ms 缓冲
    m_audioIODevice = m_audioSink->start();
    if (!m_audioIODevice) {
        qWarning() << "[VideoDecoder] failed to start audio output";
    }
}

void VideoDecoder::stopAudioOutput() {
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
        m_audioIODevice = nullptr;
    }
}

void VideoDecoder::flushAudioDecoder() {
    if (!m_audioCodecCtx) {
        return;
    }
    avcodec_send_packet(m_audioCodecCtx, nullptr);
    while (avcodec_receive_frame(m_audioCodecCtx, m_audioFrame) == 0) {
        av_frame_unref(m_audioFrame);
    }
    if (m_swrCtx) {
        swr_init(m_swrCtx);
    }
}

void VideoDecoder::flushVideoDecoder() {
    if (!m_videoCodecCtx) {
        return;
    }
    avcodec_send_packet(m_videoCodecCtx, nullptr);
    while (avcodec_receive_frame(m_videoCodecCtx, m_videoFrame) == 0) {
        av_frame_unref(m_videoFrame);
    }
}
