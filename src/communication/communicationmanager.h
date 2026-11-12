#ifndef COMMUNICATIONMANAGER_H
#define COMMUNICATIONMANAGER_H

#include "icommunication.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <functional>

// 管理多个设备各自的通信对象。
// 所有操作应在 CommunicationManager 所属线程中调用（通常是 GUI 主线程）。
class CommunicationManager : public QObject
{
    Q_OBJECT

public:
    using CommunicationFactory = std::function<ICommunication *(QObject *parent)>;

    explicit CommunicationManager(QObject *parent = nullptr);
    ~CommunicationManager() override;

    // 注册通信类型，例如 TCP、SIMULATED。可用于后续扩展 UDP、RS485、CAN。
    bool registerFactory(const QString &commType, CommunicationFactory factory);

    bool connectDevice(const DeviceInfo &device, QString *errorMessage = nullptr);
    void disconnectDevice(const QString &deviceId);
    void disconnectAll();

    bool sendData(const QString &deviceId,
                  const QByteArray &data,
                  QString *errorMessage = nullptr);

    bool isConnected(const QString &deviceId) const;
    QStringList managedDeviceIds() const;

    // 释放指定设备的通信对象；若已连接会先断开。
    void releaseDevice(const QString &deviceId);

signals:
    void deviceConnectionStateChanged(const QString &deviceId, bool connected);
    void deviceDataReceived(const QString &deviceId, const QByteArray &data);
    void deviceErrorOccurred(const QString &deviceId, const QString &message);

private:
    ICommunication *createCommunication(const QString &commType,
                                         QString *errorMessage);
    ICommunication *communicationFor(const QString &deviceId) const;
    void attachSignals(const QString &deviceId, ICommunication *communication);
    bool setError(const QString &message, QString *errorMessage);

    QHash<QString, ICommunication *> m_communications;
    QHash<QString, QString> m_commTypes;
    QHash<QString, CommunicationFactory> m_factories;
};

#endif // COMMUNICATIONMANAGER_H
