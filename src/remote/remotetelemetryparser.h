#ifndef REMOTETELEMETRYPARSER_H
#define REMOTETELEMETRYPARSER_H

#include "src/protocol/businessdatapoint.h"

#include <QByteArray>
#include <QString>
#include <QVector>

class RemoteTelemetryParser
{
public:
    static bool parse(const QString &protocol, const QString &channel,
                      const QByteArray &payload, QString &deviceId,
                      QVector<BusinessDataPoint> &points,
                      QString *errorMessage = nullptr,
                      const QString &fallbackDeviceId = QString(),
                      const QString &fallbackVariableId = QString(),
                      const QString &fallbackName = QString(),
                      const QString &fallbackUnit = QString());
};

#endif // REMOTETELEMETRYPARSER_H
