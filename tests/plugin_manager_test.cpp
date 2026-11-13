#include "src/plugins/pluginmanager.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        qCritical() << "FAIL: plugin file path argument missing.";
        return 1;
    }

    PluginManager manager;
    QString error;
    if (!manager.loadPlugin(QString::fromLocal8Bit(argv[1]), &error)) {
        qCritical() << "FAIL: plugin load:" << error;
        return 2;
    }
    const QList<LoadedPluginInfo> loaded = manager.plugins();
    if (loaded.size() != 1 || loaded.first().pluginId != QStringLiteral("test.sample")
        || loaded.first().version != QStringLiteral("1.0.0")) {
        qCritical() << "FAIL: plugin metadata mismatch.";
        return 3;
    }
    if (manager.loadPlugin(QString::fromLocal8Bit(argv[1]), &error)) {
        qCritical() << "FAIL: duplicate plugin path was accepted.";
        return 4;
    }
    if (!manager.unloadPlugin(QStringLiteral("test.sample"), &error)) {
        qCritical() << "FAIL: plugin unload:" << error;
        return 5;
    }
    if (!manager.plugins().isEmpty()) {
        qCritical() << "FAIL: plugin remains registered after unload.";
        return 6;
    }
    qInfo() << "PASS: dynamic plugin load, metadata, duplicate rejection and unload.";
    return 0;
}
