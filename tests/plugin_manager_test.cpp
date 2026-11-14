#include "src/plugins/pluginmanager.h"
#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QDebug>

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

    DeviceInfo device(QStringLiteral("PLUGIN-TEST-01"), QStringLiteral("插件回环设备"),
                      QStringLiteral("Test"), QStringLiteral("PLUGIN-ECHO"),
                      QStringLiteral("loopback"), DeviceStatus::Offline);
    if (!communicationManager.connectDevice(device, &error)) {
        qCritical() << "FAIL: plugin communication driver open:" << error;
        return 6;
    }
    if (manager.unloadPlugin(QStringLiteral("test.sample"), &error)) {
        qCritical() << "FAIL: plugin unloaded while its driver was still in use.";
        return 7;
    }
    if (error.isEmpty()) {
        qCritical() << "FAIL: busy plugin unload did not explain the reason.";
        return 8;
    }

    communicationManager.releaseDevice(device.devId);
    if (!manager.unloadPlugin(QStringLiteral("test.sample"), &error)) {
        qCritical() << "FAIL: plugin unload after device release:" << error;
        return 9;
    }
    if (!manager.plugins().isEmpty()
        || communicationManager.registeredCommunicationTypes().contains(
            QStringLiteral("PLUGIN-ECHO"))) {
        qCritical() << "FAIL: plugin or driver factory remains after unload.";
        return 10;
    }
    qInfo() << "PASS: plugin load, driver registration, active-use unload guard and clean unload.";
    return 0;
}
