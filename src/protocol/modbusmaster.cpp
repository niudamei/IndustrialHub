#include "modbusmaster.h"

#include "src/communication/communicationmanager.h"

#include <utility>

#include <QtGlobal>

ModbusMaster::ModbusMaster(ModbusCodec::Transport transport, QObject *parent)
    : QObject(parent), m_transport(transport)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout,
            this, &ModbusMaster::onTimeout);
}

void ModbusMaster::setSendFunction(SendFunction sender)
{
    m_sender = std::move(sender);
}

bool ModbusMaster::bindDevice(CommunicationManager *manager,
                              const QString &deviceId, quint8 unitId)
{
    unbindDevice();
    if (!manager || deviceId.trimmed().isEmpty() || unitId < 1 || unitId > 247)
        return false;
    m_manager = manager;
    m_deviceId = deviceId.trimmed();
    m_unitId = unitId;
    QPointer<CommunicationManager> guardedManager(manager);
    const QString boundId = m_deviceId;
    m_sender = [guardedManager, boundId](const QByteArray &frame, QString *error) {
        if (!guardedManager) {
            if (error) *error = QStringLiteral("通信管理器已销毁。");
            return false;
        }
        return guardedManager->sendData(boundId, frame, error);
    };
    m_managerConnections.append(connect(manager, &CommunicationManager::deviceDataReceived,
        this, [this, boundId](const QString &id, const QByteArray &data) {
            if (id.compare(boundId, Qt::CaseInsensitive) == 0)
                feedReceivedData(data);
        }));
    m_managerConnections.append(connect(manager, &CommunicationManager::deviceErrorOccurred,
        this, [this, boundId](const QString &id, const QString &message) {
            if (id.compare(boundId, Qt::CaseInsensitive) == 0 && m_hasActive)
                failActive(QStringLiteral("底层通信错误：%1").arg(message));
        }));
    m_managerConnections.append(connect(manager, &CommunicationManager::deviceConnectionStateChanged,
        this, [this, boundId](const QString &id, bool connected) {
            if (id.compare(boundId, Qt::CaseInsensitive) == 0 && !connected)
                cancelAll(QStringLiteral("Modbus 设备连接已断开。"));
        }));
    return true;
}

void ModbusMaster::unbindDevice()
{
    for (const QMetaObject::Connection &connection : m_managerConnections)
        disconnect(connection);
    m_managerConnections.clear();
    m_manager.clear();
    m_deviceId.clear();
    m_sender = {};
    cancelAll(QStringLiteral("Modbus 设备绑定已解除。"));
}

void ModbusMaster::setUnitId(quint8 unitId)
{
    if (!isBusy() && unitId >= 1 && unitId <= 247) m_unitId = unitId;
}

quint8 ModbusMaster::unitId() const { return m_unitId; }
ModbusCodec::Transport ModbusMaster::transport() const { return m_transport; }

void ModbusMaster::setTimeoutMs(int timeoutMs)
{
    if (!isBusy()) m_timeoutMs = qBound(50, timeoutMs, 60000);
}

int ModbusMaster::timeoutMs() const { return m_timeoutMs; }
int ModbusMaster::pendingCount() const { return m_queue.size() + (m_hasActive ? 1 : 0); }
bool ModbusMaster::isBusy() const { return m_hasActive || !m_queue.isEmpty(); }

quint64 ModbusMaster::readHoldingRegisters(quint16 address, quint16 quantity)
{
    return enqueue(0x03, address, quantity);
}

quint64 ModbusMaster::readInputRegisters(quint16 address, quint16 quantity)
{
    return enqueue(0x04, address, quantity);
}

quint64 ModbusMaster::writeSingleRegister(quint16 address, quint16 value)
{
    return enqueue(0x06, address, value);
}

quint64 ModbusMaster::writeMultipleRegisters(quint16 address, const QVector<quint16> &values)
{
    return enqueue(0x10, address, static_cast<quint16>(values.size()), values);
}

