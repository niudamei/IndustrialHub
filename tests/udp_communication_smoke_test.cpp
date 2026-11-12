#include "src/communication/udpcommunication.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QUdpSocket>
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

    QUdpSocket server;
    if (!server.bind(QHostAddress::LocalHost, 0)) {
        std::cerr << "FAIL: local UDP server bind failed." << std::endl;
        return 1;
    }

    QObject::connect(&server, &QUdpSocket::readyRead, &server, [&]() {
        while (server.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(static_cast<qsizetype>(server.pendingDatagramSize()));
            QHostAddress sender;
            quint16 senderPort = 0;
            const qint64 size = server.readDatagram(datagram.data(), datagram.size(),
                                                     &sender, &senderPort);
            if (size >= 0) {
                datagram.resize(static_cast<qsizetype>(size));
                server.writeDatagram(datagram, sender, senderPort);
            }
        }
    });

    UdpCommunication communication;
    bool connected = false;
    bool disconnected = false;
    QByteArray received;
    QObject::connect(&communication, &ICommunication::connectionStateChanged,
                     [&](bool state) {
        if (state) connected = true;
        else disconnected = true;
    });
    QObject::connect(&communication, &ICommunication::dataReceived,
                     [&](const QByteArray &data) { received.append(data); });

    DeviceInfo device;
    device.devId = QStringLiteral("UDP-TEST-001");
    device.commType = QStringLiteral("UDP");
    device.devAddr = QStringLiteral("127.0.0.1:%1").arg(server.localPort());

    QString error;
    if (!communication.open(device, &error) || !connected || !communication.isOpen()) {
        std::cerr << "FAIL: UDP open failed: " << error.toStdString() << std::endl;
        return 2;
    }

    const QByteArray payload = QByteArray::fromHex("DEADBEEF010203");
    if (!communication.sendData(payload, &error)) {
        std::cerr << "FAIL: UDP send failed: " << error.toStdString() << std::endl;
        return 3;
    }

    if (!waitUntil([&]() { return received == payload; }, 2000)) {
        std::cerr << "FAIL: UDP loopback receive timeout or payload mismatch." << std::endl;
        return 4;
    }
    std::cout << "PASS: UDP datagram loopback: "
              << received.toHex(' ').toStdString() << std::endl;

    communication.close();
    if (communication.isOpen() || !disconnected) {
        std::cerr << "FAIL: UDP close state was not reported." << std::endl;
        return 5;
    }

    device.devAddr = QStringLiteral("127.0.0.1:70000");
    if (communication.open(device, &error)) {
        std::cerr << "FAIL: invalid UDP port was accepted." << std::endl;
        return 6;
    }

    std::cout << "ALL UDP COMMUNICATION SMOKE TESTS PASSED." << std::endl;
    return 0;
}
