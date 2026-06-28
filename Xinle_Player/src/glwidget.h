#pragma once

#include "effect.h"

#include <QByteArray>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

#include <QMatrix4x4>
#include <QOpenGLFunctions_3_3_Core>
#include <QPixmap>

/**
 * @brief 基于现代 OpenGL 3.3 Core 的视频显示控件，支持 GLSL 特效。
 *
 * 支持上传 RGBA 视频帧并在全屏四边形上渲染；可通过 loadEffect 加载任意 .frag 特效。
 */
class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget();

    // 设置当前要显示的视频帧（RGBA 数据）。主线程调用。
    void setVideoFrame(const QByteArray &rgbaData, int width, int height);

    // 加载/卸载特效。fragPath 可以是资源路径、绝对路径或相对路径。
    bool loadEffect(const QString &fragPath);
    void clearEffect();
    bool hasEffect() const {
        return m_effect != nullptr;
    }

    // 设置特效 uniform 参数。支持 float/int/bool/QVector2D/3D/4D/QMatrix4x4。
    void setEffectParam(const QString &name, const QVariant &value);
    void setEffectParams(const QMap<QString, QVariant> &params);

    // 设置特效迭代次数（如 boxblur 需要 2 次）。
    void setEffectIterations(int iterations) {
        m_effectIterations = qMax(1, iterations);
    }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void initQuadGeometry();
    bool loadPassThroughShader();
    void uploadTexture();
    void updateMvp();
    void drawFullscreenQuad();

    void renderPassThrough();
    void renderWithEffect();
    void renderTextureToScreen(GLuint texture);
    void renderTextureToFbo(GLuint srcTexture, QOpenGLFramebufferObject *dstFbo,
                            Effect *effect, int iteration);

    void recreateFbos();

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    GLuint m_videoTexture = 0;

    QOpenGLShaderProgram *m_passThroughProgram = nullptr;

    Effect *m_effect = nullptr;
    QOpenGLFramebufferObject *m_fbo[2] = {nullptr, nullptr};
    QMap<QString, QVariant> m_effectParams;
    int m_effectIterations = 1;

    QByteArray m_frameData;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    bool m_frameDirty = false;
    bool m_fboSizeDirty = false;

    QMatrix4x4 m_mvp;
    QPixmap m_coverPixmap;
};
