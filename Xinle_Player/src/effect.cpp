#include "effect.h"

#include <QDebug>
#include <QFile>

Effect::~Effect() {
    if (m_program) {
        delete m_program;
        m_program = nullptr;
    }
}

bool Effect::load(const QString &vertexPath, const QString &fragPath) {
    m_vertPath = vertexPath;
    m_fragPath = fragPath;
    return loadInternal();
}

bool Effect::reloadFragment(const QString &fragPath) {
    m_fragPath = fragPath;
    return loadInternal();
}

bool Effect::loadInternal() {
    if (m_program) {
        delete m_program;
        m_program = nullptr;
    }

    m_program = new QOpenGLShaderProgram();

    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                            m_vertPath)) {
        qWarning() << "[Effect] vertex shader error:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return false;
    }

    if (!m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                            m_fragPath)) {
        qWarning() << "[Effect] fragment shader error:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return false;
    }

    if (!m_program->link()) {
        qWarning() << "[Effect] link error:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return false;
    }

    return true;
}

void Effect::bind() {
    if (m_program) {
        m_program->bind();
    }
}

void Effect::release() {
    if (m_program) {
        m_program->release();
    }
}

void Effect::setUniform(const QString &name, const QVariant &value) {
    if (!m_program) {
        return;
    }

    const QByteArray nameUtf8 = name.toUtf8();
    const char *namePtr = nameUtf8.constData();

    switch (value.metaType().id()) {
        case QMetaType::Int:
        case QMetaType::Long:
        case QMetaType::LongLong:
        case QMetaType::Short:
            m_program->setUniformValue(namePtr, value.toInt());
            break;
        case QMetaType::UInt:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
        case QMetaType::UShort:
            m_program->setUniformValue(namePtr, value.toUInt());
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            m_program->setUniformValue(namePtr, value.toFloat());
            break;
        case QMetaType::Bool:
            m_program->setUniformValue(namePtr, value.toBool());
            break;
        case QMetaType::QVector2D:
            m_program->setUniformValue(namePtr, value.value<QVector2D>());
            break;
        case QMetaType::QVector3D:
            m_program->setUniformValue(namePtr, value.value<QVector3D>());
            break;
        case QMetaType::QVector4D:
            m_program->setUniformValue(namePtr, value.value<QVector4D>());
            break;
        case QMetaType::QMatrix4x4:
            m_program->setUniformValue(namePtr, value.value<QMatrix4x4>());
            break;
        default:
            // 支持 float 数组：QVariantList 或 QList<float>。
            if (value.metaType().id() == QMetaType::QVariantList) {
                QVariantList list = value.toList();
                QVector<float> floats;
                floats.reserve(list.size());
                for (const QVariant &v : list) {
                    floats.append(v.toFloat());
                }
                m_program->setUniformValueArray(namePtr, floats.constData(),
                                                floats.size(), 1);
            } else {
                qWarning() << "[Effect] unsupported uniform type for" << name
                           << ":" << value.typeName();
            }
            break;
    }
}

void Effect::setUniforms(const QMap<QString, QVariant> &params) {
    for (auto it = params.begin(); it != params.end(); ++it) {
        setUniform(it.key(), it.value());
    }
}

void Effect::setResolution(int width, int height) {
    setUniform(
        QStringLiteral("resolution"),
        QVector2D(static_cast<float>(width), static_cast<float>(height)));
}

void Effect::setTime(float seconds) {
    setUniform(QStringLiteral("time"), seconds);
}

void Effect::setIteration(int iteration) {
    setUniform(QStringLiteral("iteration"), iteration);
}

QString Effect::log() const {
    if (m_program) {
        return m_program->log();
    }
    return QString();
}
