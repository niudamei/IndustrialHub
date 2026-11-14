#include <QCoreApplication>
#include <QEventLoop>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QTimer>
#include <QDebug>

#include "src/simulation/modbustcpsimulator.h"

static bool waitForReply(QModbusReply *reply, int timeoutMs = 2500)
{
    if (!reply) return false;
    if (reply->isFinished()) return reply->error() == QModbusDevice::NoError;
    QEventLoop loop;
    QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    const bool ok = reply->isFinished() && reply->error() == QModbusDevice::NoError;
    return ok;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ModbusTcpSimulator simulator;
    QString error;
    if (!simulator.start(QStringLiteral("127.0.0.1"), 15020, 7, &error)) {
        qCritical() << "FAIL: simulator start:" << error;
        return 1;
    }

    QModbusTcpClient client;
    client.setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                  QStringLiteral("127.0.0.1"));
    client.setConnectionParameter(QModbusDevice::NetworkPortParameter, 15020);
    client.setTimeout(1000);
    client.setNumberOfRetries(0);
    if (!client.connectDevice()) {
        qCritical() << "FAIL: client connect:" << client.errorString();
        return 2;
    }

    QEventLoop connectedLoop;
    if (client.state() != QModbusDevice::ConnectedState) {
        QObject::connect(&client, &QModbusClient::stateChanged, &connectedLoop,
                         [&connectedLoop](QModbusDevice::State state) {
            if (state == QModbusDevice::ConnectedState
                || state == QModbusDevice::UnconnectedState)
                connectedLoop.quit();
        });
        QTimer::singleShot(2500, &connectedLoop, &QEventLoop::quit);
        connectedLoop.exec();
    }
    if (client.state() != QModbusDevice::ConnectedState) {
        qCritical() << "FAIL: client did not connect:" << client.errorString();
        return 3;
    }

    QModbusDataUnit readUnit(QModbusDataUnit::InputRegisters, 0, 4);
    QModbusReply *readReply = client.sendReadRequest(readUnit, 7);
    if (!waitForReply(readReply)) {
        qCritical() << "FAIL: read request:" << (readReply ? readReply->errorString() : "null");
        if (readReply) readReply->deleteLater();
        return 4;
    }
    const QModbusDataUnit readResult = readReply->result();
    if (readResult.valueCount() != 4 || readResult.value(0) != 253
        || readResult.value(1) != 125 || readResult.value(2) != 1200) {
        qCritical() << "FAIL: unexpected register values";
        readReply->deleteLater();
        return 5;
    }
    readReply->deleteLater();

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 0, 1);
    writeUnit.setValue(0, 4321);
    QModbusReply *writeReply = client.sendWriteRequest(writeUnit, 7);
    if (!waitForReply(writeReply)) {
        qCritical() << "FAIL: write request:" << (writeReply ? writeReply->errorString() : "null");
        if (writeReply) writeReply->deleteLater();
        return 6;
    }
    writeReply->deleteLater();
    quint16 written = 0;
    if (!simulator.registerValue(QModbusDataUnit::HoldingRegisters, 0, &written)
        || written != 4321) {
        qCritical() << "FAIL: write was not applied to server map";
        return 7;
    }

    client.disconnectDevice();
    simulator.stop();
    qInfo() << "PASS: Modbus TCP simulator read/write";
    return 0;
}
