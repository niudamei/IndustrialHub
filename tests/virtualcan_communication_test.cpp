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
    return 0;
}
