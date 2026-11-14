#include "src/remote/remoteconfigstore.h"
#include "src/remote/remotetelemetryparser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString fileName = QDir::tempPath()
        + QStringLiteral("/industrialhub_remote_mapping_test.ini");
    QFile::remove(fileName);

    RemoteConfigStore store(fileName);
    RemoteTelemetryMapping mapping;
    mapping.protocol = QStringLiteral("MQTT");
    mapping.channelPattern = QStringLiteral("factory/+/telemetry/#");
    mapping.deviceId = QStringLiteral("PLC-001");
    mapping.variableId = QStringLiteral("temperature");
    mapping.name = QStringLiteral("温度");
    mapping.unit = QStringLiteral("°C");
    QString error;
    if (!store.upsertMapping(mapping, &error)) {
        qCritical() << "FAIL: save mapping:" << error;
        return 1;
    }

    RemoteTelemetryMapping invalid = mapping;
    invalid.channelPattern = QStringLiteral("factory/#/temperature");
    if (store.upsertMapping(invalid, &error)) {
        qCritical() << "FAIL: invalid MQTT multi-level wildcard placement was accepted.";
        return 5;
    }

    RemoteConfigStore reopened(fileName);
    RemoteTelemetryMapping resolved;
    if (!reopened.resolveMapping(QStringLiteral("mqtt"),
                                 QStringLiteral("factory/line1/telemetry/temperature"), resolved)
        || resolved.deviceId != QStringLiteral("PLC-001")
        || resolved.variableId != QStringLiteral("temperature")) {
        qCritical() << "FAIL: wildcard mapping persistence/resolution.";
        return 2;
    }

    QString deviceId;
    QVector<BusinessDataPoint> points;
    const QByteArray payload = QByteArrayLiteral("{\"value\":23.8}");
    if (!RemoteTelemetryParser::parse(QStringLiteral("MQTT"),
            QStringLiteral("factory/line1/telemetry"), payload, deviceId, points, &error,
            resolved.deviceId, resolved.variableId, resolved.name, resolved.unit)
        || deviceId != QStringLiteral("PLC-001") || points.size() != 1
        || points.first().variableId != QStringLiteral("temperature")
        || points.first().name != QStringLiteral("温度")
        || points.first().unit != QStringLiteral("°C")) {
        qCritical() << "FAIL: parser did not apply channel mapping:" << error;
        return 3;
    }

    if (!reopened.removeMapping(QStringLiteral("MQTT"),
                                QStringLiteral("factory/line1/*"), &error)
        || !reopened.mappings().isEmpty()) {
        qCritical() << "FAIL: remove mapping:" << error;
        return 4;
    }
    QFile::remove(fileName);
    qInfo() << "PASS: remote mapping persistence, wildcard lookup and telemetry defaults.";
    return 0;
}
