#include "simulatedcommunication.h"

#include <QTimer>

SimulatedCommunication::SimulatedCommunication(QObject *parent)
    : ICommunication(parent)
{
}

SimulatedCommunication::~SimulatedCommunication()
{
    close();
}

bool SimulatedCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (device.devId.trimmed().isEmpty())
        return setError(QStringLiteral("模拟通信：设备编号不能为空。"), errorMessage);

    if (m_open && m_deviceId == device.devId)
        return true;

    if (m_open)
        close();

    m_deviceId = device.devId;
    m_open = true;
    emit connectionStateChanged(true);
    return true;
}

void SimulatedCommunication::close()
{
    if (!m_open)
        return;

    m_open = false;
    m_deviceId.clear();
    emit connectionStateChanged(false);
}

bool SimulatedCommunication::isOpen() const
{
    return m_open;
}

bool SimulatedCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!m_open)
        return setError(QStringLiteral("模拟通信：设备尚未连接。"), errorMessage);

    const QByteArray response = data;
    QTimer::singleShot(30, this, [this, response]() {
        if (m_open)
            emit dataReceived(response);
    });
    return true;
}

bool SimulatedCommunication::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;

    emit errorOccurred(message);
    return false;
}
