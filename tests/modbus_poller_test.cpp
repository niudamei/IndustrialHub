#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>

#include "src/protocol/modbusmaster.h"
#include "src/protocol/modbuspoller.h"

using namespace ModbusCodec;

static QByteArray makeReadResponse(const QByteArray &request, quint16 value)
{
    QByteArray response;
    response.append(request.at(0));
    response.append(request.at(1));
    response.append(static_cast<char>(2));
    response.append(static_cast<char>((value >> 8) & 0xFF));
    response.append(static_cast<char>(value & 0xFF));
    const quint16 crc = crc16(response);
    response.append(static_cast<char>(crc & 0xFF));
    response.append(static_cast<char>((crc >> 8) & 0xFF));
    return response;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ModbusMaster master(Transport::Rtu);
    QByteArray lastRequest;
    master.setSendFunction([&lastRequest](const QByteArray &frame, QString *) {
        lastRequest = frame;
        return true;
    });

    ModbusPoller poller(&master, QStringLiteral("MODBUS-POLL-01"));
    QVector<ModbusRegisterMap::Mapping> mappings;
    ModbusRegisterMap::Mapping temperature;
    temperature.variableId = QStringLiteral("temperature");
    temperature.name = QStringLiteral("温度");
    temperature.unit = QStringLiteral("°C");
    temperature.address = 10;
    temperature.function = 0x03;
    temperature.dataType = ModbusRegisterMap::DataType::UInt16;
    temperature.scale = 0.1;
    mappings.append(temperature);

    ModbusRegisterMap::Mapping current = temperature;
    current.variableId = QStringLiteral("current");
    current.name = QStringLiteral("电流");
    current.unit = QStringLiteral("A");
    current.address = 20;
    current.function = 0x04;
    current.scale = 0.01;
    mappings.append(current);

    QString error;
    if (!poller.setMappings(mappings, &error)) return 1;
    auto duplicate = mappings;
    duplicate[1].variableId = duplicate[0].variableId;
    if (poller.setMappings(duplicate, &error)) return 2;

    QVector<BusinessDataPoint> points;
    int cycles = 0;
    QObject::connect(&poller, &ModbusPoller::pointsReady,
                     [&](const QString &, const QVector<BusinessDataPoint> &batch) {
        points += batch;
    });
    QObject::connect(&poller, &ModbusPoller::cycleFinished,
                     [&](int success, int failed) {
        if (success == 2 && failed == 0) ++cycles;
    });

    poller.start(5000);
    QCoreApplication::processEvents();
    if (lastRequest.isEmpty() || !poller.isPolling()) return 3;
    master.feedReceivedData(makeReadResponse(lastRequest, 250));
    QCoreApplication::processEvents();
    if (lastRequest.isEmpty() || points.size() != 1) return 4;
    master.feedReceivedData(makeReadResponse(lastRequest, 1234));
    QCoreApplication::processEvents();
    if (cycles != 1 || points.size() != 2) return 5;
    if (qAbs(points.at(0).value - 25.0) > 0.0001
        || qAbs(points.at(1).value - 12.34) > 0.0001
        || points.at(0).deviceId != QStringLiteral("MODBUS-POLL-01")
        || points.at(0).sourceProtocol != QStringLiteral("Modbus RTU")) return 6;

    poller.stop();
    if (poller.isRunning() || poller.isPolling()) return 7;
    return 0;
}
