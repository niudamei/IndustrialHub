#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QUuid>

#include "src/communication/communicationmanager.h"
#include "src/core/datamanager.h"
#include "src/protocol/businessdatapoint.h"
#include <QtMath>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IndustrialHubDataManagerTest"));
    QStandardPaths::setTestModeEnabled(true);

    CommunicationManager communicationManager;
    DataManager dataManager(&communicationManager);
    if (!dataManager.isReady())
        return 1;

    const QString deviceId = QStringLiteral("HISTORY-TEST-%1")
                                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QByteArray txPayload = QByteArray::fromHex("010203AABB");
    const QByteArray rxPayload = QByteArray::fromHex("10203040");

    emit communicationManager.deviceDataSent(deviceId, txPayload);
    emit communicationManager.deviceDataReceived(deviceId, rxPayload);
    QVector<BusinessDataPoint> businessPoints;
    BusinessDataPoint temperature;
    temperature.deviceId = deviceId;
    temperature.variableId = QStringLiteral("temperature");
    temperature.name = QStringLiteral("温度");
    temperature.value = 42.5;
    temperature.unit = QStringLiteral("°C");
    temperature.timestamp = QDateTime::currentDateTime();
    temperature.quality = DataQuality::Good;
    temperature.sourceProtocol = QStringLiteral("TEST");
    businessPoints.append(temperature);

    BusinessDataPoint rpm = temperature;
    rpm.variableId = QStringLiteral("rpm");
    rpm.name = QStringLiteral("转速");
    rpm.value = 1250.0;
    rpm.unit = QStringLiteral("rpm");
    businessPoints.append(rpm);

    dataManager.appendBusinessDataPoints(deviceId, businessPoints);
    dataManager.flushPendingRecords();

    QList<CommunicationHistoryRecord> records;
    int totalCount = 0;
    const QDateTime start = QDateTime::currentDateTime().addSecs(-60);
    const QDateTime end = QDateTime::currentDateTime().addSecs(60);
    QString error;

    if (!dataManager.queryCommunicationHistory(
            start, end, deviceId, QString(), 0, 20,
            records, totalCount, &error))
        return 2;
    if (totalCount != 2 || records.size() != 2)
        return 3;

    if (!dataManager.queryCommunicationHistory(
            start, end, deviceId, QString(), 0, 1,
            records, totalCount, &error))
        return 7;
    if (totalCount != 2 || records.size() != 1)
        return 8;

    // 翻页时复用已统计的总数，确保查询不会重新执行 COUNT(*)。
    totalCount = 2;
    if (!dataManager.queryCommunicationHistory(
            start, end, deviceId, QString(), 1, 1,
            records, totalCount, &error, false))
        return 16;
    if (totalCount != 2 || records.size() != 1)
        return 17;

    if (!dataManager.queryCommunicationHistory(
            start, end, deviceId, QStringLiteral("RX"), 0, 20,
            records, totalCount, &error))
        return 4;
    if (totalCount != 1 || records.size() != 1)
        return 5;
    if (records.first().direction != QStringLiteral("RX")
        || records.first().payload != rxPayload)
        return 6;

    QList<BusinessDataHistoryRecord> businessRecords;
    int businessCount = 0;
    if (!dataManager.queryBusinessDataHistory(
            start, end, deviceId, QString(), 0, 20,
            businessRecords, businessCount, &error))
        return 9;
    if (businessCount != 2 || businessRecords.size() != 2)
        return 10;

    if (!dataManager.queryBusinessDataHistory(
            start, end, deviceId, QStringLiteral("temperature"), 0, 20,
            businessRecords, businessCount, &error))
        return 11;
    if (businessCount != 1 || businessRecords.size() != 1)
        return 12;
    if (businessRecords.first().point.variableId != QStringLiteral("temperature")
        || qAbs(businessRecords.first().point.value - 42.5) > 0.001
        || businessRecords.first().point.quality != DataQuality::Good
        || businessRecords.first().point.sourceProtocol != QStringLiteral("TEST"))
        return 13;

    if (!dataManager.queryBusinessDataHistory(
            start, end, deviceId, QString(), 0, 1,
            businessRecords, businessCount, &error))
        return 14;
    if (businessCount != 2 || businessRecords.size() != 1)
        return 15;

    businessCount = 2;
    if (!dataManager.queryBusinessDataHistory(
            start, end, deviceId, QString(), 1, 1,
            businessRecords, businessCount, &error, false))
        return 18;
    if (businessCount != 2 || businessRecords.size() != 1)
        return 19;

    return 0;
}
