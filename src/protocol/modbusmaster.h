#ifndef MODBUSMASTER_H
#define MODBUSMASTER_H

#include "modbuscodec.h"

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <functional>

class ModbusMaster : public QObject
{
    Q_OBJECT
public:
    using SendFunction = std::function<bool(const QByteArray &, QString *)>;

    explicit ModbusMaster(ModbusCodec::Transport transport, QObject *parent = nullptr);

    void setSendFunction(SendFunction sender);
    void setUnitId(quint8 unitId);
    quint8 unitId() const;
    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const;
    int pendingCount() const;
    bool isBusy() const;

    quint64 readHoldingRegisters(quint16 startAddress, quint16 quantity);
    quint64 readInputRegisters(quint16 startAddress, quint16 quantity);
    quint64 writeSingleRegister(quint16 address, quint16 value);
    quint64 writeMultipleRegisters(quint16 startAddress, const QVector<quint16> &values);

public slots:
    void feedReceivedData(const QByteArray &data);
    void cancelAll(const QString &reason = QStringLiteral("Modbus 请求已取消。"));

signals:
    void requestCompleted(quint64 requestId, const ModbusCodec::Response &response);
    void requestFailed(quint64 requestId, const QString &reason);
    void queueChanged(int pendingCount);
    void busyChanged(bool busy);

private slots:
    void onTimeout();

private:
    struct Request {
        quint64 id = 0;
        quint8 function = 0;
        quint16 address = 0;
        quint16 valueOrQuantity = 0;
        QVector<quint16> values;
        QByteArray frame;
        quint16 transactionId = 0;
    };

    quint64 enqueue(quint8 function, quint16 address,
                    quint16 valueOrQuantity, const QVector<quint16> &values = {});
    void sendNext();
    bool extractOneFrame(QByteArray &frame);
    void processFrame(const QByteArray &frame);
    void finishActive(const ModbusCodec::Response &response);
    void failActive(const QString &reason);

    ModbusCodec::Transport m_transport;
    SendFunction m_sender;
    quint8 m_unitId = 1;
    int m_timeoutMs = 1000;
    quint64 m_nextRequestId = 1;
    quint16 m_nextTransactionId = 1;
    QQueue<Request> m_queue;
    Request m_active;
    bool m_hasActive = false;
    QByteArray m_receiveBuffer;
    QTimer m_timeoutTimer;
};

#endif // MODBUSMASTER_H
