#include "glwidget.h"

#include <QDebug>

GLWidget::GLWidget(QWidget *parent) : QOpenGLWidget(parent) {}

GLWidget::~GLWidget() {
    makeCurrent();

    clearEffect();

    if (m_passThroughProgram) {
        delete m_passThroughProgram;
        m_passThroughProgram = nullptr;
    }
    if (m_videoTexture != 0) {
        glDeleteTextures(1, &m_videoTexture);
        m_videoTexture = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    for (int i = 0; i < 2; ++i) {
        delete m_fbo[i];
        m_fbo[i] = nullptr;
    }

    doneCurrent();
}

void GLWidget::setVideoFrame(const QByteArray &rgbaData, int width,
                             int height) {
    if (width <= 0 || height <= 0 || rgbaData.isEmpty()) {
        return;
    }

    bool sizeChanged = (width != m_videoWidth || height != m_videoHeight);
    m_frameData = rgbaData;
    m_videoWidth = width;
    m_videoHeight = height;
    m_frameDirty = true;

    if (sizeChanged) {
        m_fboSizeDirty = true;
        updateMvp();
    }

    update();
}

bool GLWidget::loadEffect(const QString &fragPath) {
    makeCurrent();

    clearEffect();

    Effect *effect = new Effect();
    if (!effect->load(QStringLiteral(":/Player/shaders/common.vert"),
                      fragPath)) {
        qWarning() << "[GLWidget] failed to load effect:" << fragPath;
        delete effect;
        return false;
    }

    m_effect = effect;
    m_effectParams.clear();
    m_effectIterations = 1;

    // 默认传入 resolution。
    m_effect->setResolution(m_videoWidth, m_videoHeight);

    update();
    return true;
}

void GLWidget::clearEffect() {
    if (m_effect) {
        makeCurrent();
        delete m_effect;
        m_effect = nullptr;
    }
    m_effectParams.clear();
    m_effectIterations = 1;
}

void GLWidget::setEffectParam(const QString &name, const QVariant &value) {
    m_effectParams[name] = value;
    update();
}

void GLWidget::setEffectParams(const QMap<QString, QVariant> &params) {
    m_effectParams = params;
    update();
}

void GLWidget::initializeGL() {
    initializeOpenGLFunctions();

    qDebug() << "OpenGL Vendor:"
             << reinterpret_cast<const char *>(glGetString(GL_VENDOR));
    qDebug() << "OpenGL Renderer:"
             << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    qDebug() << "OpenGL Version:"
             << reinterpret_cast<const char *>(glGetString(GL_VERSION));
    qDebug() << "GLSL Version:"
             << reinterpret_cast<const char *>(
                    glGetString(GL_SHADING_LANGUAGE_VERSION));

    initQuadGeometry();
    loadPassThroughShader();

    glGenTextures(1, &m_videoTexture);
    glBindTexture(GL_TEXTURE_2D, m_videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    updateMvp();
}

void GLWidget::paintGL() {
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_passThroughProgram || !m_passThroughProgram->isLinked() ||
        m_videoWidth <= 0 || m_videoHeight <= 0) {
        return;
    }

    if (m_fboSizeDirty) {
        recreateFbos();
        m_fboSizeDirty = false;
    }

    uploadTexture();

    if (m_effect && m_effect->isValid()) {
        renderWithEffect();
    } else {
        renderPassThrough();
    }
}

void GLWidget::initQuadGeometry() {
    // 全屏四边形：位置 + 纹理坐标。
    // 注意纹理坐标 v=0 在顶部，与 FFmpeg 解码出的 RGBA 图像方向一致。
    float vertices[] = {
        // positions      // texcoords
        -1.0f, 1.0f,  0.0f, 0.0f,  // 左上
        1.0f,  1.0f,  1.0f, 0.0f,  // 右上
        1.0f,  -1.0f, 1.0f, 1.0f,  // 右下
        -1.0f, -1.0f, 0.0f, 1.0f  // 左下
    };

    unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

bool GLWidget::loadPassThroughShader() {
    m_passThroughProgram = new QOpenGLShaderProgram(this);
    if (!m_passThroughProgram->addShaderFromSourceFile(
            QOpenGLShader::Vertex,
            QStringLiteral(":/Player/shaders/common.vert"))) {
        qCritical() << "[GLWidget] pass-through vertex shader error:"
                    << m_passThroughProgram->log();
        return false;
    }
    if (!m_passThroughProgram->addShaderFromSourceFile(
            QOpenGLShader::Fragment,
            QStringLiteral(":/Player/shaders/pass_through.frag"))) {
        qCritical() << "[GLWidget] pass-through fragment shader error:"
                    << m_passThroughProgram->log();
        return false;
    }
    if (!m_passThroughProgram->link()) {
        qCritical() << "[GLWidget] pass-through link error:"
                    << m_passThroughProgram->log();
        return false;
    }
    return true;
}

void GLWidget::uploadTexture() {
    if (!m_frameDirty) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_videoTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_videoWidth, m_videoHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_frameData.constData());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_frameDirty = false;
}

void GLWidget::updateMvp() {
    m_mvp.setToIdentity();

    if (m_videoWidth <= 0 || m_videoHeight <= 0 || width() <= 0 ||
        height() <= 0) {
        return;
    }

    float widgetAspect =
        static_cast<float>(width()) / static_cast<float>(height());
    float videoAspect =
        static_cast<float>(m_videoWidth) / static_cast<float>(m_videoHeight);

    if (widgetAspect > videoAspect) {
        m_mvp.scale(videoAspect / widgetAspect, 1.0f, 1.0f);
    } else {
        m_mvp.scale(1.0f, widgetAspect / videoAspect, 1.0f);
    }
}

void GLWidget::drawFullscreenQuad() {
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void GLWidget::renderPassThrough() {
    m_passThroughProgram->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTexture);
    m_passThroughProgram->setUniformValue("image", 0);
    m_passThroughProgram->setUniformValue("mvp", m_mvp);
    m_passThroughProgram->setUniformValue("flipY", 0.0f);

    drawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    m_passThroughProgram->release();
}

void GLWidget::renderWithEffect() {
    // 先保存窗口 viewport，FBO 渲染完成后再恢复。
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // 确保 FBO 尺寸与视频一致。
    recreateFbos();

    GLuint sourceTexture = m_videoTexture;
    int srcFbo = 0;
    int dstFbo = 1;

    // 第 0 遍：先把视频纹理拷贝到 FBO[0]。
    renderTextureToFbo(sourceTexture, m_fbo[srcFbo], nullptr, 0);

    // 特效多遍渲染：FBO[0] -> FBO[1] -> FBO[0] -> ...
    for (int i = 0; i < m_effectIterations; ++i) {
        m_effect->bind();
        m_effect->setUniforms(m_effectParams);
        m_effect->setResolution(m_videoWidth, m_videoHeight);
        m_effect->setIteration(i);

        renderTextureToFbo(m_fbo[srcFbo]->texture(), m_fbo[dstFbo], m_effect,
                           i);

        qSwap(srcFbo, dstFbo);
    }

    // 恢复窗口 viewport 后再渲染到屏幕。
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    // 最后把结果渲染到屏幕。
    renderTextureToScreen(m_fbo[srcFbo]->texture());
}

void GLWidget::renderTextureToScreen(GLuint texture) {
    // 不在这里设置 viewport，保持 Qt 在 paintGL 前设置好的物理 framebuffer viewport。

    m_passThroughProgram->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    m_passThroughProgram->setUniformValue("image", 0);
    m_passThroughProgram->setUniformValue("mvp", m_mvp);
    m_passThroughProgram->setUniformValue("flipY", 1.0f);

    drawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    m_passThroughProgram->release();
}

void GLWidget::renderTextureToFbo(GLuint srcTexture,
                                  QOpenGLFramebufferObject *dstFbo,
                                  Effect *effect, int iteration) {
    dstFbo->bind();
    glViewport(0, 0, m_videoWidth, m_videoHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (effect) {
        effect->bind();
    } else {
        m_passThroughProgram->bind();
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTexture);

    if (effect) {
        effect->setUniform(QStringLiteral("image"), 0);
        effect->setUniform(QStringLiteral("mvp"),
                           QVariant::fromValue(QMatrix4x4()));
        effect->setUniform(QStringLiteral("flipY"), 1.0f);
    } else {
        m_passThroughProgram->setUniformValue("image", 0);
        m_passThroughProgram->setUniformValue("mvp", QMatrix4x4());
        m_passThroughProgram->setUniformValue("flipY", 0.0f);
    }

    drawFullscreenQuad();

    glBindTexture(GL_TEXTURE_2D, 0);

    if (effect) {
        effect->release();
    } else {
        m_passThroughProgram->release();
    }

    dstFbo->release();
}

void GLWidget::recreateFbos() {
    if (m_videoWidth <= 0 || m_videoHeight <= 0) {
        return;
    }

    for (int i = 0; i < 2; ++i) {
        if (m_fbo[i] == nullptr || m_fbo[i]->width() != m_videoWidth ||
            m_fbo[i]->height() != m_videoHeight) {
            delete m_fbo[i];
            QOpenGLFramebufferObjectFormat format;
            format.setAttachment(QOpenGLFramebufferObject::NoAttachment);
            format.setTextureTarget(GL_TEXTURE_2D);
            format.setInternalTextureFormat(GL_RGBA8);
            m_fbo[i] = new QOpenGLFramebufferObject(m_videoWidth, m_videoHeight,
                                                    format);
        }
    }
}
