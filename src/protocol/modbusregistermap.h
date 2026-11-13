#ifndef MODBUSREGISTERMAP_H
#define MODBUSREGISTERMAP_H

#include "src/protocol/businessdatapoint.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace ModbusRegisterMap {

enum class DataType { UInt16, Int16, UInt32, Int32, Float32 };
enum class WordOrder { ABCD, CDAB, BADC, DCBA };

struct Mapping
{
    QString variableId;
    QString name;
    QString unit;
    quint16 address = 0;
    quint8 function = 0x03;
    DataType dataType = DataType::UInt16;
    WordOrder wordOrder = WordOrder::ABCD;
    double scale = 1.0;
    double offset = 0.0;
};

bool validate(const Mapping &mapping, QString *error = nullptr);
bool decode(const Mapping &mapping, const QVector<quint16> &registers,
            const QString &deviceId, const QString &sourceProtocol,
            const QDateTime &timestamp, BusinessDataPoint &point,
            QString *error = nullptr);

} // namespace ModbusRegisterMap

#endif // MODBUSREGISTERMAP_H
