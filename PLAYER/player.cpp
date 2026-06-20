#include "player.h"

#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QVector>

Player::Player(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);

    setWindowTitle(QStringLiteral("OpenGL + FFmpeg Player"));
    resize(960, 640);

    // 加载特效配置。
    QString effectsPath = QStringLiteral(":/Player/effects/effects.json");
    if (!m_effectRegistry.load(effectsPath)) {
        qWarning() << "[Player] failed to load effects registry:"
                   << m_effectRegistry.lastError();
    }

    // 主布局。
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // OpenGL 视频显示区。
    m_glWidget = new GLWidget(this);
    mainLayout->addWidget(m_glWidget, 1);

    // 控制按钮。
    auto *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);

    m_openBtn = new QPushButton(QStringLiteral("打开文件"), this);
    m_playBtn = new QPushButton(QStringLiteral("播放"), this);
    m_playBtn->setEnabled(false);

    controlLayout->addWidget(m_openBtn);
    controlLayout->addWidget(m_playBtn);
    controlLayout->addStretch();

    mainLayout->addLayout(controlLayout);

    // 特效控制。
    auto *effectLayout = new QHBoxLayout();
    effectLayout->setSpacing(8);

    effectLayout->addWidget(new QLabel(QStringLiteral("特效:"), this));
    m_effectCombo = new QComboBox(this);
    for (const EffectDef &def : m_effectRegistry.effects()) {
        m_effectCombo->addItem(def.name);
    }
    effectLayout->addWidget(m_effectCombo);

    effectLayout->addWidget(new QLabel(QStringLiteral("参数:"), this));
    m_effectSlider = new QSlider(Qt::Horizontal, this);
    m_effectSlider->setRange(0, 100);
    m_effectSlider->setValue(0);
    m_effectSlider->setEnabled(false);
    effectLayout->addWidget(m_effectSlider, 1);

    mainLayout->addLayout(effectLayout);

    // 解码器。
    m_decoder = new VideoDecoder(this);
    connect(m_decoder, &VideoDecoder::decodeError, this,
            &Player::onDecodeError);

    // 视频显示定时器。
    m_videoTimer = new QTimer(this);
    m_videoTimer->setTimerType(Qt::PreciseTimer);
    connect(m_videoTimer, &QTimer::timeout, this, &Player::onVideoTimer);

    connect(m_openBtn, &QPushButton::clicked, this, &Player::onOpenFile);
    connect(m_playBtn, &QPushButton::clicked, this, &Player::onPlayPause);
    connect(m_effectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Player::onEffectChanged);
    connect(m_effectSlider, &QSlider::valueChanged, this,
            &Player::onEffectParamChanged);

    // 默认选中第一项（通常是“无”）。
    if (m_effectCombo->count() > 0) {
        m_effectCombo->setCurrentIndex(0);
    }
}

Player::~Player() {
    if (m_videoTimer) {
        m_videoTimer->stop();
    }
    if (m_decoder) {
        m_decoder->close();
    }
}

void Player::onOpenFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开视频文件"), QString(),
        QStringLiteral("视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)"));

    if (filePath.isEmpty()) {
        return;
    }

    m_videoTimer->stop();
    if (m_decoder->isRunning()) {
        m_decoder->close();
    }

    if (!m_decoder->open(filePath)) {
        m_playBtn->setEnabled(false);
        return;
    }

    int intervalMs = m_decoder->frameRate() > 0.0
                         ? static_cast<int>(1000.0 / m_decoder->frameRate())
                         : 33;
    m_videoTimer->start(intervalMs);

    // 重新应用当前选中的特效。
    onEffectChanged(m_effectCombo->currentIndex());

    m_playBtn->setEnabled(true);
    m_playBtn->setText(QStringLiteral("播放"));
}

void Player::onPlayPause() {
    if (!m_decoder) {
        return;
    }

    if (m_decoder->isPlaying()) {
        m_decoder->pause();
        m_videoTimer->stop();
        m_playBtn->setText(QStringLiteral("播放"));
    } else {
        m_decoder->play();
        if (!m_videoTimer->isActive()) {
            int intervalMs =
                m_decoder->frameRate() > 0.0
                    ? static_cast<int>(1000.0 / m_decoder->frameRate())
                    : 33;
            m_videoTimer->start(intervalMs);
        }
        m_playBtn->setText(QStringLiteral("暂停"));
    }
}

