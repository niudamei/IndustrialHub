#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include "iplugin.h"

#include <QObject>
#include <QList>

class CommunicationManager;
class QPluginLoader;

struct LoadedPluginInfo
{
    QString pluginId;
    QString displayName;
    QString version;
    QString filePath;
    QStringList communicationTypes;
};

class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    void setCommunicationManager(CommunicationManager *manager);
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
        bool shutdownCalled = false;
    };
    CommunicationManager *m_communicationManager = nullptr;
    QList<LoadedPlugin> m_plugins;
};

#endif // PLUGINMANAGER_H
