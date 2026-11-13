#ifndef IINDUSTRIALPLUGIN_H
#define IINDUSTRIALPLUGIN_H

#include <QString>
#include <QtPlugin>

class IIndustrialPlugin
{
public:
    virtual ~IIndustrialPlugin() = default;
    virtual QString pluginId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString version() const = 0;
    virtual bool initialize(QString *errorMessage) = 0;
    virtual void shutdown() = 0;
};

#define INDUSTRIALHUB_PLUGIN_IID "org.industrialhub.IIndustrialPlugin/1.0"
Q_DECLARE_INTERFACE(IIndustrialPlugin, INDUSTRIALHUB_PLUGIN_IID)

#endif // IINDUSTRIALPLUGIN_H
