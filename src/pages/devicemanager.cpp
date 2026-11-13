#include "devicemanager.h"
#include <QDateTime>

DeviceManager::DeviceManager(QObject *parent) : QObject(parent)
{
    if (!m_database.initialize(&m_lastError))
        return;

    if (!m_database.loadAll(m_devices, &m_lastError))
        return;

    m_databaseReady = true;
}

QList<DeviceInfo> DeviceManager::getAllDevices() const
{
    return m_devices;
}

bool DeviceManager::getDeviceById(const QString &id, DeviceInfo &device) const
{
    const int i = indexOf(id.trimmed());
    if (i < 0)
        return false;

    device = m_devices.at(i);
    return true;
}

bool DeviceManager::isDatabaseReady() const
{
    return m_databaseReady;
}

QString DeviceManager::databasePath() const
{
    return m_database.databasePath();
}

QString DeviceManager::lastError() const
{
    return m_lastError;
}

bool DeviceManager::addDevice(const DeviceInfo &device, QString *error)
{
    if (!m_databaseReady)
        return setError(QStringLiteral("数据库不可用：%1").arg(m_lastError), error);

    DeviceInfo d = device;
    d.devId = d.devId.trimmed();
    d.devName = d.devName.trimmed();
    d.devType = d.devType.trimmed();
    d.commType = d.commType.trimmed();
    d.devAddr = d.devAddr.trimmed();
    d.groupName = d.groupName.trimmed();

    if (d.devId.isEmpty() || d.devName.isEmpty())
        return setError(QStringLiteral("设备编号和设备名称不能为空。"), error);

    if (indexOf(d.devId) >= 0)
        return setError(QStringLiteral("设备编号已存在：%1").arg(d.devId), error);

    if (!d.addTime.isValid())
        d.addTime = QDateTime::currentDateTime();

    if (!m_database.insert(d, &m_lastError))
        return setError(m_lastError, error);

    m_devices.append(d);
    m_lastError.clear();
    emit deviceAdded(d);
    emit devicesChanged();
    return true;
}

bool DeviceManager::updateDevice(const DeviceInfo &device, QString *error)
{
    if (!m_databaseReady)
        return setError(QStringLiteral("数据库不可用：%1").arg(m_lastError), error);

    DeviceInfo d = device;
    d.devId = d.devId.trimmed();
    d.devName = d.devName.trimmed();
    d.devType = d.devType.trimmed();
    d.commType = d.commType.trimmed();
    d.devAddr = d.devAddr.trimmed();

    if (d.devId.isEmpty() || d.devName.isEmpty())
        return setError(QStringLiteral("设备编号和设备名称不能为空。"), error);

    const int i = indexOf(d.devId);
    if (i < 0)
        return setError(QStringLiteral("设备不存在，无法更新。"), error);

    if (!d.addTime.isValid())
        d.addTime = m_devices.at(i).addTime;

    if (!m_database.update(d, &m_lastError))
        return setError(m_lastError, error);

    m_devices[i] = d;
    m_lastError.clear();
    emit deviceUpdated(d);
    emit devicesChanged();
    return true;
}

bool DeviceManager::removeDevice(const QString &id, QString *error)
{
    if (!m_databaseReady)
        return setError(QStringLiteral("数据库不可用：%1").arg(m_lastError), error);

    const QString key = id.trimmed();
    const int i = indexOf(key);
    if (i < 0)
        return setError(QStringLiteral("设备不存在，无法删除。"), error);

    if (!m_database.remove(key, &m_lastError))
        return setError(m_lastError, error);

    m_devices.removeAt(i);
    m_lastError.clear();
    emit deviceRemoved(key);
    emit devicesChanged();
    return true;
}

bool DeviceManager::updateDeviceStatus(const QString &id, DeviceStatus status,
                                       QString *error)
{
    if (!m_databaseReady)
        return setError(QStringLiteral("数据库不可用：%1").arg(m_lastError), error);

    const QString key = id.trimmed();
    const int i = indexOf(key);
    if (i < 0)
        return setError(QStringLiteral("设备不存在，无法修改状态。"), error);

    if (m_devices.at(i).status == status)
        return true;

    if (!m_database.updateStatus(key, status, &m_lastError))
        return setError(m_lastError, error);

    m_devices[i].status = status;
    m_lastError.clear();
    emit deviceStatusChanged(m_devices.at(i).devId, status);
    emit deviceUpdated(m_devices.at(i));
    emit devicesChanged();
    return true;
}

int DeviceManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).devId.compare(id, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

bool DeviceManager::setError(const QString &message, QString *error)
{
    m_lastError = message;
    if (error)
        *error = message;
    return false;
}
