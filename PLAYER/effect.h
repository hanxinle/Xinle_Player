#pragma once

#include <QMap>
#include <QOpenGLShaderProgram>
#include <QString>
#include <QVariant>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

/**
 * @brief 通用 GLSL 特效。
 *
 * 每个特效由一对 shader 组成：固定 common.vert + 用户提供的 .frag。
 * 通过 setUniform 注入任意 uniform 参数，方便后续自定义 frag 文件。
 */
class Effect {
public:
    Effect() = default;
    ~Effect();

    // 加载 shader。vertexPath 通常为 common.vert，fragPath 为特效 .frag 文件。
    bool load(const QString &vertexPath, const QString &fragPath);

    // 重新加载 fragment shader，用于运行时替换特效。
    bool reloadFragment(const QString &fragPath);

    bool isValid() const {
        return m_program != nullptr && m_program->isLinked();
    }

    void bind();
    void release();

    // 设置 uniform。支持 float、int、bool、QVector2D/3D/4D、QMatrix4x4。
    void setUniform(const QString &name, const QVariant &value);
    void setUniforms(const QMap<QString, QVariant> &params);

    // 预置标准 uniform。
    void setResolution(int width, int height);
    void setTime(float seconds);
    void setIteration(int iteration);

    const QString &fragmentPath() const {
        return m_fragPath;
    }
    QString log() const;

private:
    bool loadInternal();

    QOpenGLShaderProgram *m_program = nullptr;
    QString m_vertPath;
    QString m_fragPath;
};
