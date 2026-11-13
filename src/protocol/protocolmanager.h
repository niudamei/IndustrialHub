#ifndef PROTOCOLMANAGER_H
#define PROTOCOLMANAGER_H

#include "iprotocolparser.h"

#include <QHash>
#include <QObject>

#include <memory>

// 按设备绑定解析器，统一分发解析结果。
// 不会根据 CAN/TCP 等通信类型擅自推断设备私有协议。
class ProtocolManager : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolManager(QObject *parent = nullptr);

    bool registerParser(const QString &parserId,
                        std::shared_ptr<const IProtocolParser> parser,
                        QString *errorMessage = nullptr);
    bool bindDevice(const QString &deviceId,
                    const QString &parserId,
                    QString *errorMessage = nullptr);
    void unbindDevice(const QString &deviceId);

    QString parserForDevice(const QString &deviceId) const;
    bool parseFrame(const QString &deviceId,
                    const QByteArray &payload,
                    QString *errorMessage = nullptr);

signals:
    void businessDataDecoded(const QString &deviceId,
                             const QVector<BusinessDataPoint> &points);
    void parseFailed(const QString &deviceId, const QString &message);

private:
    static QString normalizedKey(const QString &value);
    bool setError(const QString &message, QString *errorMessage) const;

    QHash<QString, std::shared_ptr<const IProtocolParser>> m_parsers;
    QHash<QString, QString> m_deviceBindings;
};

#endif // PROTOCOLMANAGER_H
