#include <QCoreApplication>
#include <QDebug>
#include "src/protocol/modbuscodec.h"

using namespace ModbusCodec;

static bool check(bool condition, int code)
{
    if (!condition) qCritical() << "Modbus codec test failed:" << code;
    return condition;
}

static QByteArray withCrc(QByteArray frame)
{
    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString error;

    const QByteArray rtuRead = readRequest(Transport::Rtu, 1, 0x03, 0, 10);
    if (!check(rtuRead.toHex() == QByteArray("01030000000ac5cd"), 1)) return 1;

    const QByteArray tcpRead = readRequest(Transport::Tcp, 1, 0x03, 0, 10, 0x1234);
    if (!check(tcpRead.toHex() == QByteArray("12340000000601030000000a"), 2)) return 2;

    const QByteArray single = writeSingleRegisterRequest(Transport::Rtu, 7, 0x1234, 0xABCD);
    if (!check(single.size() == 8 && single.left(6).toHex() == QByteArray("07061234abcd"), 3)) return 3;

    const QByteArray multi = writeMultipleRegistersRequest(
        Transport::Tcp, 2, 0x0010, {0x1234, 0xABCD}, 0x3344);
    if (!check(multi.toHex() == QByteArray("33440000000b021000100002041234abcd"), 4)) return 4;

    const QByteArray readResponse = withCrc(QByteArray::fromHex("010304000a0102"));
    const Response decoded = decodeResponse(Transport::Rtu, readResponse, 1, 0x03);
    if (!check(decoded.ok && decoded.registers.size() == 2
               && decoded.registers.at(0) == 10 && decoded.registers.at(1) == 0x0102, 5)) return 5;

    QByteArray badCrc = readResponse;
    badCrc[badCrc.size() - 1] = static_cast<char>(badCrc.at(badCrc.size() - 1) ^ 0x01);
    if (!check(!decodeResponse(Transport::Rtu, badCrc, 1, 0x03).ok, 6)) return 6;

    const QByteArray tcpResponse = QByteArray::fromHex("123400000007010304000a0102");
    const Response tcpDecoded = decodeResponse(Transport::Tcp, tcpResponse, 1, 0x03, 0x1234);
    if (!check(tcpDecoded.ok && tcpDecoded.registers.size() == 2
               && tcpDecoded.registers.at(1) == 0x0102, 7)) return 7;
    if (!check(!decodeResponse(Transport::Tcp, tcpResponse, 1, 0x03, 0x1235).ok, 8)) return 8;

    const QByteArray exception = withCrc(QByteArray::fromHex("018302"));
    const Response exceptionResult = decodeResponse(Transport::Rtu, exception, 1, 0x03);
    if (!check(!exceptionResult.ok && exceptionResult.exception
               && exceptionResult.exceptionCode == 2, 9)) return 9;

    if (!check(readRequest(Transport::Rtu, 0, 0x03, 0, 1).isEmpty(), 10)) return 10;
    if (!check(readRequest(Transport::Rtu, 1, 0x03, 0, 126).isEmpty(), 11)) return 11;
    if (!check(writeMultipleRegistersRequest(Transport::Rtu, 1, 0, {}).isEmpty(), 12)) return 12;
    return 0;
}
