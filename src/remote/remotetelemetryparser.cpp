#include "remotetelemetryparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QtMath>

bool RemoteTelemetryParser::parse(const QString &protocol, const QString &channel,
                                  const QByteArray &payload, QString &deviceId,
                                  QVector<BusinessDataPoint> &points,
                                  QString *errorMessage)
{
    deviceId.clear();
    points.clear();
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };
    if (payload.isEmpty() || payload.size() > 1024 * 1024)
        return fail(QStringLiteral("远程消息为空或超过 1 MiB 限制。"));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("远程消息不是有效 JSON 对象。"));

    const QJsonObject root = document.object();
    deviceId = root.value(QStringLiteral("deviceId")).toString().trimmed();
    if (deviceId.isEmpty())
        return fail(QStringLiteral("JSON 缺少 deviceId。"));

    QJsonArray array = root.value(QStringLiteral("points")).toArray();
    if (array.isEmpty())
        array = root.value(QStringLiteral("variables")).toArray();
    if (array.isEmpty())
        array.append(root);
    if (array.size() > 500)
        return fail(QStringLiteral("单条远程消息变量数超过 500。"));

    QVector<BusinessDataPoint> parsed;
    parsed.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        const QString variableId = object.value(QStringLiteral("variableId")).toString(
            object.value(QStringLiteral("id")).toString()).trimmed();
        const QJsonValue numericValue = object.value(QStringLiteral("value"));
        if (variableId.isEmpty() || !numericValue.isDouble())
            continue;
        const double number = numericValue.toDouble();
        if (!qIsFinite(number))
            continue;

        BusinessDataPoint point;
        point.deviceId = deviceId;
        point.variableId = variableId;
        point.name = object.value(QStringLiteral("name")).toString(variableId);
        point.value = number;
        point.unit = object.value(QStringLiteral("unit")).toString();
        point.timestamp = QDateTime::fromString(
            object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
        if (!point.timestamp.isValid())
            point.timestamp = QDateTime::fromString(
                object.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        if (!point.timestamp.isValid())
            point.timestamp = QDateTime::currentDateTime();
        const QString quality = object.value(QStringLiteral("quality")).toString();
        point.quality = quality.compare(QStringLiteral("Bad"), Qt::CaseInsensitive) == 0
            ? DataQuality::Bad
            : quality.compare(QStringLiteral("Uncertain"), Qt::CaseInsensitive) == 0
                ? DataQuality::Uncertain : DataQuality::Good;
        point.sourceProtocol = protocol
            + (channel.isEmpty() ? QString() : QStringLiteral(":") + channel);
        parsed.append(point);
    }
    if (parsed.isEmpty())
        return fail(QStringLiteral("JSON 中没有有效业务变量（需要 variableId/id 和数值 value）。"));

    points = parsed;
    if (errorMessage) errorMessage->clear();
    return true;
}
