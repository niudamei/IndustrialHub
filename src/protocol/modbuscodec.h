#ifndef MODBUSCODEC_H
#define MODBUSCODEC_H

#include <QByteArray>
#include <QString>
#include <QMetaType>
#include <QVector>
#include <QtGlobal>

namespace ModbusCodec {

enum class Transport { Rtu, Tcp };

struct Response
{
    bool ok = false;
    bool exception = false;
    quint8 unitId = 0;
    quint8 function = 0;
    quint8 exceptionCode = 0;
    quint16 address = 0;
    quint16 value = 0;
    QVector<quint16> registers;
    QString error;
};

quint16 crc16(const QByteArray &data);
QByteArray readRequest(Transport transport, quint8 unitId, quint8 function,
                       quint16 startAddress, quint16 quantity,
                       quint16 transactionId = 1, QString *error = nullptr);
QByteArray writeSingleRegisterRequest(Transport transport, quint8 unitId,
                                      quint16 address, quint16 value,
                                      quint16 transactionId = 1,
                                      QString *error = nullptr);
QByteArray writeMultipleRegistersRequest(Transport transport, quint8 unitId,
                                         quint16 startAddress,
                                         const QVector<quint16> &values,
                                         quint16 transactionId = 1,
                                         QString *error = nullptr);
Response decodeResponse(Transport transport, const QByteArray &frame,
                        quint8 expectedUnitId, quint8 expectedFunction,
                        quint16 expectedTransactionId = 1);

} // namespace ModbusCodec

Q_DECLARE_METATYPE(ModbusCodec::Response)

#endif // MODBUSCODEC_H
