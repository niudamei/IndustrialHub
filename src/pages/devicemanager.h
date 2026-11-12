#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include "deviceinfo.h"
#include "src/database/devicedatabase.h"

// 设备数据的唯一管理入口。页面不直接持有或修改设备列表。
// 所有增删改操作先写入 SQLite，成功后再更新内存数据并发出信号。
class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    QList<DeviceInfo> getAllDevices() const;
    bool getDeviceById(const QString &devId, DeviceInfo &device) const;

    bool isDatabaseReady() const;
    QString databasePath() const;
    QString lastError() const;

    bool addDevice(const DeviceInfo &device, QString *errorMessage = nullptr);
    bool updateDevice(const DeviceInfo &device, QString *errorMessage = nullptr);
    bool removeDevice(const QString &devId, QString *errorMessage = nullptr);
    bool updateDeviceStatus(const QString &devId,
                            DeviceStatus status,
                            QString *errorMessage = nullptr);

signals:
    void devicesChanged();
    void deviceAdded(const DeviceInfo &device);
    void deviceUpdated(const DeviceInfo &device);
    void deviceRemoved(const QString &devId);
    void deviceStatusChanged(const QString &devId, DeviceStatus status);

private:
    int indexOf(const QString &devId) const;
    bool setError(const QString &message, QString *errorMessage);

    DeviceDatabase m_database;
    QList<DeviceInfo> m_devices;
    bool m_databaseReady = false;
    QString m_lastError;
};

#endif // DEVICEMANAGER_H
