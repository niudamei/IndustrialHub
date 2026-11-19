#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

#include <iostream>

#include "src/database/devicedatabase.h"
#include "src/pages/devicemanager.h"

namespace {
DeviceInfo makeDevice(const QString &id, const QString &name)
{
    DeviceInfo device;
    device.devId = id;
    device.devName = name;
    device.devType = QStringLiteral("测试设备");
    device.commType = QStringLiteral("TCP");
    device.devAddr = QStringLiteral("127.0.0.1:9000");
    device.status = DeviceStatus::Offline;
    device.addTime = QDateTime::currentDateTime();
    return device;
}

int fail(const char *message, const QString &detail = {})
{
    std::cerr << "FAIL: " << message;
    if (!detail.isEmpty())
        std::cerr << " - " << detail.toStdString();
    std::cerr << std::endl;
    return 1;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IndustrialHubTests"));
    QCoreApplication::setApplicationName(QStringLiteral("DeviceManagerReservedIds"));
    QStandardPaths::setTestModeEnabled(true);

    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDirectory.isEmpty())
        QDir(dataDirectory).removeRecursively();

    // Seed a legacy database row to verify it remains manageable and is not deleted.
    {
        DeviceDatabase legacyDatabase;
        QString error;
        if (!legacyDatabase.initialize(&error))
            return fail("legacy database initialization", error);

        DeviceInfo legacy = makeDevice(QStringLiteral("DEMO-CAN-001"),
                                       QStringLiteral("旧版用户设备"));
        if (!legacyDatabase.insert(legacy, &error))
            return fail("legacy reserved-ID seed", error);
    }

    DeviceManager manager;
    if (!manager.isDatabaseReady())
        return fail("device manager initialization", manager.lastError());

    DeviceInfo legacy;
    if (!manager.getDeviceById(QStringLiteral("DEMO-CAN-001"), legacy))
        return fail("legacy reserved-ID row was not loaded");

    const QStringList reservedIds = {
        QStringLiteral("DEMO-CAN-001"),
        QStringLiteral("DEMO-CANFD-002"),
        QStringLiteral("DEMO-TCP-003"),
        QStringLiteral(" demo-can-001 ")
    };
    for (const QString &id : reservedIds) {
        QString error;
        if (manager.addDevice(makeDevice(id, QStringLiteral("不应注册")), &error))
            return fail("reserved demo ID was accepted", id);
        if (!error.contains(QStringLiteral("保留")))
            return fail("reserved ID rejection did not explain the rule", error);
    }

    // Legacy rows can still be edited and removed by the owner.
    legacy.devName = QStringLiteral("旧版用户设备（已保留）");
    QString error;
    if (!manager.updateDevice(legacy, &error))
        return fail("legacy reserved-ID row cannot be edited", error);

    DeviceInfo updated;
    if (!manager.getDeviceById(QStringLiteral("DEMO-CAN-001"), updated)
        || updated.devName != QStringLiteral("旧版用户设备（已保留）")) {
        return fail("legacy reserved-ID row changed unexpectedly");
    }

    if (!manager.removeDevice(QStringLiteral("DEMO-CAN-001"), &error))
        return fail("legacy reserved-ID row cannot be removed manually", error);

    if (manager.addDevice(makeDevice(QStringLiteral("DEMO-CAN-001"),
                                     QStringLiteral("仍不允许注册")), &error)) {
        return fail("reserved ID was accepted after legacy row removal");
    }

    const QString ordinaryId = QStringLiteral("DEVICE-TEST-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!manager.addDevice(makeDevice(ordinaryId, QStringLiteral("普通测试设备")), &error))
        return fail("ordinary device registration was rejected", error);
    if (!manager.removeDevice(ordinaryId, &error))
        return fail("ordinary test device cleanup failed", error);

    std::cout << "ALL DEVICE MANAGER RESERVED-ID TESTS PASSED." << std::endl;
    return 0;
}
