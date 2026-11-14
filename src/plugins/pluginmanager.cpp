#include "pluginmanager.h"

#include "src/communication/communicationmanager.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>
#include <QDebug>
#include <QSet>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
}

void PluginManager::setCommunicationManager(CommunicationManager *manager)
{
    m_communicationManager = manager;
}

bool PluginManager::loadPlugin(const QString &filePath, QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
        return fail(QStringLiteral("插件文件不存在：%1").arg(filePath));
    const QString absolutePath = fileInfo.absoluteFilePath();
    for (const LoadedPlugin &loaded : m_plugins) {
        if (loaded.info.filePath.compare(absolutePath, Qt::CaseInsensitive) == 0)
            return fail(QStringLiteral("该插件已经加载。"));
    }

    auto *loader = new QPluginLoader(absolutePath, this);
    loader->setLoadHints(QLibrary::LoadHints());
    const QJsonObject metadata = loader->metaData();
    if (metadata.isEmpty()) {
        const QString reason = loader->errorString();
        delete loader;
        return fail(QStringLiteral("读取插件元数据失败：%1").arg(reason));
    }
    QObject *object = loader->instance();
    if (!object) {
        const QString reason = loader->errorString();
        delete loader;
        return fail(QStringLiteral("加载插件动态库失败：%1").arg(reason));
    }
    auto *plugin = qobject_cast<IIndustrialPlugin *>(object);
    if (!plugin) {
        loader->unload();
        delete loader;
        return fail(QStringLiteral("插件未实现 IndustrialHub 插件接口版本 1.0。"));
    }
    const QString id = plugin->pluginId().trimmed();
    if (id.isEmpty() || plugin->displayName().trimmed().isEmpty()) {
        loader->unload();
        delete loader;
        return fail(QStringLiteral("插件 ID 和显示名称不能为空。"));
    }
    for (const LoadedPlugin &loaded : m_plugins) {
        if (loaded.info.pluginId.compare(id, Qt::CaseInsensitive) == 0) {
            loader->unload();
            delete loader;
            return fail(QStringLiteral("插件 ID 已被占用：%1").arg(id));
        }
    }

    auto *driverPlugin = qobject_cast<ICommunicationDriverPlugin *>(object);
    QStringList driverTypes;
    if (driverPlugin) {
        if (!m_communicationManager) {
            loader->unload();
            delete loader;
            return fail(QStringLiteral("该插件提供通信驱动，但尚未绑定 CommunicationManager。"));
        }
        QSet<QString> uniqueTypes;
        for (const QString &rawType : driverPlugin->communicationTypes()) {
            const QString type = rawType.trimmed().toUpper();
            if (type.isEmpty() || uniqueTypes.contains(type)) {
                loader->unload();
                delete loader;
                return fail(QStringLiteral("插件声明了空或重复的通信类型。"));
            }
            uniqueTypes.insert(type);
            driverTypes.append(type);
        }
        if (driverTypes.isEmpty()) {
            loader->unload();
            delete loader;
            return fail(QStringLiteral("通信驱动插件至少需要声明一种通信类型。"));
        }
    }

    QString initializeError;
    if (!plugin->initialize(&initializeError)) {
        loader->unload();
        delete loader;
        return fail(initializeError.isEmpty()
            ? QStringLiteral("插件初始化失败：%1").arg(id) : initializeError);
    }

    QStringList registeredTypes;
    if (driverPlugin) {
        for (const QString &type : driverTypes) {
            const bool registered = m_communicationManager->registerFactory(
                type, [driverPlugin, type](QObject *parent) {
                    return driverPlugin->createCommunication(type, parent);
                });
            if (!registered) {
                QString rollbackError;
                if (!registeredTypes.isEmpty())
                    m_communicationManager->unregisterFactories(registeredTypes, &rollbackError);
                plugin->shutdown();
                loader->unload();
                delete loader;
                return fail(QStringLiteral("注册通信类型 %1 失败：%2")
                    .arg(type, rollbackError.isEmpty()
                        ? QStringLiteral("类型已存在或工厂无效") : rollbackError));
            }
            registeredTypes.append(type);
        }
    }

    LoadedPlugin loaded;
    loaded.info.pluginId = id;
    loaded.info.displayName = plugin->displayName();
    loaded.info.version = plugin->version();
    loaded.info.filePath = absolutePath;
    loaded.info.communicationTypes = registeredTypes;
    loaded.loader = loader;
    loaded.instance = plugin;
    m_plugins.append(loaded);
    if (errorMessage) errorMessage->clear();
    emit pluginsChanged();
    return true;
}

bool PluginManager::unloadPlugin(const QString &pluginId, QString *errorMessage)
{
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (m_plugins.at(i).info.pluginId.compare(pluginId, Qt::CaseInsensitive) != 0)
            continue;
        LoadedPlugin &loaded = m_plugins[i];
        if (!loaded.info.communicationTypes.isEmpty()) {
            QString unregisterError;
            if (!m_communicationManager
                || !m_communicationManager->unregisterFactories(
                    loaded.info.communicationTypes, &unregisterError)) {
                if (errorMessage) *errorMessage = unregisterError.isEmpty()
                    ? QStringLiteral("通信驱动仍被引用，不能卸载插件。") : unregisterError;
                return false;
            }
        }
        if (loaded.instance && !loaded.shutdownCalled) {
            loaded.instance->shutdown();
            loaded.shutdownCalled = true;
        }
        if (!loaded.loader || !loaded.loader->unload()) {
            if (errorMessage) *errorMessage = QStringLiteral("插件卸载失败：%1")
                .arg(loaded.loader ? loaded.loader->errorString()
                                   : QStringLiteral("无效的插件加载器"));
            return false;
        }
        QPluginLoader *loader = loaded.loader;
        m_plugins.removeAt(i);
        delete loader;
        emit pluginsChanged();
        if (errorMessage) errorMessage->clear();
        return true;
    }
    if (errorMessage) *errorMessage = QStringLiteral("未找到已加载插件：%1").arg(pluginId);
    return false;
}

void PluginManager::unloadAll()
{
    while (!m_plugins.isEmpty()) {
        const QString id = m_plugins.last().info.pluginId;
        QString error;
        if (!unloadPlugin(id, &error)) {
            qWarning() << "无法卸载插件" << id << error;
            break;
        }
    }
}

QList<LoadedPluginInfo> PluginManager::plugins() const
{
    QList<LoadedPluginInfo> result;
    result.reserve(m_plugins.size());
    for (const LoadedPlugin &plugin : m_plugins)
        result.append(plugin.info);
    return result;
}
