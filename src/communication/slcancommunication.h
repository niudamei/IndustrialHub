#ifndef SLCANCOMMUNICATION_H
#define SLCANCOMMUNICATION_H

#include "icommunication.h"

#include <QSerialPort>
#include "slcanframecodec.h"

// CANable 2.0 SLCAN 串口通信。
// DeviceInfo.commType 为 CAN 或 CANFD。
// 设备地址：COM4,1000000（经典 CAN）或 COM4,1000000,2000000（CAN-FD）。
class SlcanCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit SlcanCommunication(bool canFdMode, QObject *parent = nullptr);
    ~SlcanCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data,
                  QString *errorMessage = nullptr) override;

private:
    struct AdapterSettings
    {
        QString portName;
        int nominalBitrateIndex = 8;
        int dataBitrateIndex = 2;
    };

    bool parseAddress(const QString &address,
                      AdapterSettings &settings,
                      QString *errorMessage);
    bool writeCommand(const QByteArray &command, QString *errorMessage);
    bool setError(const QString &message, QString *errorMessage);
    void readSerialData();

    QSerialPort m_serialPort;
    QByteArray m_receiveBuffer;
    bool m_canFdMode = false;
    bool m_open = false;
};

#endif // SLCANCOMMUNICATION_H
