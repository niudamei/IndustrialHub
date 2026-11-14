#ifndef IINDUSTRIALPLUGIN_H
#define IINDUSTRIALPLUGIN_H

#include "src/communication/icommunication.h"

#include <QString>
#include <QStringList>
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

// Optional capability: a plugin may contribute one or more ICommunication drivers.
// createCommunication() is called in the device I/O worker thread.
class ICommunicationDriverPlugin
{
public:
    virtual ~ICommunicationDriverPlugin() = default;
    virtual QStringList communicationTypes() const = 0;
    virtual ICommunication *createCommunication(const QString &communicationType,
                                                QObject *parent) = 0;
};

#define INDUSTRIALHUB_COMMUNICATION_PLUGIN_IID "org.industrialhub.ICommunicationDriverPlugin/1.0"
Q_DECLARE_INTERFACE(ICommunicationDriverPlugin, INDUSTRIALHUB_COMMUNICATION_PLUGIN_IID)

#endif // IINDUSTRIALPLUGIN_H
