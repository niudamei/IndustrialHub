#include "pluginmanager.h"

#include <QFileInfo>
#include <QPluginLoader>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    unloadAll();
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
    QString initializeError;
    if (!plugin->initialize(&initializeError)) {
        loader->unload();
        delete loader;
        return fail(initializeError.isEmpty()
            ? QStringLiteral("插件初始化失败：%1").arg(id) : initializeError);
    }

    LoadedPlugin loaded;
    loaded.info.pluginId = id;
    loaded.info.displayName = plugin->displayName();
    loaded.info.version = plugin->version();
    loaded.info.filePath = absolutePath;
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
        LoadedPlugin loaded = m_plugins.takeAt(i);
        if (loaded.instance)
            loaded.instance->shutdown();
        const bool unloaded = loaded.loader && loaded.loader->unload();
        const QString reason = loaded.loader ? loaded.loader->errorString() : QString();
        delete loaded.loader;
        emit pluginsChanged();
        if (!unloaded) {
            if (errorMessage) *errorMessage = QStringLiteral("插件卸载失败：%1").arg(reason);
            return false;
        }
        if (errorMessage) errorMessage->clear();
        return true;
    }
    if (errorMessage) *errorMessage = QStringLiteral("未找到已加载插件：%1").arg(pluginId);
    return false;
}

void PluginManager::unloadAll()
{
    while (!m_plugins.isEmpty())
        unloadPlugin(m_plugins.last().info.pluginId);
}

QList<LoadedPluginInfo> PluginManager::plugins() const
{
    QList<LoadedPluginInfo> result;
    result.reserve(m_plugins.size());
    for (const LoadedPlugin &plugin : m_plugins)
        result.append(plugin.info);
    return result;
}
