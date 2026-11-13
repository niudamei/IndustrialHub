#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "iplugin.h"

#include <QObject>
#include <QList>

class QPluginLoader;

struct LoadedPluginInfo
{
    QString pluginId;
    QString displayName;
    QString version;
    QString filePath;
};

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    bool loadPlugin(const QString &filePath, QString *errorMessage = nullptr);
    bool unloadPlugin(const QString &pluginId, QString *errorMessage = nullptr);
    void unloadAll();
    QList<LoadedPluginInfo> plugins() const;

signals:
    void pluginsChanged();

private:
    struct LoadedPlugin
    {
        LoadedPluginInfo info;
        QPluginLoader *loader = nullptr;
        IIndustrialPlugin *instance = nullptr;
    };
    QList<LoadedPlugin> m_plugins;
};

#endif // PLUGINMANAGER_H
