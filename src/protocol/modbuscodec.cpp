#include "modbuscodec.h"

#include <utility>

namespace ModbusCodec {
namespace {

void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>(value & 0xFF));
}

quint16 readU16(const QByteArray &data, int offset)
{
    return static_cast<quint16>(
        (static_cast<quint8>(data.at(offset)) << 8)
        | static_cast<quint8>(data.at(offset + 1)));
}

QByteArray finishFrame(Transport transport, quint8 unitId,
                       const QByteArray &pdu, quint16 transactionId)
{
    QByteArray frame;
    if (transport == Transport::Tcp) {
        appendU16(frame, transactionId);
        appendU16(frame, 0);
        appendU16(frame, static_cast<quint16>(pdu.size() + 1));
        frame.append(static_cast<char>(unitId));
        frame.append(pdu);
    } else {
        frame.append(static_cast<char>(unitId));
        frame.append(pdu);
        const quint16 crc = crc16(frame);
        frame.append(static_cast<char>(crc & 0xFF));
        frame.append(static_cast<char>((crc >> 8) & 0xFF));
    }
    return frame;
}

QByteArray failed(QString message, QString *error)
{
    if (error) *error = std::move(message);
    return {};
}

} // namespace

quint16 crc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x0001) ? static_cast<quint16>((crc >> 1) ^ 0xA001)
                                 : static_cast<quint16>(crc >> 1);
    }
    return crc;
}

QByteArray readRequest(Transport transport, quint8 unitId, quint8 function,
                       quint16 startAddress, quint16 quantity,
                       quint16 transactionId, QString *error)
{
    if (unitId == 0 || unitId > 247)
        return failed(QStringLiteral("Modbus 单元地址必须在 1–247 之间。"), error);
    if (function != 0x03 && function != 0x04)
        return failed(QStringLiteral("读取功能码仅支持 0x03 和 0x04。"), error);
    if (quantity == 0 || quantity > 125
        || static_cast<quint32>(startAddress) + quantity > 65536U)
        return failed(QStringLiteral("读取寄存器数量或地址范围无效。"), error);

    QByteArray pdu;
    pdu.append(static_cast<char>(function));
    appendU16(pdu, startAddress);
    appendU16(pdu, quantity);
    if (error) error->clear();
    return finishFrame(transport, unitId, pdu, transactionId);
}

QByteArray writeSingleRegisterRequest(Transport transport, quint8 unitId,
                                      quint16 address, quint16 value,
                                      quint16 transactionId, QString *error)
{
    if (unitId == 0 || unitId > 247)
        return failed(QStringLiteral("Modbus 单元地址必须在 1–247 之间。"), error);
    QByteArray pdu;
    pdu.append(static_cast<char>(0x06));
    appendU16(pdu, address);
    appendU16(pdu, value);
    if (error) error->clear();
    return finishFrame(transport, unitId, pdu, transactionId);
}

QByteArray writeMultipleRegistersRequest(Transport transport, quint8 unitId,
                                         quint16 startAddress,
                                         const QVector<quint16> &values,
                                         quint16 transactionId, QString *error)
{
    if (unitId == 0 || unitId > 247)
        return failed(QStringLiteral("Modbus 单元地址必须在 1–247 之间。"), error);
    if (values.isEmpty() || values.size() > 123
        || static_cast<quint32>(startAddress) + static_cast<quint32>(values.size()) > 65536U)
        return failed(QStringLiteral("批量写入寄存器数量或地址范围无效。"), error);

    QByteArray pdu;
    pdu.append(static_cast<char>(0x10));
    appendU16(pdu, startAddress);
    appendU16(pdu, static_cast<quint16>(values.size()));
    pdu.append(static_cast<char>(values.size() * 2));
    for (quint16 value : values) appendU16(pdu, value);
    if (error) error->clear();
    return finishFrame(transport, unitId, pdu, transactionId);
}