void Player::onVideoTimer() {
    QByteArray rgbaData;
    int width = 0;
    int height = 0;
    if (m_decoder->getVideoFrame(rgbaData, width, height)) {
        m_glWidget->setVideoFrame(rgbaData, width, height);
    }
}

void Player::onDecodeError(const QString message) {
    qWarning() << "[Player] decode error:" << message;
}

void Player::onEffectChanged(int index) {
    m_currentEffectIndex = index;

    if (index < 0 || index >= m_effectRegistry.effects().size()) {
        m_glWidget->clearEffect();
        m_effectSlider->setEnabled(false);
        return;
    }

    const EffectDef &def = m_effectRegistry.effects()[index];

    // 无特效。
    if (def.frag.isEmpty()) {
        m_glWidget->clearEffect();
        m_effectSlider->setEnabled(false);
        return;
    }

    // 加载 shader。
    if (!m_glWidget->loadEffect(def.frag)) {
        m_effectSlider->setEnabled(false);
        return;
    }

    m_glWidget->setEffectIterations(def.iterations);

    // 设置所有参数默认值。
    QMap<QString, QVariant> params;
    int firstFloatParamIndex = -1;
    for (int i = 0; i < def.params.size(); ++i) {
        const EffectParamDef &param = def.params[i];
        params[param.name] = param.defaultValue;

        if (firstFloatParamIndex < 0 && param.type == QStringLiteral("float")) {
            firstFloatParamIndex = i;
        }
    }

    // 特殊 setup：遮罩。
    if (def.setup == QStringLiteral("mask")) {
        setupMaskEffect(params);
    }

    m_glWidget->setEffectParams(params);

    // 配置滑块绑定第一个 float 参数。
    if (firstFloatParamIndex >= 0) {
        const EffectParamDef &param = def.params[firstFloatParamIndex];
        m_effectSlider->setRange(static_cast<int>(param.min),
                                 static_cast<int>(param.max));
        m_effectSlider->setValue(param.defaultValue.toInt());
        m_effectSlider->setEnabled(true);
    } else {
        m_effectSlider->setEnabled(false);
    }
}

void Player::onEffectParamChanged(int value) {
    if (m_currentEffectIndex < 0 ||
        m_currentEffectIndex >= m_effectRegistry.effects().size()) {
        return;
    }

    const EffectDef &def = m_effectRegistry.effects()[m_currentEffectIndex];
    applyEffectParams(value);
}

void Player::applyEffectParams(int sliderValue) {
    if (m_currentEffectIndex < 0 ||
        m_currentEffectIndex >= m_effectRegistry.effects().size()) {
        return;
    }

    const EffectDef &def = m_effectRegistry.effects()[m_currentEffectIndex];

    // 找到第一个 float 参数，用滑块值驱动。
    for (const EffectParamDef &param : def.params) {
        if (param.type == QStringLiteral("float")) {
            m_glWidget->setEffectParam(param.name,
                                       static_cast<float>(sliderValue));
            break;
        }
    }

    // 遮罩的羽化也随滑块变化。
    if (def.setup == QStringLiteral("mask")) {
        m_glWidget->setEffectParam(QStringLiteral("mask_blur"),
                                   static_cast<float>(sliderValue));
    }
}

void Player::setupMaskEffect(QMap<QString, QVariant> &params) {
    // 示例：用一个三角形遮罩覆盖画面中心区域。
    float cx = 0.5f;
    float cy = 0.5f;
    float size = 0.3f;

    QVector<float> points;
    points << cx << cy - size;
    points << cx - size << cy + size;
    points << cx + size << cy + size;

    params[QStringLiteral("numPoints")] = points.size() / 2;
    params[QStringLiteral("isHasChromakey")] = false;

    QVariantList pointDataList;
    for (float v : points) {
        pointDataList.append(v);
    }
    params[QStringLiteral("pointData")] = pointDataList;

    // 如果 JSON 里没有默认 mask_blur，这里给一个。
    if (!params.contains(QStringLiteral("mask_blur"))) {
        params[QStringLiteral("mask_blur")] = 10.0f;
    }
}
