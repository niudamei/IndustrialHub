#include "src/communication/tcpcommunication.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <iostream>
#include <functional>

static bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return condition();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::cerr << "FAIL: local TCP test server could not listen: "
                  << server.errorString().toStdString() << std::endl;
        return 1;
    }

    QTcpSocket *peer = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [peer]() {
            peer->write(peer->readAll());
        });
    });

    TcpCommunication communication;
    bool connectedSignal = false;
    bool disconnectedSignal = false;
    QByteArray receivedData;

    QObject::connect(&communication, &ICommunication::connectionStateChanged,
                     [&](bool connected) {
        if (connected)
            connectedSignal = true;
        else
            disconnectedSignal = true;
    });
    QObject::connect(&communication, &ICommunication::dataReceived,
                     [&](const QByteArray &data) {
        receivedData.append(data);
    });

    DeviceInfo device;
    device.devId = QStringLiteral("TCP-TEST-001");
    device.devName = QStringLiteral("本地 TCP 回环测试");
    device.commType = QStringLiteral("TCP");
    device.devAddr = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());

    QString error;
    if (!communication.open(device, &error)) {
        std::cerr << "FAIL: TCP open rejected: " << error.toStdString() << std::endl;
        return 2;
    }

    if (!waitUntil([&]() { return connectedSignal && communication.isOpen(); }, 2000)) {
        std::cerr << "FAIL: TCP connection timeout." << std::endl;
        return 3;
    }
    std::cout << "PASS: connected to local TCP server." << std::endl;

    const QByteArray payload = QByteArray::fromHex("010203A0FF");
    if (!communication.sendData(payload, &error)) {
        std::cerr << "FAIL: TCP send failed: " << error.toStdString() << std::endl;
        return 4;
    }

    if (!waitUntil([&]() { return receivedData.size() >= payload.size(); }, 2000)
        || receivedData != payload) {
        std::cerr << "FAIL: TCP loopback payload mismatch." << std::endl;
        return 5;
    }
    std::cout << "PASS: TCP loopback data: "
              << receivedData.toHex(' ').toStdString() << std::endl;

    communication.close();
    if (communication.isOpen() || !disconnectedSignal) {
        std::cerr << "FAIL: TCP disconnect state was not reported." << std::endl;
        return 6;
    }
    std::cout << "PASS: TCP connection closed." << std::endl;

    DeviceInfo invalidDevice = device;
    invalidDevice.devAddr = QStringLiteral("127.0.0.1:70000");
    if (communication.open(invalidDevice, &error)) {
        std::cerr << "FAIL: invalid TCP port was accepted." << std::endl;
        return 7;
    }
    std::cout << "PASS: invalid TCP port was rejected." << std::endl;

    std::cout << "ALL TCP COMMUNICATION SMOKE TESTS PASSED." << std::endl;
    return 0;
}
