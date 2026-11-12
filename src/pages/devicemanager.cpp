#include "devicemanager.h"
#include <QDateTime>

DeviceManager::DeviceManager(QObject *parent) : QObject(parent) {}

QList<DeviceInfo> DeviceManager::getAllDevices() const { return m_devices; }

bool DeviceManager::getDeviceById(const QString &id, DeviceInfo &device) const
{
    const int i = indexOf(id.trimmed());
    if (i < 0) return false;
    device = m_devices.at(i);
    return true;
}

bool DeviceManager::addDevice(const DeviceInfo &device, QString *error)
{
    DeviceInfo d = device;
    d.devId = d.devId.trimmed();
    d.devName = d.devName.trimmed();
    d.devType = d.devType.trimmed();
    d.commType = d.commType.trimmed();
    d.devAddr = d.devAddr.trimmed();
    if (d.devId.isEmpty() || d.devName.isEmpty()) {
        if (error) *error = QStringLiteral("设备编号和设备名称不能为空。");
        return false;
    }
    if (indexOf(d.devId) >= 0) {
        if (error) *error = QStringLiteral("设备编号已存在：%1").arg(d.devId);
        return false;
    }
    if (!d.addTime.isValid()) d.addTime = QDateTime::currentDateTime();
    m_devices.append(d);
    emit deviceAdded(d);
    emit devicesChanged();
    return true;
}

bool DeviceManager::updateDevice(const DeviceInfo &device, QString *error)
{
    DeviceInfo d = device;
    d.devId = d.devId.trimmed();
    d.devName = d.devName.trimmed();
    d.devType = d.devType.trimmed();
    d.commType = d.commType.trimmed();
    d.devAddr = d.devAddr.trimmed();
    if (d.devId.isEmpty() || d.devName.isEmpty()) {
        if (error) *error = QStringLiteral("设备编号和设备名称不能为空。");
        return false;
    }
    const int i = indexOf(d.devId);
    if (i < 0) {
        if (error) *error = QStringLiteral("设备不存在，无法更新。");
        return false;
    }
    if (!d.addTime.isValid()) d.addTime = m_devices.at(i).addTime;
    m_devices[i] = d;
    emit deviceUpdated(d);
    emit devicesChanged();
    return true;
}

bool DeviceManager::removeDevice(const QString &id, QString *error)
{
    const QString key = id.trimmed();
    const int i = indexOf(key);
    if (i < 0) {
        if (error) *error = QStringLiteral("设备不存在，无法删除。");
        return false;
    }
    m_devices.removeAt(i);
    emit deviceRemoved(key);
    emit devicesChanged();
    return true;
}

bool DeviceManager::updateDeviceStatus(const QString &id, DeviceStatus status, QString *error)
{
    const int i = indexOf(id.trimmed());
    if (i < 0) {
        if (error) *error = QStringLiteral("设备不存在，无法修改状态。");
        return false;
    }
    if (m_devices[i].status == status) return true;
    m_devices[i].status = status;
    emit deviceStatusChanged(m_devices.at(i).devId, status);
    emit deviceUpdated(m_devices.at(i));
    emit devicesChanged();
    return true;
}

int DeviceManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_devices.size(); ++i)
        if (m_devices.at(i).devId.compare(id, Qt::CaseInsensitive) == 0) return i;
    return -1;
}
