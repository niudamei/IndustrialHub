#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include "src/protocol/modbusmaster.h"
#include "src/communication/communicationmanager.h"
#include "src/communication/icommunication.h"
#include "src/pages/deviceinfo.h"

using namespace ModbusCodec;


class ModbusLoopbackCommunication : public ICommunication
{
public:
    explicit ModbusLoopbackCommunication(QObject *parent = nullptr)
        : ICommunication(parent) {}
    bool open(const DeviceInfo &, QString *) override
    {
        m_open = true;
        emit connectionStateChanged(true);
        return true;
    }
    void close() override
    {
        if (!m_open) return;
        m_open = false;
        emit connectionStateChanged(false);
    }
    bool isOpen() const override { return m_open; }
    bool sendData(const QByteArray &request, QString *) override
    {
        if (!m_open || request.size() < 8) return false;
        const quint8 unit = static_cast<quint8>(request.at(0));
        const quint8 function = static_cast<quint8>(
            request.at(request.size() > 8 ? (request.size() - 6) : 1));
        QByteArray response;
        if (request.size() >= 12 && request.at(2) == 0 && request.at(3) == 0) {
            const quint16 tid = (static_cast<quint8>(request.at(0)) << 8)
                | static_cast<quint8>(request.at(1));
            const quint8 tcpUnit = static_cast<quint8>(request.at(6));
            const quint8 tcpFunction = static_cast<quint8>(request.at(7));
            const quint16 quantity = (static_cast<quint8>(request.at(10)) << 8)
                | static_cast<quint8>(request.at(11));
            response.append(static_cast<char>((tid >> 8) & 0xFF));
            response.append(static_cast<char>(tid & 0xFF));
            response.append(QByteArray::fromHex("0000"));
            response.append(static_cast<char>(0));
            response.append(static_cast<char>(3 + quantity * 2));
            response.append(static_cast<char>(tcpUnit));
            response.append(static_cast<char>(tcpFunction));
            response.append(static_cast<char>(quantity * 2));
            for (quint16 i = 0; i < quantity; ++i) {
                response.append(static_cast<char>(0));
                response.append(static_cast<char>(0x2A + i));
            }
        } else {
            const quint16 quantity = (static_cast<quint8>(request.at(4)) << 8)
                | static_cast<quint8>(request.at(5));
            response.append(static_cast<char>(unit));
            response.append(static_cast<char>(function));
            response.append(static_cast<char>(quantity * 2));
            for (quint16 i = 0; i < quantity; ++i) {
                response.append(static_cast<char>(0));
                response.append(static_cast<char>(0x2A + i));
            }
            const quint16 crc = ModbusCodec::crc16(response);
            response.append(static_cast<char>(crc & 0xFF));
            response.append(static_cast<char>((crc >> 8) & 0xFF));
        }
        QTimer::singleShot(5, this, [this, response]() {
            if (m_open) emit dataReceived(response);
        });
        return true;
    }
private:
    bool m_open = false;
};

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
    QCoreApplication::processEvents();
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
    QCoreApplication::processEvents();
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

    CommunicationManager manager;
    DeviceInfo loopbackDevice;
    loopbackDevice.devId = QStringLiteral("MODBUS-RTU-LOOPBACK");
    loopbackDevice.commType = QStringLiteral("RS485");
    ModbusMaster boundMaster(Transport::Rtu);
    if (!manager.registerFactory(QStringLiteral("MODBUS-LOOPBACK"),
            [](QObject *parent) { return new ModbusLoopbackCommunication(parent); })) return 11;
    if (!manager.connectDevice(loopbackDevice)
        || !boundMaster.bindDevice(&manager, loopbackDevice.devId, 1)) return 12;
    bool boundComplete = false;
    QObject::connect(&boundMaster, &ModbusMaster::requestCompleted,
                     [&](quint64, const Response &r) {
        boundComplete = r.ok && r.registers.size() == 2
            && r.registers.at(0) == 0x2A && r.registers.at(1) == 0x2B;
    });
    boundMaster.setTimeoutMs(500);
    if (boundMaster.readHoldingRegisters(0, 2) == 0) return 13;
    QEventLoop boundLoop;
    QTimer::singleShot(1000, &boundLoop, &QEventLoop::quit);
    QObject::connect(&boundMaster, &ModbusMaster::requestCompleted,
                     &boundLoop, &QEventLoop::quit);
    boundLoop.exec();
    if (!boundComplete) return 14;
    manager.disconnectAll();
    return 0;
}
