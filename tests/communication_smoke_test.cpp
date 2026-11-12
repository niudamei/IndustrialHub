#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

#include "src/communication/simulatedcommunication.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    SimulatedCommunication communication;
    bool connectedSignalReceived = false;
    bool disconnectedSignalReceived = false;
    bool errorSignalReceived = false;
    QByteArray receivedData;

    QObject::connect(&communication, &ICommunication::connectionStateChanged,
                     [&connectedSignalReceived, &disconnectedSignalReceived](bool connected) {
        if (connected)
            connectedSignalReceived = true;
        else
            disconnectedSignalReceived = true;
    });

    QObject::connect(&communication, &ICommunication::dataReceived,
                     [&receivedData](const QByteArray &data) {
        receivedData = data;
    });

    QObject::connect(&communication, &ICommunication::errorOccurred,
                     [&errorSignalReceived](const QString &message) {
        errorSignalReceived = true;
        qWarning().noquote() << "Expected error signal:" << message;
    });

    // 1. Sending before opening must fail and emit an error.
    QString error;
    if (communication.sendData(QByteArray::fromHex("0102"), &error)) {
        qCritical() << "FAIL: sendData unexpectedly succeeded before open().";
        return 1;
    }
    if (error.isEmpty() || !errorSignalReceived) {
        qCritical() << "FAIL: missing error message or errorOccurred signal.";
        return 2;
    }
    qInfo() << "PASS: sending before open is rejected.";

    // 2. Open a simulated device and verify the connection signal.
    DeviceInfo device;
    device.devId = QStringLiteral("TEST-001");
    device.devName = QStringLiteral("Communication loopback test");
    if (!communication.open(device, &error) || !communication.isOpen()) {
        qCritical().noquote() << "FAIL: open() failed:" << error;
        return 3;
    }
    if (!connectedSignalReceived) {
        qCritical() << "FAIL: connectionStateChanged(true) was not emitted.";
        return 4;
    }
    qInfo() << "PASS: simulated device opened.";

    // 3. Send bytes and wait for the simulator to echo them asynchronously.
    const QByteArray payload = QByteArray::fromHex("010203A0FF");
    if (!communication.sendData(payload, &error)) {
        qCritical().noquote() << "FAIL: sendData() failed:" << error;
        return 5;
    }

    QEventLoop waitLoop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&communication, &ICommunication::dataReceived,
                     &waitLoop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,
                     &waitLoop, &QEventLoop::quit);
    timeout.start(1000);
    waitLoop.exec();

    if (receivedData != payload) {
        qCritical() << "FAIL: loopback data mismatch or timeout."
                    << "Expected:" << payload.toHex()
                    << "Received:" << receivedData.toHex();
        return 6;
    }
    qInfo() << "PASS: asynchronous loopback data:"
            << receivedData.toHex(' ');

    // 4. Close and verify the state transition.
    communication.close();
    if (communication.isOpen() || !disconnectedSignalReceived) {
        qCritical() << "FAIL: close() did not update state or emit signal.";
        return 7;
    }
    qInfo() << "PASS: simulated device closed.";
    qInfo() << "ALL COMMUNICATION SMOKE TESTS PASSED.";
    return 0;
}
