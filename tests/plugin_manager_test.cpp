#include "src/plugins/pluginmanager.h"
#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <functional>

static bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return condition();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qCritical() << "FAIL: plugin file path argument missing.";
        return 1;
    }

    CommunicationManager communicationManager;
    PluginManager manager;
    manager.setCommunicationManager(&communicationManager);
    QString error;
    if (!manager.loadPlugin(QString::fromLocal8Bit(argv[1]), &error)) {
        qCritical() << "FAIL: plugin load:" << error;
        return 2;
    }
    const QList<LoadedPluginInfo> loaded = manager.plugins();
    if (loaded.size() != 1 || loaded.first().pluginId != QStringLiteral("test.sample")
        || loaded.first().version != QStringLiteral("1.1.0")
        || loaded.first().communicationTypes != QStringList{QStringLiteral("PLUGIN-ECHO")}) {
        qCritical() << "FAIL: plugin metadata or driver registration mismatch.";
        return 3;
    }
    if (!communicationManager.registeredCommunicationTypes().contains(
            QStringLiteral("PLUGIN-ECHO"))) {
        qCritical() << "FAIL: communication factory was not registered.";
        return 4;
    }
    if (manager.loadPlugin(QString::fromLocal8Bit(argv[1]), &error)) {
        qCritical() << "FAIL: duplicate plugin path was accepted.";
        return 5;
    }

    if (communicationManager.registerFactory(QStringLiteral("PLUGIN-ECHO"),
            [](QObject *) -> ICommunication * { return nullptr; })) {
        qCritical() << "FAIL: a plugin must not replace an already registered driver.";
        return 6;
    }

    DeviceInfo device(QStringLiteral("PLUGIN-TEST-01"), QStringLiteral("插件回环设备"),
                      QStringLiteral("Test"), QStringLiteral("PLUGIN-ECHO"),
                      QStringLiteral("loopback"), DeviceStatus::Offline);
    if (!communicationManager.connectDevice(device, &error)) {
        qCritical() << "FAIL: plugin communication driver open:" << error;
        return 6;
    }
    QByteArray pluginRx;
    QObject::connect(&communicationManager, &CommunicationManager::deviceDataReceived,
                     [&](const QString &deviceId, const QByteArray &data) {
        if (deviceId == device.devId)
            pluginRx.append(data);
    });
    const QByteArray payload = QByteArray::fromHex("DEADBEEF");
    if (!communicationManager.sendData(device.devId, payload, &error)
        || !waitUntil([&]() { return pluginRx == payload; }, 1000)) {
        qCritical() << "FAIL: plugin driver TX/RX loopback:" << error;
        return 7;
    }

    communicationManager.disconnectDevice(device.devId);
    if (manager.unloadPlugin(QStringLiteral("test.sample"), &error)) {
        qCritical() << "FAIL: plugin unloaded while its device worker still referenced the driver.";
        return 8;
    }
    if (error.isEmpty()) {
        qCritical() << "FAIL: busy plugin unload did not explain the reason.";
        return 9;
    }

    communicationManager.releaseDevice(device.devId);
    if (!manager.unloadPlugin(QStringLiteral("test.sample"), &error)) {
        qCritical() << "FAIL: plugin unload after device release:" << error;
        return 10;
    }
    if (!manager.plugins().isEmpty()
        || communicationManager.registeredCommunicationTypes().contains(
            QStringLiteral("PLUGIN-ECHO"))) {
        qCritical() << "FAIL: plugin or driver factory remains after unload.";
        return 11;
    }
    qInfo() << "PASS: plugin load, driver registration, active-use unload guard and clean unload.";
    return 0;
}
