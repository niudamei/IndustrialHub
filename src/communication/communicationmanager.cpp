#include "communicationmanager.h"

#include "simulatedcommunication.h"
#include "tcpcommunication.h"
#include "udpcommunication.h"
#ifdef INDUSTRIALHUB_HAS_SERIALPORT
#include "rs485communication.h"
#endif

#include <utility>

CommunicationManager::CommunicationManager(QObject *parent)
    : QObject(parent)
{
    registerFactory(QStringLiteral("TCP"), [](QObject *parent) {
        return new TcpCommunication(parent);
    });
    registerFactory(QStringLiteral("UDP"), [](QObject *parent) {
        return new UdpCommunication(parent);
    });
#ifdef INDUSTRIALHUB_HAS_SERIALPORT
    registerFactory(QStringLiteral("RS485"), [](QObject *parent) {
        return new Rs485Communication(parent);
    });
    registerFactory(QStringLiteral("SERIAL"), [](QObject *parent) {
        return new Rs485Communication(parent);
    });
#endif
    registerFactory(QStringLiteral("SIMULATED"), [](QObject *parent) {
        return new SimulatedCommunication(parent);
    });
    registerFactory(QStringLiteral("SIM"), [](QObject *parent) {
        return new SimulatedCommunication(parent);
    });
}

CommunicationManager::~CommunicationManager()
{
    disconnectAll();
}

bool CommunicationManager::registerFactory(const QString &commType,
                                             CommunicationFactory factory)
{
    const QString key = commType.trimmed().toUpper();
    if (key.isEmpty() || !factory)
        return false;

    m_factories.insert(key, std::move(factory));
    return true;
}

bool CommunicationManager::connectDevice(const DeviceInfo &device,
                                           QString *errorMessage)
{
    const QString deviceId = device.devId.trimmed();
    const QString key = deviceKey(deviceId);
    if (deviceId.isEmpty())
        return setError(QStringLiteral("连接失败：设备编号不能为空。"), errorMessage);

    const QString commType = device.commType.trimmed().toUpper();
    if (commType.isEmpty())
        return setError(QStringLiteral("连接失败：设备通信类型不能为空。"), errorMessage);

    ICommunication *communication = communicationFor(deviceId);
    if (communication && m_commTypes.value(key) != commType) {
        communication->close();
        communication->deleteLater();
        m_communications.remove(key);
        m_commTypes.remove(key);
        m_deviceIds.remove(key);
        communication = nullptr;
    }

    if (!communication) {
        communication = createCommunication(commType, errorMessage);
        if (!communication)
            return false;

        communication->setParent(this);
        m_communications.insert(key, communication);
        m_commTypes.insert(key, commType);
        m_deviceIds.insert(key, deviceId);
        attachSignals(deviceId, communication);
    }

    return communication->open(device, errorMessage);
}

void CommunicationManager::disconnectDevice(const QString &deviceId)
{
    ICommunication *communication = communicationFor(deviceId);
    if (communication)
        communication->close();
}

void CommunicationManager::disconnectAll()
{
    const auto communications = m_communications.values();
    for (ICommunication *communication : communications) {
        if (communication)
            communication->close();
    }
}

bool CommunicationManager::sendData(const QString &deviceId,
                                     const QByteArray &data,
                                     QString *errorMessage)
{
    ICommunication *communication = communicationFor(deviceId);
    if (!communication) {
        return setError(QStringLiteral("发送失败：设备 %1 尚未创建通信实例。")
                            .arg(deviceId), errorMessage);
    }

    return communication->sendData(data, errorMessage);
}

bool CommunicationManager::isConnected(const QString &deviceId) const
{
    ICommunication *communication = communicationFor(deviceId);
    return communication && communication->isOpen();
}

QStringList CommunicationManager::managedDeviceIds() const
{
    QStringList ids = m_deviceIds.values();
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

void CommunicationManager::releaseDevice(const QString &deviceId)
{
    const QString key = deviceKey(deviceId);
    ICommunication *communication = m_communications.take(key);
    m_commTypes.remove(key);
    m_deviceIds.remove(key);
    if (!communication)
        return;

    communication->close();
    communication->deleteLater();
}

ICommunication *CommunicationManager::createCommunication(
    const QString &commType, QString *errorMessage)
{
    const auto it = m_factories.constFind(commType);
    if (it == m_factories.cend()) {
        setError(QStringLiteral("不支持的通信类型：%1").arg(commType), errorMessage);
        return nullptr;
    }

    ICommunication *communication = it.value()(nullptr);
    if (!communication) {
        setError(QStringLiteral("通信类型 %1 的工厂创建实例失败。").arg(commType),
                 errorMessage);
    }
    return communication;
}

ICommunication *CommunicationManager::communicationFor(
    const QString &deviceId) const
{
    return m_communications.value(deviceKey(deviceId), nullptr);
}

void CommunicationManager::attachSignals(const QString &deviceId,
                                         ICommunication *communication)
{
    connect(communication, &ICommunication::connectionStateChanged,
            this, [this, deviceId](bool connected) {
        emit deviceConnectionStateChanged(deviceId, connected);
    });

    connect(communication, &ICommunication::dataReceived,
            this, [this, deviceId](const QByteArray &data) {
        emit deviceDataReceived(deviceId, data);
    });

    connect(communication, &ICommunication::errorOccurred,
            this, [this, deviceId](const QString &message) {
        emit deviceErrorOccurred(deviceId, message);
    });
}

QString CommunicationManager::deviceKey(const QString &deviceId)
{
    return deviceId.trimmed().toCaseFolded();
}

bool CommunicationManager::setError(const QString &message,
                                     QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;
    emit deviceErrorOccurred(QString(), message);
    return false;
}
