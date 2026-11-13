#ifndef COMMUNICATIONMANAGER_H
#define COMMUNICATIONMANAGER_H

#include "icommunication.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <functional>

class CommunicationWorker;

class CommunicationManager : public QObject
{
    Q_OBJECT

public:
    using CommunicationFactory = std::function<ICommunication *(QObject *parent)>;

    explicit CommunicationManager(QObject *parent = nullptr);
    ~CommunicationManager() override;

    bool registerFactory(const QString &commType, CommunicationFactory factory);
    // true 表示驱动已接受连接请求；TCP 远端握手结果通过状态信号异步返回。
    bool connectDevice(const DeviceInfo &device, QString *errorMessage = nullptr);
    void disconnectDevice(const QString &deviceId);
    void disconnectAll();

    // true 表示 TX 请求已进入设备工作线程队列；实际写入成功后发出 deviceDataSent。
    bool sendData(const QString &deviceId,
                  const QByteArray &data,
                  QString *errorMessage = nullptr);

    bool isConnected(const QString &deviceId) const;
    QString communicationType(const QString &deviceId) const;
    QStringList managedDeviceIds() const;
    void releaseDevice(const QString &deviceId);

signals:
    void deviceConnectionStateChanged(const QString &deviceId, bool connected);
    void deviceDataSent(const QString &deviceId, const QByteArray &data);
    void deviceDataReceived(const QString &deviceId, const QByteArray &data);
    void deviceErrorOccurred(const QString &deviceId, const QString &message);
    void deviceReconnecting(const QString &deviceId, int attempt, int delayMs);

private:
    static QString deviceKey(const QString &deviceId);
    void attachSignals(const QString &deviceId, CommunicationWorker *worker);
    bool setError(const QString &message, QString *errorMessage);
    void scheduleReconnect(const QString &key);
    void stopReconnect(const QString &key);
    void attemptReconnect(const QString &key);

    QHash<QString, CommunicationWorker *> m_workers;
    QHash<QString, QString> m_commTypes;
    QHash<QString, QString> m_deviceIds;
    QHash<QString, DeviceInfo> m_deviceConfigs;
    QHash<QString, QTimer *> m_reconnectTimers;
    QHash<QString, int> m_reconnectAttempts;
    QSet<QString> m_autoReconnectDevices;
    QSet<QString> m_connectedDevices;
    QHash<QString, CommunicationFactory> m_factories;
};

#endif // COMMUNICATIONMANAGER_H