Response decodeResponse(Transport transport, const QByteArray &frame,
                        quint8 expectedUnitId, quint8 expectedFunction,
                        quint16 expectedTransactionId)
{
    Response result;
    int pduOffset = 1;
    int pduLength = 0;

    if (transport == Transport::Tcp) {
        if (frame.size() < 9) {
            result.error = QStringLiteral("Modbus TCP 响应长度不足。");
            return result;
        }
        if (readU16(frame, 0) != expectedTransactionId) {
            result.error = QStringLiteral("Modbus TCP 事务 ID 不匹配。");
            return result;
        }
        if (readU16(frame, 2) != 0) {
            result.error = QStringLiteral("Modbus TCP 协议标识必须为 0。");
            return result;
        }
        const quint16 length = readU16(frame, 4);
        if (length < 3 || frame.size() != 6 + length) {
            result.error = QStringLiteral("Modbus TCP MBAP 长度字段无效。");
            return result;
        }
        if (static_cast<quint8>(frame.at(6)) != expectedUnitId) {
            result.error = QStringLiteral("Modbus TCP 单元地址不匹配。");
            return result;
        }
        pduOffset = 7;
        pduLength = frame.size() - pduOffset;
    } else {
        if (frame.size() < 5) {
            result.error = QStringLiteral("Modbus RTU 响应长度不足。");
            return result;
        }
        const quint16 receivedCrc = static_cast<quint8>(frame.at(frame.size() - 2))
            | (static_cast<quint16>(static_cast<quint8>(frame.at(frame.size() - 1))) << 8);
        if (crc16(frame.left(frame.size() - 2)) != receivedCrc) {
            result.error = QStringLiteral("Modbus RTU CRC 校验失败。");
            return result;
        }
        if (static_cast<quint8>(frame.at(0)) != expectedUnitId) {
            result.error = QStringLiteral("Modbus RTU 单元地址不匹配。");
            return result;
        }
        pduOffset = 1;
        pduLength = frame.size() - 3;
    }

    result.unitId = expectedUnitId;
    result.function = static_cast<quint8>(frame.at(pduOffset));
    if (result.function == static_cast<quint8>(expectedFunction | 0x80)) {
        if (pduLength != 2) {
            result.error = QStringLiteral("Modbus 异常响应格式无效。");
            return result;
        }
        result.exception = true;
        result.exceptionCode = static_cast<quint8>(frame.at(pduOffset + 1));
        result.error = QStringLiteral("设备返回 Modbus 异常码 0x%1。")
                           .arg(result.exceptionCode, 2, 16, QLatin1Char('0')).toUpper();
        return result;
    }
    if (result.function != expectedFunction) {
        result.error = QStringLiteral("Modbus 功能码不匹配。");
        return result;
    }

    const int body = pduOffset + 1;
    if (expectedFunction == 0x03 || expectedFunction == 0x04) {
        if (pduLength < 2) {
            result.error = QStringLiteral("Modbus 读取响应缺少字节数。");
            return result;
        }
        const int byteCount = static_cast<quint8>(frame.at(body));
        if ((byteCount % 2) != 0 || pduLength != byteCount + 2) {
            result.error = QStringLiteral("Modbus 读取响应字节数无效。");
            return result;
        }
        for (int i = 0; i < byteCount; i += 2)
            result.registers.append(readU16(frame, body + 1 + i));
    } else if (expectedFunction == 0x06) {
        if (pduLength != 5) {
            result.error = QStringLiteral("Modbus 单寄存器写入响应长度无效。");
            return result;
        }
        result.address = readU16(frame, body);
        result.value = readU16(frame, body + 2);
    } else if (expectedFunction == 0x10) {
        if (pduLength != 5) {
            result.error = QStringLiteral("Modbus 多寄存器写入响应长度无效。");
            return result;
        }
        result.address = readU16(frame, body);
        result.value = readU16(frame, body + 2); // quantity
    } else {
        result.error = QStringLiteral("当前尚不支持该 Modbus 响应功能码。");
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace ModbusCodec
