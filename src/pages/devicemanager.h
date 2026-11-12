#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include "deviceinfo.h"

// 设备数据的唯一管理入口。页面不直接持有或修改设备列表。
class DeviceManager : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);

    QList<DeviceInfo> getAllDevices() const;
    bool getDeviceById(const QString &devId, DeviceInfo &device) const;

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
    QList<DeviceInfo> m_devices;
};

#endif // DEVICEMANAGER_H