quint64 ModbusMaster::enqueue(quint8 function, quint16 address,
                              quint16 valueOrQuantity, const QVector<quint16> &values)
{
    if (!m_sender) {
        emit requestFailed(0, QStringLiteral("Modbus 主站尚未设置底层发送函数。"));
        return 0;
    }
    if (pendingCount() >= 1024) {
        emit requestFailed(0, QStringLiteral("Modbus 请求队列已满（最多 1024 个待处理请求）。"));
        return 0;
    }

    QString error;
    QByteArray frame;
    quint16 transaction = 0;
    if (m_transport == ModbusCodec::Transport::Tcp) {
        transaction = m_nextTransactionId++;
        frame = function == 0x03 || function == 0x04
            ? ModbusCodec::readRequest(m_transport, m_unitId, function,
                                       address, valueOrQuantity, transaction, &error)
            : (function == 0x06
                ? ModbusCodec::writeSingleRegisterRequest(m_transport, m_unitId,
                                                          address, valueOrQuantity, transaction, &error)
                : ModbusCodec::writeMultipleRegistersRequest(m_transport, m_unitId,
                                                              address, values, transaction, &error));
    } else {
        frame = function == 0x03 || function == 0x04
            ? ModbusCodec::readRequest(m_transport, m_unitId, function,
                                       address, valueOrQuantity, 1, &error)
            : (function == 0x06
                ? ModbusCodec::writeSingleRegisterRequest(m_transport, m_unitId,
                                                          address, valueOrQuantity, 1, &error)
                : ModbusCodec::writeMultipleRegistersRequest(m_transport, m_unitId,
                                                              address, values, 1, &error));
    }
    if (frame.isEmpty()) {
        emit requestFailed(0, error.isEmpty() ? QStringLiteral("无法构造 Modbus 请求帧。") : error);
        return 0;
    }

    const bool wasIdle = !isBusy();
    Request request;
    request.id = m_nextRequestId++;
    request.function = function;
    request.address = address;
    request.valueOrQuantity = valueOrQuantity;
    request.values = values;
    request.frame = frame;
    request.transactionId = transaction;
    m_queue.enqueue(request);
    emit queueChanged(pendingCount());
    if (wasIdle) QTimer::singleShot(0, this, &ModbusMaster::sendNext);
    return request.id;
}

void ModbusMaster::sendNext()
{
    if (m_hasActive || m_queue.isEmpty()) return;
    m_active = m_queue.dequeue();
    m_hasActive = true;
    emit busyChanged(true);
    m_timeoutTimer.start(m_timeoutMs);
    QString error;
    if (!m_sender(m_active.frame, &error)) {
        failActive(error.isEmpty() ? QStringLiteral("底层通信驱动拒绝发送 Modbus 请求。") : error);
        return;
    }
    emit queueChanged(pendingCount());
}

bool ModbusMaster::extractOneFrame(QByteArray &frame)
{
    if (!m_hasActive || m_receiveBuffer.isEmpty()) return false;

    if (m_transport == ModbusCodec::Transport::Tcp) {
        while (m_receiveBuffer.size() >= 6) {
            const quint16 protocol = (static_cast<quint8>(m_receiveBuffer.at(2)) << 8)
                | static_cast<quint8>(m_receiveBuffer.at(3));
            const quint16 length = (static_cast<quint8>(m_receiveBuffer.at(4)) << 8)
                | static_cast<quint8>(m_receiveBuffer.at(5));
            if (protocol != 0 || length < 3 || length > 254) {
                m_receiveBuffer.remove(0, 1);
                continue;
            }
            const int total = 6 + length;
            if (m_receiveBuffer.size() < total) return false;
            frame = m_receiveBuffer.left(total);
            m_receiveBuffer.remove(0, total);
            return true;
        }
        return false;
    }

    while (!m_receiveBuffer.isEmpty()
           && static_cast<quint8>(m_receiveBuffer.at(0)) != m_unitId)
        m_receiveBuffer.remove(0, 1);
    if (m_receiveBuffer.size() < 2) return false;
    const quint8 function = static_cast<quint8>(m_receiveBuffer.at(1));
    int total = 0;
    if (function == static_cast<quint8>(m_active.function | 0x80)) {
        total = 5;
    } else if (function == 0x06 || function == 0x10) {
        total = 8;
    } else if (function == 0x03 || function == 0x04) {
        if (m_receiveBuffer.size() < 3) return false;
        const int byteCount = static_cast<quint8>(m_receiveBuffer.at(2));
        if (byteCount > 250 || (byteCount % 2) != 0) {
            m_receiveBuffer.remove(0, 1);
            return false;
        }
        total = 5 + byteCount;
    } else {
        m_receiveBuffer.remove(0, 1);
        return false;
    }
    if (m_receiveBuffer.size() < total) return false;
    frame = m_receiveBuffer.left(total);
    m_receiveBuffer.remove(0, total);
    return true;
}

