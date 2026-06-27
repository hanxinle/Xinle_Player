#pragma once

#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

/**
 * @brief 基于 FFmpeg 7.x 的音视频解码器。
 *
 * 解码线程全速读取音视频包：音频直接写入 QAudioSink；视频帧压入队列，由调用方按帧率取出显示。
 */
class VideoDecoder : public QThread {
    Q_OBJECT

public:
    explicit VideoDecoder(QObject *parent = nullptr);
    ~VideoDecoder();

    // 打开本地视频文件。
    bool open(const QString &filePath);

    // 关闭并释放资源。
    void close();

    // 播放控制。
    void play();
    void pause();
    void seek(double seconds);

    // 当前是否正在播放。
    bool isPlaying() const;

    // 从视频帧队列取出一帧。返回 false 表示队列为空。
    bool getVideoFrame(QByteArray &rgbaData, int &width, int &height);

    // 当前队列中的帧数。
    int frameQueueSize() const;

    // 视频基本信息。
    int width() const {
        return m_width;
    }
    int height() const {
        return m_height;
    }
    double duration() const {
        return m_duration;
    }
    double frameRate() const {
        return m_frameRate;
    }

signals:
    void decodeError(const QString message);

protected:
    void run() override;

private:
    struct VideoFrame {
        QByteArray rgbaData;
        int width = 0;
        int height = 0;
    };

    bool decodeVideoPacket(const AVPacket *packet);
    bool decodeAudioPacket(const AVPacket *packet);
    void initAudioOutput();
    void stopAudioOutput();
    void flushAudioDecoder();
    void flushVideoDecoder();

    QString m_filePath;

    AVFormatContext *m_fmtCtx = nullptr;

    // 视频。
    AVCodecContext *m_videoCodecCtx = nullptr;
    const AVCodec *m_videoCodec = nullptr;
    AVFrame *m_videoFrame = nullptr;
    AVFrame *m_rgbaFrame = nullptr;
    SwsContext *m_swsCtx = nullptr;
    uint8_t *m_rgbaBuffer = nullptr;
    int m_videoStreamIndex = -1;

    int m_width = 0;
    int m_height = 0;
    double m_duration = 0.0;
    double m_frameRate = 0.0;

    // 音频。
    AVCodecContext *m_audioCodecCtx = nullptr;
    const AVCodec *m_audioCodec = nullptr;
    AVFrame *m_audioFrame = nullptr;
    SwrContext *m_swrCtx = nullptr;
    int m_audioStreamIndex = -1;
    int m_audioSampleRate = 48000;
    int m_audioChannels = 2;

    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioIODevice = nullptr;

    // 视频帧队列。
    mutable QMutex m_frameQueueMutex;
    QQueue<VideoFrame> m_frameQueue;
    static constexpr int MAX_FRAME_QUEUE_SIZE = 60;

    bool m_running = false;
    bool m_playing = false;
    bool m_seekRequested = false;
    double m_seekTarget = 0.0;
    mutable QMutex m_stateMutex;
};
