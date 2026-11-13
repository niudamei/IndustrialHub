#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include "src/protocol/modbusmaster.h"

using namespace ModbusCodec;

static QByteArray rtuReadResponse()
{
    QByteArray frame = QByteArray::fromHex("010304000a0102");
    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ModbusMaster master(Transport::Rtu);
    QVector<QByteArray> sent;
    master.setSendFunction([&sent](const QByteArray &frame, QString *) {
        sent.append(frame);
        return true;
    });

    bool completed = false;
    QObject::connect(&master, &ModbusMaster::requestCompleted,
                     [&](quint64, const Response &response) {
        completed = response.ok && response.registers.size() == 2
            && response.registers.at(0) == 10;
    });
    const quint64 first = master.readHoldingRegisters(0, 2);
    const quint64 second = master.writeSingleRegister(5, 0x1234);
    if (first == 0 || second == 0 || sent.size() != 1 || master.pendingCount() != 2) return 1;

    const QByteArray response = rtuReadResponse();
    master.feedReceivedData(response.left(3));
    if (completed || sent.size() != 1) return 2;
    master.feedReceivedData(response.mid(3));
    if (!completed || sent.size() != 2 || master.pendingCount() != 1) return 3;

    QByteArray writeEcho = QByteArray::fromHex("010600051234");
    const quint16 crc = crc16(writeEcho);
    writeEcho.append(static_cast<char>(crc & 0xFF));
    writeEcho.append(static_cast<char>((crc >> 8) & 0xFF));
    master.feedReceivedData(writeEcho);
    if (master.isBusy()) return 4;

    bool timedOut = false;
    QObject::connect(&master, &ModbusMaster::requestFailed,
                     [&](quint64, const QString &reason) {
        if (reason.contains(QStringLiteral("超时"))) timedOut = true;
    });
    master.setTimeoutMs(50);
    master.readHoldingRegisters(0, 1);
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    if (!timedOut || master.isBusy()) return 5;

    ModbusMaster tcp(Transport::Tcp);
    QByteArray tcpSent;
    tcp.setSendFunction([&tcpSent](const QByteArray &frame, QString *) {
        tcpSent = frame; return true;
    });
    tcp.setTimeoutMs(500);
    bool tcpDone = false;
    QObject::connect(&tcp, &ModbusMaster::requestCompleted,
                     [&](quint64, const Response &r) { tcpDone = r.ok; });
    tcp.readInputRegisters(10, 1);
    const quint16 tid = (static_cast<quint8>(tcpSent.at(0)) << 8)
        | static_cast<quint8>(tcpSent.at(1));
    QByteArray tcpResponse;
    tcpResponse.append(static_cast<char>((tid >> 8) & 0xFF));
    tcpResponse.append(static_cast<char>(tid & 0xFF));
    tcpResponse.append(QByteArray::fromHex("00000005010402002a"));
    QByteArray staleResponse = tcpResponse;
    staleResponse[1] = static_cast<char>(staleResponse.at(1) + 1);
    tcp.feedReceivedData(staleResponse);
    if (tcpDone || !tcp.isBusy()) return 6;
    tcp.feedReceivedData(tcpResponse.left(4));
    if (tcpDone) return 7;
    tcp.feedReceivedData(tcpResponse.mid(4));
    if (!tcpDone || tcp.isBusy()) return 8;

    ModbusMaster limited(Transport::Rtu);
    limited.setSendFunction([](const QByteArray &, QString *) { return true; });
    for (int i = 0; i < 1024; ++i)
        if (limited.readHoldingRegisters(0, 1) == 0) return 9;
    if (limited.pendingCount() != 1024 || limited.readHoldingRegisters(0, 1) != 0) return 10;
    limited.cancelAll();

    master.cancelAll();
    return 0;
}
