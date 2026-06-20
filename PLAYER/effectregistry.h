#pragma once

#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>

/**
 * @brief 特效参数定义。
 *
 * 支持类型：float、int、bool。
 * 对于 float/int，可指定 min/max 用于 UI 滑块范围。
 */
struct EffectParamDef {
    QString name;
    QString label;
    QString type;
    QVariant defaultValue;
    double min = 0.0;
    double max = 100.0;
};

/**
 * @brief 特效定义。
 */
struct EffectDef {
    QString name;
    QString frag;  // 空字符串表示无特效。
    int iterations = 1;
    QString setup;  // 可选的 setup 标识，如 "mask"。
    QVector<EffectParamDef> params;
};

/**
 * @brief 特效注册表。
 *
 * 从 effects.json 加载特效配置，供 Player 动态生成 UI 和绑定 uniform。
 */
class EffectRegistry {
public:
    EffectRegistry();

    // 从 JSON 文件加载特效配置。path 可以是资源路径、绝对路径或相对路径。
    bool load(const QString &path);

    const QVector<EffectDef> &effects() const {
        return m_effects;
    }

    // 按名称查找特效定义，返回 nullptr 表示未找到。
    const EffectDef *effectByName(const QString &name) const;

    // 按名称查找特效索引，返回 -1 表示未找到。
    int indexByName(const QString &name) const;

    QString lastError() const {
        return m_lastError;
    }

private:
    QVariant parseDefaultValue(const QJsonValue &value,
                               const QString &type) const;

    QVector<EffectDef> m_effects;
    QString m_lastError;
};
