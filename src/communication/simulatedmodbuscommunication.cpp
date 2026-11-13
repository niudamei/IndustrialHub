#include "simulatedmodbuscommunication.h"

#include "src/protocol/modbuscodec.h"

#include <QTimer>

SimulatedModbusCommunication::SimulatedModbusCommunication(QObject *parent)
    : ICommunication(parent)
{
}

SimulatedModbusCommunication::~SimulatedModbusCommunication()
{
    close();
}

bool SimulatedModbusCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    const QString id = device.devId.trimmed();
    if (id.isEmpty())
        return setError(QStringLiteral("Modbus 模拟设备编号不能为空。"), errorMessage);

    bool addressOk = true;
    const int configuredUnit = device.devAddr.trimmed().isEmpty()
        ? 1 : device.devAddr.trimmed().toInt(&addressOk);
    if (!addressOk || configuredUnit < 1 || configuredUnit > 247)
        return setError(QStringLiteral("Modbus 模拟设备地址必须是 1–247 的 RTU 单元地址。"), errorMessage);

    if (m_open && m_deviceId == id && m_unitId == configuredUnit)
        return true;
    if (m_open) close();

    ++m_sessionGeneration;
    m_deviceId = id;
    m_unitId = static_cast<quint8>(configuredUnit);
    m_registers.clear();
    // 示例寄存器仅用于离线演示，不代表任何真实设备的寄存器定义。
    m_registers.insert(0, 253);       // 温度原始值：25.3（scale 0.1）
    m_registers.insert(1, 125);       // 电流原始值：1.25（scale 0.01）
    m_registers.insert(2, 1200);      // 转速示例：1200
    m_registers.insert(3, 1);         // 状态示例
    m_registers.insert(10, 0x3F80);   // Float32 示例：1.0
    m_registers.insert(11, 0x0000);
    m_open = true;
    emit connectionStateChanged(true);
    if (errorMessage) errorMessage->clear();
    return true;
}

void SimulatedModbusCommunication::close()
{
    if (!m_open) return;
    m_open = false;
    ++m_sessionGeneration;
    m_deviceId.clear();
    emit connectionStateChanged(false);
}

bool SimulatedModbusCommunication::isOpen() const
{
    return m_open;
}

bool SimulatedModbusCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!m_open)
        return setError(QStringLiteral("Modbus 模拟设备尚未连接。"), errorMessage);
    if (data.size() < 8)
        return setError(QStringLiteral("Modbus RTU 请求长度不足。"), errorMessage);

    const quint16 receivedCrc = static_cast<quint8>(data.at(data.size() - 2))
        | (static_cast<quint16>(static_cast<quint8>(data.at(data.size() - 1))) << 8);
    if (ModbusCodec::crc16(data.left(data.size() - 2)) != receivedCrc)
        return setError(QStringLiteral("Modbus 模拟设备收到 CRC 错误的请求。"), errorMessage);
    if (static_cast<quint8>(data.at(0)) != m_unitId)
        return setError(QStringLiteral("Modbus 请求单元地址与模拟设备地址不匹配。"), errorMessage);

    const quint8 function = static_cast<quint8>(data.at(1));
    const quint16 address = (static_cast<quint8>(data.at(2)) << 8)
        | static_cast<quint8>(data.at(3));
    QByteArray response;

    if (function == 0x03 || function == 0x04) {
        if (data.size() != 8)
            return setError(QStringLiteral("Modbus 读取请求帧长度无效。"), errorMessage);
        const quint16 quantity = (static_cast<quint8>(data.at(4)) << 8)
            | static_cast<quint8>(data.at(5));
        if (quantity == 0 || quantity > 125
            || static_cast<quint32>(address) + quantity > 65536U) {
            response = makeException(function, 0x03);
        } else {
            response.append(static_cast<char>(m_unitId));
            response.append(static_cast<char>(function));
            response.append(static_cast<char>(quantity * 2));
            for (quint32 i = 0; i < quantity; ++i) {
                const quint16 value = m_registers.value(
                    static_cast<quint16>(static_cast<quint32>(address) + i), 0);
                response.append(static_cast<char>((value >> 8) & 0xFF));
                response.append(static_cast<char>(value & 0xFF));
            }
            response = addCrc(response);
        }
    } else if (function == 0x06) {
        if (data.size() != 8)
            return setError(QStringLiteral("Modbus 单寄存器写入请求长度无效。"), errorMessage);
        const quint16 value = (static_cast<quint8>(data.at(4)) << 8)
            | static_cast<quint8>(data.at(5));
        m_registers.insert(address, value);
        response = addCrc(data.left(6));
    } else if (function == 0x10) {
        if (data.size() < 9)
            return setError(QStringLiteral("Modbus 多寄存器写入请求长度无效。"), errorMessage);
        const quint16 quantity = (static_cast<quint8>(data.at(4)) << 8)
            | static_cast<quint8>(data.at(5));
        const int byteCount = static_cast<quint8>(data.at(6));
        if (quantity == 0 || quantity > 123 || byteCount != quantity * 2
            || data.size() != 9 + byteCount
            || static_cast<quint32>(address) + quantity > 65536U) {
            response = makeException(function, 0x03);
        } else {
            for (quint16 i = 0; i < quantity; ++i) {
                const int offset = 7 + i * 2;
                const quint16 value = (static_cast<quint8>(data.at(offset)) << 8)
                    | static_cast<quint8>(data.at(offset + 1));
                m_registers.insert(static_cast<quint16>(address + i), value);
            }
            QByteArray pdu = data.mid(0, 6);
            response = addCrc(pdu);
        }
    } else {
        response = makeException(function, 0x01);
    }

    if (response.isEmpty())
        return setError(QStringLiteral("无法生成 Modbus 模拟响应。"), errorMessage);
    if (errorMessage) errorMessage->clear();
    const quint64 generation = m_sessionGeneration;
    QTimer::singleShot(5, this, [this, response, generation]() {
        if (m_open && generation == m_sessionGeneration)
            emit dataReceived(response);
    });
    return true;
}

QByteArray SimulatedModbusCommunication::makeException(
    quint8 function, quint8 exceptionCode) const
{
    QByteArray response;
    response.append(static_cast<char>(m_unitId));
    response.append(static_cast<char>(function | 0x80));
    response.append(static_cast<char>(exceptionCode));
    return addCrc(response);
}

QByteArray SimulatedModbusCommunication::addCrc(QByteArray frame) const
{
    const quint16 crc = ModbusCodec::crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

bool SimulatedModbusCommunication::setError(
    const QString &message, QString *errorMessage)
{
    if (errorMessage) *errorMessage = message;
    emit errorOccurred(message);
    return false;
}
