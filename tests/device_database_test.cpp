#include <QCoreApplication>
#include <QStandardPaths>
#include <QUuid>

#include "src/database/devicedatabase.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("IndustrialHubDeviceDatabaseTest"));
    QStandardPaths::setTestModeEnabled(true);

    DeviceDatabase database;
    QString error;
    if (!database.initialize(&error)) return 1;

    DeviceInfo device;
    device.devId = QStringLiteral("DEVICE-TEST-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    device.devName = QStringLiteral("测试设备");
    device.devType = QStringLiteral("伺服电机");
    device.commType = QStringLiteral("CANFD");
    device.devAddr = QStringLiteral("COM9,1000000,2000000");
    device.groupName = QStringLiteral("产线一");
    device.enabled = false;
    device.status = DeviceStatus::Offline;
    device.addTime = QDateTime::currentDateTime();

    if (!database.insert(device, &error)) return 2;
    QList<DeviceInfo> devices;
    if (!database.loadAll(devices, &error)) return 3;
    bool found = false;
    for (const DeviceInfo &loaded : devices) {
        if (loaded.devId.compare(device.devId, Qt::CaseInsensitive) == 0) {
            found = loaded.groupName == QStringLiteral("产线一")
                && !loaded.enabled
                && loaded.commType == QStringLiteral("CANFD")
                && loaded.devAddr == QStringLiteral("COM9,1000000,2000000");
            break;
        }
    }
    if (!found) return 4;

    device.groupName = QStringLiteral("测试台");
    device.enabled = true;
    if (!database.update(device, &error)) return 5;
    if (!database.loadAll(devices, &error)) return 6;
    found = false;
    for (const DeviceInfo &loaded : devices) {
        if (loaded.devId.compare(device.devId, Qt::CaseInsensitive) == 0) {
            found = loaded.groupName == QStringLiteral("测试台") && loaded.enabled;
            break;
        }
    }
    if (!found) return 7;
    if (!database.remove(device.devId, &error)) return 8;
    return 0;
}
