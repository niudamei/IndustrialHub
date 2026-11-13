#include <QCoreApplication>
#include <QtMath>
#include "src/protocol/modbusregistermap.h"

using namespace ModbusRegisterMap;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString error;
    BusinessDataPoint point;
    const QDateTime now = QDateTime::currentDateTime();

    Mapping temperature;
    temperature.variableId = QStringLiteral("temperature");
    temperature.name = QStringLiteral("温度");
    temperature.unit = QStringLiteral("°C");
    temperature.address = 100;
    temperature.dataType = DataType::Int16;
    temperature.scale = 0.1;
    if (!decode(temperature, {static_cast<quint16>(-125)}, "MODBUS-1",
                "Modbus RTU", now, point, &error)) return 1;
    if (qAbs(point.value - (-12.5)) > 0.0001
        || point.quality != DataQuality::Good
        || point.sourceProtocol != QStringLiteral("Modbus RTU")) return 2;

    Mapping counter = temperature;
    counter.variableId = QStringLiteral("counter");
    counter.name = QStringLiteral("计数");
    counter.dataType = DataType::UInt32;
    counter.scale = 1.0;
    if (!decode(counter, {0x0001, 0x2345}, "MODBUS-1", "Modbus TCP", now, point, &error)
        || point.value != 0x00012345) return 3;

    counter.wordOrder = WordOrder::CDAB;
    if (!decode(counter, {0x2345, 0x0001}, "MODBUS-1", "Modbus TCP", now, point, &error)
        || point.value != 0x00012345) return 4;

    Mapping floating = temperature;
    floating.variableId = QStringLiteral("voltage");
    floating.name = QStringLiteral("电压");
    floating.unit = QStringLiteral("V");
    floating.dataType = DataType::Float32;
    floating.scale = 2.0;
    if (!decode(floating, {0x3F80, 0x0000}, "MODBUS-1", "Modbus TCP", now, point, &error)
        || qAbs(point.value - 2.0) > 0.0001) return 5;

    if (decode(temperature, {}, "MODBUS-1", "Modbus RTU", now, point, &error)) return 6;
    Mapping invalid = temperature;
    invalid.function = 0x06;
    if (validate(invalid, &error)) return 7;
    invalid = temperature;
    invalid.scale = qQNaN();
    if (validate(invalid, &error)) return 8;
    if (decode(temperature, {1}, QString(), "Modbus RTU", now, point, &error)) return 9;
    return 0;
}
