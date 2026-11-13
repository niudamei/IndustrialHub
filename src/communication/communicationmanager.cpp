#include "communicationmanager.h"

#include "communicationworker.h"
#include "simulatedcommunication.h"
#include "tcpcommunication.h"
#include "udpcommunication.h"
#ifdef INDUSTRIALHUB_HAS_SERIALPORT
#include "slcancommunication.h"
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
    const QStringList keys = m_workers.keys();
    for (const QString &key : keys)
        releaseDevice(m_deviceIds.value(key));
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

    CommunicationWorker *worker = m_workers.value(key, nullptr);
    if (worker && m_commTypes.value(key) != commType) {
        releaseDevice(deviceId);
        worker = nullptr;
    }

    if (!worker) {
        const auto factory = m_factories.constFind(commType);
        if (factory == m_factories.cend())
            return setError(QStringLiteral("不支持的通信类型：%1").arg(commType), errorMessage);

        worker = new CommunicationWorker(factory.value(), this);
        if (!worker->isReady()) {
            delete worker;
            return setError(QStringLiteral("通信工作线程初始化失败：%1").arg(deviceId),
                            errorMessage);
        }
        m_workers.insert(key, worker);
        m_commTypes.insert(key, commType);
        m_deviceIds.insert(key, deviceId);
        attachSignals(deviceId, worker);
    }

    m_deviceConfigs.insert(key, device);
    if (commType == QStringLiteral("TCP"))
        m_autoReconnectDevices.insert(key);
    else
        m_autoReconnectDevices.remove(key);

    if (!worker->open(device, errorMessage)) {
        if (worker->isOpen())
            m_connectedDevices.insert(key);
        return false;
    }
    return true;
}

void CommunicationManager::disconnectDevice(const QString &deviceId)
{
    const QString key = deviceKey(deviceId);
    m_autoReconnectDevices.remove(key);
    stopReconnect(key);
    m_reconnectAttempts.insert(key, 0);
    CommunicationWorker *worker = m_workers.value(key, nullptr);
    if (worker)
        worker->close();
}

void CommunicationManager::disconnectAll()
{
    m_autoReconnectDevices.clear();
    const auto timerKeys = m_reconnectTimers.keys();
    for (const QString &key : timerKeys)
        stopReconnect(key);
    m_reconnectAttempts.clear();
    const auto workers = m_workers.values();
    for (CommunicationWorker *worker : workers) {
        if (worker)
            worker->close();
    }
}

bool CommunicationManager::sendData(const QString &deviceId,
                                     const QByteArray &data,
                                     QString *errorMessage)
{
    CommunicationWorker *worker = m_workers.value(deviceKey(deviceId), nullptr);
    if (!worker) {
        return setError(QStringLiteral("发送失败：设备 %1 尚未创建通信实例。")
                            .arg(deviceId), errorMessage);
    }
    // 仅确认发送任务成功入队；真正写入成功后由 worker 的 dataSent 信号记录 TX。
    return worker->sendData(data, errorMessage);
}

bool CommunicationManager::isConnected(const QString &deviceId) const
{
    const QString key = deviceKey(deviceId);
    CommunicationWorker *worker = m_workers.value(key, nullptr);
    return worker && m_connectedDevices.contains(key) && worker->isOpen();
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
    CommunicationWorker *worker = m_workers.take(key);
    m_commTypes.remove(key);
    m_deviceIds.remove(key);
    m_deviceConfigs.remove(key);
    m_autoReconnectDevices.remove(key);
    m_connectedDevices.remove(key);
    stopReconnect(key);
    QTimer *timer = m_reconnectTimers.take(key);
    if (timer)
        delete timer;
    m_reconnectAttempts.remove(key);
    if (!worker)
        return;
    worker->close();
    delete worker;
}

void CommunicationManager::attachSignals(const QString &deviceId,
                                         CommunicationWorker *worker)
{
    const QString key = deviceKey(deviceId);
    connect(worker, &CommunicationWorker::connectionStateChanged,
            this, [this, deviceId, key](bool connected) {
        if (connected) {
            m_connectedDevices.insert(key);
            stopReconnect(key);
            m_reconnectAttempts.insert(key, 0);
        } else {
            m_connectedDevices.remove(key);
            scheduleReconnect(key);
        }
        emit deviceConnectionStateChanged(deviceId, connected);
    });

    connect(worker, &CommunicationWorker::dataReceived,
            this, [this, deviceId](const QByteArray &data) {
        emit deviceDataReceived(deviceId, data);
    });

    connect(worker, &CommunicationWorker::dataSent,
            this, [this, deviceId](const QByteArray &data) {
        emit deviceDataSent(deviceId, data);
    });

    connect(worker, &CommunicationWorker::errorOccurred,
            this, [this, deviceId, key](const QString &message) {
        emit deviceErrorOccurred(deviceId, message);
        scheduleReconnect(key);
    });
}

void CommunicationManager::scheduleReconnect(const QString &key)
{
    if (!m_autoReconnectDevices.contains(key) || !m_deviceConfigs.contains(key))
        return;

    CommunicationWorker *worker = m_workers.value(key, nullptr);
    if (!worker || worker->isOpen())
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

    CommunicationWorker *worker = m_workers.value(key, nullptr);
    if (!worker)
        return;
    if (worker->isOpen()) {
        stopReconnect(key);
        m_reconnectAttempts.insert(key, 0);
        return;
    }

    m_reconnectAttempts.insert(key, m_reconnectAttempts.value(key, 0) + 1);
    QString error;
    if (!worker->open(m_deviceConfigs.value(key), &error)) {
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
