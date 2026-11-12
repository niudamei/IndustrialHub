#include "src/communication/slcancommunication.h"

#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString error;

    SlcanCommunication classicCan(false);
    DeviceInfo device;
    device.devId = QStringLiteral("CAN-TEST-001");
    device.commType = QStringLiteral("CAN");

    if (classicCan.sendData("T000001000", &error)) {
        std::cerr << "FAIL: SLCAN send succeeded before adapter open." << std::endl;
        return 1;
    }

    device.devAddr = QStringLiteral("COM3,123456");
    if (classicCan.open(device, &error)) {
        std::cerr << "FAIL: unsupported nominal bitrate was accepted." << std::endl;
        return 2;
    }
    std::cout << "PASS: invalid classic CAN bitrate rejected." << std::endl;

    SlcanCommunication canFd(true);
    device.commType = QStringLiteral("CANFD");
    device.devAddr = QStringLiteral("COM3,1000000,1000000");
    if (canFd.open(device, &error)) {
        std::cerr << "FAIL: unsupported CAN-FD data bitrate was accepted." << std::endl;
        return 3;
    }
    std::cout << "PASS: unsupported CAN-FD data bitrate rejected." << std::endl;
    std::cout << "SLCAN COMMUNICATION CONFIGURATION TESTS PASSED." << std::endl;
    return 0;
}
