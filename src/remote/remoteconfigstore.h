#ifndef REMOTECONFIGSTORE_H
#define REMOTECONFIGSTORE_H

#include <QList>
#include <QString>

struct RemoteTelemetryMapping
{
    QString protocol;
    QString channelPattern;
    QString deviceId;
    QString variableId;
    QString name;
    QString unit;
};

class RemoteConfigStore
{
public:
    explicit RemoteConfigStore(const QString &settingsFile = QString());
    QList<RemoteTelemetryMapping> mappings() const;
    bool upsertMapping(const RemoteTelemetryMapping &mapping, QString *errorMessage = nullptr);
    bool removeMapping(const QString &protocol, const QString &channelPattern,
                       QString *errorMessage = nullptr);
    bool resolveMapping(const QString &protocol, const QString &channel,
                        RemoteTelemetryMapping &mapping) const;
private:
    QString m_settingsFile;
};

#endif
