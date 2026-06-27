#pragma once

#include "effectregistry.h"
#include "glwidget.h"
#include "ui_player.h"
#include "videodecoder.h"

#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class Player : public QWidget {
    Q_OBJECT

public:
    explicit Player(QWidget *parent = nullptr);
    ~Player();

private slots:
    void onOpenFile();
    void onPlayPause();
    void onVideoTimer();
    void onDecodeError(const QString message);
    void onEffectChanged(int index);
    void onEffectParamChanged(int value);

private:
    void setupMaskEffect(QMap<QString, QVariant> &params);
    void applyEffectParams(int sliderValue);

    Ui::PlayerClass ui;

    GLWidget *m_glWidget = nullptr;
    VideoDecoder *m_decoder = nullptr;
    QTimer *m_videoTimer = nullptr;

    EffectRegistry m_effectRegistry;

    QPushButton *m_openBtn = nullptr;
    QPushButton *m_playBtn = nullptr;
    QComboBox *m_effectCombo = nullptr;
    QSlider *m_effectSlider = nullptr;

    int m_currentEffectIndex = -1;
};
