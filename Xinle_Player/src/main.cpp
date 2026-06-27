#include "player.h"

#include <QApplication>
#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

int main(int argc, char *argv[]) {
    qDebug() << "FFmpeg version:" << av_version_info();
    qDebug() << "avcodec version:" << avcodec_version();
    qDebug() << "avformat version:" << avformat_version();
    qDebug() << "avutil version:" << avutil_version();

    QApplication app(argc, argv);

    Player window;
    window.show();

    return app.exec();
}
