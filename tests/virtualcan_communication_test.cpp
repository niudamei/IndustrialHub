#include "src/communication/virtualcancommunication.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    VirtualCanCommunication sender(false);
    VirtualCanCommunication receiver(false);

    DeviceInfo tx(QStringLiteral("VIRTUAL-TX"), QStringLiteral("Virtual TX"),
                  QStringLiteral("CAN"), QStringLiteral("VIRTUALCAN"),
                  QStringLiteral("can0"), DeviceStatus::Offline);
    DeviceInfo rx(QStringLiteral("VIRTUAL-RX"), QStringLiteral("Virtual RX"),
                  QStringLiteral("CAN"), QStringLiteral("VIRTUALCAN"),
                  QStringLiteral("can0"), DeviceStatus::Offline);
    QString error;
    if (!sender.open(tx, &error) || !receiver.open(rx, &error)) {
        std::cerr << "SKIP/FAIL: VirtualCAN unavailable: " << error.toStdString() << std::endl;
        return 2;
    }

    QByteArray received;
    QEventLoop loop;
    QObject::connect(&receiver, &ICommunication::dataReceived, &loop,
                     [&received, &loop](const QByteArray &data) {
        received = data;
        loop.quit();
    });
    if (!sender.sendData(QByteArray("T18AABBCC3112233\r"), &error)) {
        std::cerr << "FAIL: VirtualCAN send: " << error.toStdString() << std::endl;
        return 3;
    }
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    loop.exec();

    sender.close();
    receiver.close();
    if (received != QByteArray("T18AABBCC3112233\r")) {
        std::cerr << "FAIL: expected SLCAN frame not received, got: "
                  << received.toStdString() << std::endl;
        return 4;
    }
    std::cout << "PASS: two independent VirtualCAN clients exchanged a classic CAN frame."
              << std::endl;

    VirtualCanCommunication fdSender(true);
    VirtualCanCommunication fdReceiver(true);
    DeviceInfo fdTx(QStringLiteral("VIRTUAL-FD-TX"), QStringLiteral("Virtual FD TX"),
                    QStringLiteral("CAN-FD"), QStringLiteral("VIRTUALCANFD"),
                    QStringLiteral("can1"), DeviceStatus::Offline);
    DeviceInfo fdRx(QStringLiteral("VIRTUAL-FD-RX"), QStringLiteral("Virtual FD RX"),
                    QStringLiteral("CAN-FD"), QStringLiteral("VIRTUALCANFD"),
                    QStringLiteral("can1"), DeviceStatus::Offline);
    if (!fdSender.open(fdTx, &error) || !fdReceiver.open(fdRx, &error)) {
        std::cerr << "FAIL: VirtualCAN FD open: " << error.toStdString() << std::endl;
        return 5;
    }
    QByteArray fdReceived;
    QEventLoop fdLoop;
    QObject::connect(&fdReceiver, &ICommunication::dataReceived, &fdLoop,
                     [&fdReceived, &fdLoop](const QByteArray &data) {
        fdReceived = data;
        fdLoop.quit();
    });
    const QByteArray fdFrame("B7E09112233445566778899AABBCC\\r");
    if (!fdSender.sendData(fdFrame, &error)) {
        std::cerr << "FAIL: VirtualCAN FD send: " << error.toStdString() << std::endl;
        return 6;
    }
    QTimer::singleShot(1500, &fdLoop, &QEventLoop::quit);
    fdLoop.exec();
    fdSender.close();
    fdReceiver.close();
    if (fdReceived != fdFrame) {
        std::cerr << "FAIL: expected CAN-FD/BRS frame not received, got: "
                  << fdReceived.toStdString() << std::endl;
        return 7;
    }
    std::cout << "PASS: two VirtualCAN FD clients exchanged a 12-byte BRS frame."
              << std::endl;
    return 0;
}
