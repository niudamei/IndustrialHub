#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>

#include "src/communication/communicationmanager.h"
#include "src/core/datamanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IndustrialHubDataManagerTest"));
    QStandardPaths::setTestModeEnabled(true);

    CommunicationManager communicationManager;
    DataManager dataManager(&communicationManager);
    if (!dataManager.isReady())
        return 1;

    const QString deviceId = QStringLiteral("HISTORY-TEST-DEVICE");
    const QByteArray txPayload = QByteArray::fromHex("010203AABB");
    const QByteArray rxPayload = QByteArray::fromHex("10203040");

    emit communicationManager.deviceDataSent(deviceId, txPayload);
    emit communicationManager.deviceDataReceived(deviceId, rxPayload);
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
            start, end, deviceId, QStringLiteral("RX"), 0, 20,
            records, totalCount, &error))
        return 4;
    if (totalCount != 1 || records.size() != 1)
        return 5;
    if (records.first().direction != QStringLiteral("RX")
        || records.first().payload != rxPayload)
        return 6;

    return 0;
}
