#include "effectregistry.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

EffectRegistry::EffectRegistry() {}

bool EffectRegistry::load(const QString &path) {
    m_effects.clear();
    m_lastError.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("无法打开特效配置文件: %1").arg(path);
        qWarning() << "[EffectRegistry]" << m_lastError;
        return false;
    }

    QByteArray data = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError =
            QStringLiteral("JSON 解析错误: %1").arg(parseError.errorString());
        qWarning() << "[EffectRegistry]" << m_lastError;
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray effectsArray = root.value(QStringLiteral("effects")).toArray();

    for (const QJsonValue &effectValue : effectsArray) {
        QJsonObject effectObj = effectValue.toObject();
        EffectDef def;
        def.name = effectObj.value(QStringLiteral("name")).toString();
        def.frag = effectObj.value(QStringLiteral("frag")).toString();
        def.iterations = effectObj.value(QStringLiteral("iterations")).toInt(1);
        def.setup = effectObj.value(QStringLiteral("setup")).toString();

        if (def.name.isEmpty()) {
            qWarning() << "[EffectRegistry] 跳过没有 name 的特效";
            continue;
        }

        QJsonArray paramsArray =
            effectObj.value(QStringLiteral("params")).toArray();
        for (const QJsonValue &paramValue : paramsArray) {
            QJsonObject paramObj = paramValue.toObject();
            EffectParamDef param;
            param.name = paramObj.value(QStringLiteral("name")).toString();
            param.label = paramObj.value(QStringLiteral("label")).toString();
            param.type = paramObj.value(QStringLiteral("type")).toString();

            if (param.name.isEmpty() || param.type.isEmpty()) {
                qWarning() << "[EffectRegistry] 跳过不完整参数";
                continue;
            }

            if (param.type == QStringLiteral("float") ||
                param.type == QStringLiteral("int")) {
                param.min = paramObj.value(QStringLiteral("min")).toDouble(0.0);
                param.max =
                    paramObj.value(QStringLiteral("max")).toDouble(100.0);
            }

            param.defaultValue = parseDefaultValue(
                paramObj.value(QStringLiteral("default")), param.type);

            def.params.append(param);
        }

        m_effects.append(def);
    }

    return true;
}

const EffectDef *EffectRegistry::effectByName(const QString &name) const {
    for (const EffectDef &def : m_effects) {
        if (def.name == name) {
            return &def;
        }
    }
    return nullptr;
}

int EffectRegistry::indexByName(const QString &name) const {
    for (int i = 0; i < m_effects.size(); ++i) {
        if (m_effects[i].name == name) {
            return i;
        }
    }
    return -1;
}

QVariant EffectRegistry::parseDefaultValue(const QJsonValue &value,
                                           const QString &type) const {
    if (type == QStringLiteral("bool")) {
        return value.toBool(false);
    }
    if (type == QStringLiteral("int")) {
        return value.toInt(0);
    }
    if (type == QStringLiteral("float")) {
        return static_cast<float>(value.toDouble(0.0));
    }
    if (type == QStringLiteral("vec2")) {
        QJsonArray arr = value.toArray();
        if (arr.size() >= 2) {
            return QVariant::fromValue(
                QVector2D(static_cast<float>(arr[0].toDouble()),
                          static_cast<float>(arr[1].toDouble())));
        }
        return QVariant::fromValue(QVector2D());
    }
    if (type == QStringLiteral("vec3")) {
        QJsonArray arr = value.toArray();
        if (arr.size() >= 3) {
            return QVariant::fromValue(
                QVector3D(static_cast<float>(arr[0].toDouble()),
                          static_cast<float>(arr[1].toDouble()),
                          static_cast<float>(arr[2].toDouble())));
        }
        return QVariant::fromValue(QVector3D());
    }
    if (type == QStringLiteral("vec4")) {
        QJsonArray arr = value.toArray();
        if (arr.size() >= 4) {
            return QVariant::fromValue(
                QVector4D(static_cast<float>(arr[0].toDouble()),
                          static_cast<float>(arr[1].toDouble()),
                          static_cast<float>(arr[2].toDouble()),
                          static_cast<float>(arr[3].toDouble())));
        }
        return QVariant::fromValue(QVector4D());
    }

    return QVariant();
}
