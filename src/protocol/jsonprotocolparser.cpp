#include "jsonprotocolparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

#include <cmath>

QString JsonProtocolParser::protocolName() const
{
    return QStringLiteral("JSON");
}

bool JsonProtocolParser::parse(const QString &deviceId,
                               const QByteArray &payload,
                               QVector<BusinessDataPoint> &points,
                               QString *errorMessage) const
{
    const QString normalizedDeviceId = deviceId.trimmed();
    if (normalizedDeviceId.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("设备编号不能为空。");
        return false;
    }

    QJsonParseError jsonError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("JSON 报文格式错误：%1")
                                .arg(jsonError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue valuesValue = root.value(QStringLiteral("values"));
    if (!valuesValue.isArray() || valuesValue.toArray().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("JSON 报文必须包含非空 values 数组。");
        return false;
    }

    QDateTime timestamp = QDateTime::currentDateTime();
    const QJsonValue timestampValue = root.value(QStringLiteral("timestamp"));
    if (!timestampValue.isUndefined() && !timestampValue.isNull()) {
        if (!timestampValue.isString()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("timestamp 必须是 ISO 8601 字符串。");
            return false;
        }

        const QString timestampText = timestampValue.toString();
        timestamp = QDateTime::fromString(timestampText, Qt::ISODateWithMs);
        if (!timestamp.isValid())
            timestamp = QDateTime::fromString(timestampText, Qt::ISODate);
        if (!timestamp.isValid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("timestamp 不是有效的 ISO 8601 时间。");
            return false;
        }
    }

    QVector<BusinessDataPoint> parsedPoints;
    const QJsonArray values = valuesValue.toArray();
    QSet<QString> variableIds;

    for (qsizetype index = 0; index < values.size(); ++index) {
        if (!values.at(index).isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("values[%1] 必须是对象。").arg(index);
            return false;
        }

        const QJsonObject valueObject = values.at(index).toObject();
        const QString variableId =
            valueObject.value(QStringLiteral("id")).toString().trimmed();
        const QJsonValue numericValue = valueObject.value(QStringLiteral("value"));

        if (variableId.isEmpty() || !numericValue.isDouble()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "values[%1] 必须包含非空 id 和数值型 value。").arg(index);
            }
            return false;
        }

        const double number = numericValue.toDouble();
        if (!std::isfinite(number)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("values[%1].value 不是有限数值。").arg(index);
            return false;
        }

        const QString normalizedVariableId = variableId.toCaseFolded();
        if (variableIds.contains(normalizedVariableId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("变量 ID 重复：%1").arg(variableId);
            }
            return false;
        }
        variableIds.insert(normalizedVariableId);

        const QString qualityText =
            valueObject.value(QStringLiteral("quality")).toString(QStringLiteral("good"))
                .trimmed().toLower();
        DataQuality quality = DataQuality::Good;
        if (qualityText == QStringLiteral("uncertain")) {
            quality = DataQuality::Uncertain;
        } else if (qualityText == QStringLiteral("bad")) {
            quality = DataQuality::Bad;
        } else if (qualityText != QStringLiteral("good")) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("不支持的质量状态：%1").arg(qualityText);
            }
            return false;
        }

        BusinessDataPoint point;
        point.deviceId = normalizedDeviceId;
        point.variableId = variableId;
        point.name = valueObject.value(QStringLiteral("name")).toString(variableId).trimmed();
        if (point.name.isEmpty())
            point.name = variableId;
        point.value = number;
        point.unit = valueObject.value(QStringLiteral("unit")).toString().trimmed();
        point.timestamp = timestamp;
        point.quality = quality;
        point.sourceProtocol = protocolName();
        parsedPoints.append(point);
    }

    points = parsedPoints;
    if (errorMessage)
        errorMessage->clear();
    return true;
}