void ModbusMaster::feedReceivedData(const QByteArray &data)
{
    if (data.isEmpty()) return;
    m_receiveBuffer.append(data);
    if (m_receiveBuffer.size() > 65536)
        m_receiveBuffer = m_receiveBuffer.right(4096);

    QByteArray frame;
    while (m_hasActive && extractOneFrame(frame)) {
        processFrame(frame);
        frame.clear();
    }
}

void ModbusMaster::processFrame(const QByteArray &frame)
{
    if (!m_hasActive) return;
    if (m_transport == ModbusCodec::Transport::Tcp && frame.size() >= 2) {
        const quint16 receivedTransaction = (static_cast<quint8>(frame.at(0)) << 8)
            | static_cast<quint8>(frame.at(1));
        if (receivedTransaction != m_active.transactionId)
            return; // Ignore a delayed response belonging to an earlier request.
    }
    const ModbusCodec::Response response = ModbusCodec::decodeResponse(
        m_transport, frame, m_unitId, m_active.function,
        m_active.transactionId);
    if (!response.ok) {
        if (response.exception) {
            finishActive(response);
        } else {
            failActive(response.error);
        }
        return;
    }

    if (m_active.function == 0x03 || m_active.function == 0x04) {
        if (response.registers.size() != m_active.valueOrQuantity) {
            failActive(QStringLiteral("Modbus 响应寄存器数量与请求不一致。"));
            return;
        }
    } else if (response.address != m_active.address) {
        failActive(QStringLiteral("Modbus 写入响应地址与请求不一致。"));
        return;
    } else if (m_active.function == 0x06
               && response.value != m_active.valueOrQuantity) {
        failActive(QStringLiteral("Modbus 单寄存器写入回显值与请求不一致。"));
        return;
    } else if (m_active.function == 0x10
               && response.value != m_active.valueOrQuantity) {
        failActive(QStringLiteral("Modbus 多寄存器写入数量回显与请求不一致。"));
        return;
    }
    finishActive(response);
}

void ModbusMaster::finishActive(const ModbusCodec::Response &response)
{
    if (!m_hasActive) return;
    const quint64 id = m_active.id;
    m_timeoutTimer.stop();
    m_hasActive = false;
    emit requestCompleted(id, response);
    emit queueChanged(pendingCount());
    if (!isBusy()) emit busyChanged(false);
    sendNext();
}

void ModbusMaster::failActive(const QString &reason)
{
    if (!m_hasActive) return;
    const quint64 id = m_active.id;
    m_timeoutTimer.stop();
    m_hasActive = false;
    emit requestFailed(id, reason);
    emit queueChanged(pendingCount());
    if (!isBusy()) emit busyChanged(false);
    QTimer::singleShot(0, this, &ModbusMaster::sendNext);
}

void ModbusMaster::onTimeout()
{
    failActive(QStringLiteral("Modbus 请求超时（%1 ms），设备未在期限内返回完整响应。")
                   .arg(m_timeoutMs));
}

void ModbusMaster::cancelAll(const QString &reason)
{
    m_timeoutTimer.stop();
    if (m_hasActive) {
        emit requestFailed(m_active.id, reason);
        m_hasActive = false;
    }
    while (!m_queue.isEmpty())
        emit requestFailed(m_queue.dequeue().id, reason);
    m_receiveBuffer.clear();
    emit queueChanged(0);
    emit busyChanged(false);
}
