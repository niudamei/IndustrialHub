#include "src/communication/rs485communication.h"

#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Rs485Communication communication;

    DeviceInfo device;
    device.devId = QStringLiteral("RS485-TEST-001");
    device.commType = QStringLiteral("RS485");

    QString error;

    device.devAddr = QStringLiteral("COM3,115200,9,N,1");
    if (communication.open(device, &error)) {
        std::cerr << "FAIL: unsupported data bits were accepted." << std::endl;
        return 1;
    }
    if (!error.contains(QStringLiteral("数据位"))) {
        std::cerr << "FAIL: invalid data bits returned an unexpected error: "
                  << error.toStdString() << std::endl;
        return 2;
    }
    std::cout << "PASS: unsupported serial data bits are rejected." << std::endl;

    device.devAddr = QStringLiteral("COM3,0,8,N,1");
    if (communication.open(device, &error)) {
        std::cerr << "FAIL: invalid baud rate was accepted." << std::endl;
        return 3;
    }
    std::cout << "PASS: invalid baud rate is rejected." << std::endl;

    device.devAddr = QStringLiteral("COM3,115200,8,N,1,X");
    if (communication.open(device, &error)) {
        std::cerr << "FAIL: invalid flow control was accepted." << std::endl;
        return 4;
    }
    std::cout << "PASS: invalid flow control is rejected." << std::endl;
    std::cout << "RS485 CONFIGURATION TESTS PASSED." << std::endl;
    return 0;
}
