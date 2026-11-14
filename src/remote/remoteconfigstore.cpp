#include "remoteconfigstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <memory>

namespace {
const QString kMappingsKey = QStringLiteral("remote/mappings");

std::unique_ptr<QSettings> createSettings(const QString &fileName)
{
    if (!fileName.isEmpty())
        return std::make_unique<QSettings>(fileName, QSettings::IniFormat);
    return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                       QStringLiteral("IndustrialHub"),
                                       QStringLiteral("IndustrialHub"));
}

QJsonObject toJson(const RemoteTelemetryMapping &item)
{
    QJsonObject object;
    object.insert(QStringLiteral("protocol"), item.protocol);
    object.insert(QStringLiteral("channelPattern"), item.channelPattern);
    object.insert(QStringLiteral("deviceId"), item.deviceId);
    object.insert(QStringLiteral("variableId"), item.variableId);
    object.insert(QStringLiteral("name"), item.name);
    object.insert(QStringLiteral("unit"), item.unit);
    return object;
}
}

RemoteConfigStore::RemoteConfigStore(const QString &settingsFile)
    : m_settingsFile(settingsFile)
{
}

QList<RemoteTelemetryMapping> RemoteConfigStore::mappings() const
{
    const auto settings = createSettings(m_settingsFile);
    const QJsonDocument document = QJsonDocument::fromJson(
        settings->value(kMappingsKey).toByteArray());
    QList<RemoteTelemetryMapping> result;
    for (const QJsonValue &value : document.array()) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        RemoteTelemetryMapping item;
        item.protocol = object.value(QStringLiteral("protocol")).toString();
        item.channelPattern = object.value(QStringLiteral("channelPattern")).toString();
        item.deviceId = object.value(QStringLiteral("deviceId")).toString();
        item.variableId = object.value(QStringLiteral("variableId")).toString();
        item.name = object.value(QStringLiteral("name")).toString();
        item.unit = object.value(QStringLiteral("unit")).toString();
        if (!item.protocol.isEmpty() && !item.channelPattern.isEmpty()
            && !item.deviceId.isEmpty())
            result.append(item);
    }
    return result;
}

bool RemoteConfigStore::upsertMapping(const RemoteTelemetryMapping &mapping,
                                      QString *errorMessage)
{
    const QString protocol = mapping.protocol.trimmed().toUpper();
    const QString pattern = mapping.channelPattern.trimmed();
    const QString deviceId = mapping.deviceId.trimmed();
    if (protocol != QStringLiteral("MQTT") && protocol != QStringLiteral("WEBSOCKET")
        && protocol != QStringLiteral("HTTP")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("协议必须是 MQTT、WebSocket 或 HTTP。");
        return false;
    }
    if (pattern.isEmpty() || pattern.size() > 512 || deviceId.isEmpty()
        || deviceId.size() > 128 || mapping.variableId.trimmed().size() > 128
        || mapping.name.size() > 128 || mapping.unit.size() > 32) {
        if (errorMessage)
            *errorMessage = QStringLiteral("映射字段为空或超过长度限制。");
        return false;
    }
    if (protocol == QStringLiteral("MQTT")) {
        const QStringList segments = pattern.split(QLatin1Char('/'));
        for (int i = 0; i < segments.size(); ++i) {
            const QString &segment = segments.at(i);
            if ((segment.contains(QLatin1Char('+')) && segment != QStringLiteral("+"))
                || (segment.contains(QLatin1Char('#')) && segment != QStringLiteral("#"))
                || (segment == QStringLiteral("#") && i != segments.size() - 1)) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("MQTT + 必须独占一个 Topic 层级，# 只能位于末尾。");
                return false;
            }
        }
    }

    QList<RemoteTelemetryMapping> all = mappings();
    bool replaced = false;
    for (RemoteTelemetryMapping &existing : all) {
        if (existing.protocol.compare(protocol, Qt::CaseInsensitive) == 0
            && existing.channelPattern == pattern) {
            existing = mapping;
            existing.protocol = protocol;
            existing.channelPattern = pattern;
            existing.deviceId = deviceId;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        RemoteTelemetryMapping normalized = mapping;
        normalized.protocol = protocol;
        normalized.channelPattern = pattern;
        normalized.deviceId = deviceId;
        all.append(normalized);
    }

    QJsonArray array;
    for (const RemoteTelemetryMapping &item : all)
        array.append(toJson(item));
    auto settings = createSettings(m_settingsFile);
    settings->setValue(kMappingsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings->sync();
    if (settings->status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("保存远程映射配置失败。");
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool RemoteConfigStore::removeMapping(const QString &protocol,
                                      const QString &channelPattern,
                                      QString *errorMessage)
{
    QList<RemoteTelemetryMapping> all = mappings();
    const int before = all.size();
    for (int i = all.size() - 1; i >= 0; --i) {
        if (all.at(i).protocol.compare(protocol.trimmed(), Qt::CaseInsensitive) == 0
            && all.at(i).channelPattern == channelPattern.trimmed())
            all.removeAt(i);
    }
    if (all.size() == before) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未找到指定远程映射。");
        return false;
    }

    QJsonArray array;
    for (const RemoteTelemetryMapping &item : all)
        array.append(toJson(item));
    auto settings = createSettings(m_settingsFile);
    settings->setValue(kMappingsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings->sync();
    if (settings->status() != QSettings::NoError) {
        if (errorMessage)
            *errorMessage = QStringLiteral("删除远程映射配置失败。");
        return false;
    }
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool RemoteConfigStore::resolveMapping(const QString &protocol, const QString &channel,
                                       RemoteTelemetryMapping &mapping) const
{
    const QList<RemoteTelemetryMapping> all = mappings();
    for (const RemoteTelemetryMapping &candidate : all) {
        if (candidate.protocol.compare(protocol.trimmed(), Qt::CaseInsensitive) == 0
            && candidate.channelPattern == channel) {
            mapping = candidate;
            return true;
        }
    }

    for (const RemoteTelemetryMapping &candidate : all) {
        if (candidate.protocol.compare(protocol.trimmed(), Qt::CaseInsensitive) != 0)
            continue;

        if (candidate.protocol.compare(QStringLiteral("MQTT"), Qt::CaseInsensitive) == 0) {
            const QStringList segments = candidate.channelPattern.split(QLatin1Char('/'));
            QStringList regexSegments;
            for (const QString &segment : segments) {
                if (segment == QStringLiteral("+"))
                    regexSegments.append(QStringLiteral("[^/]+"));
                else if (segment == QStringLiteral("#") || segment == QStringLiteral("*"))
                    regexSegments.append(QStringLiteral(".*"));
                else
                    regexSegments.append(QRegularExpression::escape(segment));
            }
            QString regex = regexSegments.join(QLatin1Char('/'));
            if (!segments.isEmpty() && segments.last() == QStringLiteral("#")) {
                if (segments.size() == 1)
                    regex = QStringLiteral(".*");
                else
                    regex = regexSegments.mid(0, regexSegments.size() - 1).join(QLatin1Char('/'))
                        + QStringLiteral("(?:/.*)?");
            }
            const QRegularExpression expression(
                QStringLiteral("^") + regex + QStringLiteral("$"));
            if (expression.match(channel).hasMatch()) {
                mapping = candidate;
                return true;
            }
            continue;
        }

        const QRegularExpression expression(
            QRegularExpression::wildcardToRegularExpression(candidate.channelPattern));
        if (expression.match(channel).hasMatch()) {
            mapping = candidate;
            return true;
        }
    }
    return false;
}
