#include "communicationmanager.h"

#include "simulatedcommunication.h"
#include "tcpcommunication.h"
#include "udpcommunication.h"
#ifdef INDUSTRIALHUB_HAS_SERIALPORT
#include "slcancommunication.h"
#endif
#ifdef INDUSTRIALHUB_HAS_SERIALPORT
#include "rs485communication.h"
#endif

#include <utility>
#include <QTimer>
#include <QtMath>

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
    registerFactory(QStringLiteral("CAN"), [](QObject *parent) {
        return new SlcanCommunication(false, parent);
    });
    registerFactory(QStringLiteral("CANFD"), [](QObject *parent) {
        return new SlcanCommunication(true, parent);
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
        stopReconnect(key);
        m_autoReconnectDevices.remove(key);
        m_deviceConfigs.remove(key);
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

    if (!communication->open(device, errorMessage))
        return false;

    m_deviceConfigs.insert(key, device);
    if (commType == QStringLiteral("TCP"))
        m_autoReconnectDevices.insert(key);
    else
        m_autoReconnectDevices.remove(key);
    return true;
}

void CommunicationManager::disconnectDevice(const QString &deviceId)
{
    const QString key = deviceKey(deviceId);
    m_autoReconnectDevices.remove(key);
    stopReconnect(key);
    m_reconnectAttempts.insert(key, 0);
    ICommunication *communication = communicationFor(deviceId);
    if (communication)
        communication->close();
}

void CommunicationManager::disconnectAll()
{
    m_autoReconnectDevices.clear();
    const auto timerKeys = m_reconnectTimers.keys();
    for (const QString &key : timerKeys)
        stopReconnect(key);
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

    if (!communication->sendData(data, errorMessage))
        return false;

    // 通信驱动接受发送请求后通知监控页面；这不代表远端已经收到数据。
    emit deviceDataSent(m_deviceIds.value(deviceKey(deviceId), deviceId), data);
    return true;
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
    m_deviceConfigs.remove(key);
    m_autoReconnectDevices.remove(key);
    stopReconnect(key);
    m_reconnectAttempts.remove(key);
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
    const QString key = deviceKey(deviceId);
    connect(communication, &ICommunication::connectionStateChanged,
            this, [this, deviceId, key](bool connected) {
        if (connected) {
            stopReconnect(key);
            m_reconnectAttempts.insert(key, 0);
        } else {
            scheduleReconnect(key);
        }
        emit deviceConnectionStateChanged(deviceId, connected);
    });

    connect(communication, &ICommunication::dataReceived,
            this, [this, deviceId](const QByteArray &data) {
        emit deviceDataReceived(deviceId, data);
    });

    connect(communication, &ICommunication::errorOccurred,
            this, [this, deviceId, key](const QString &message) {
        emit deviceErrorOccurred(deviceId, message);
        scheduleReconnect(key);
    });
}

void CommunicationManager::scheduleReconnect(const QString &key)
{
    if (!m_autoReconnectDevices.contains(key) || !m_deviceConfigs.contains(key))
        return;

    ICommunication *communication = m_communications.value(key, nullptr);
    if (!communication || communication->isOpen())
        return;

    QTimer *timer = m_reconnectTimers.value(key, nullptr);
    if (!timer) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, key]() {
            attemptReconnect(key);
        });
        m_reconnectTimers.insert(key, timer);
    }
    if (timer->isActive())
        return;

    const int attempt = m_reconnectAttempts.value(key, 0) + 1;
    const int exponent = qMin(m_reconnectAttempts.value(key, 0), 5);
    const int delayMs = qMin(1000 * (1 << exponent), 30000);
    emit deviceReconnecting(m_deviceIds.value(key), attempt, delayMs);
    timer->start(delayMs);
}

void CommunicationManager::stopReconnect(const QString &key)
{
    QTimer *timer = m_reconnectTimers.value(key, nullptr);
    if (timer)
        timer->stop();
}

void CommunicationManager::attemptReconnect(const QString &key)
{
    if (!m_autoReconnectDevices.contains(key))
        return;

    ICommunication *communication = m_communications.value(key, nullptr);
    if (!communication)
        return;
    if (communication->isOpen()) {
        stopReconnect(key);
        m_reconnectAttempts.insert(key, 0);
        return;
    }

    m_reconnectAttempts.insert(key, m_reconnectAttempts.value(key, 0) + 1);
    QString error;
    if (!communication->open(m_deviceConfigs.value(key), &error)) {
        if (!error.isEmpty())
            emit deviceErrorOccurred(m_deviceIds.value(key), error);
        scheduleReconnect(key);
    }
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
