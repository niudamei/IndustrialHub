#ifndef RS485COMMUNICATION_H
#define RS485COMMUNICATION_H

#include "icommunication.h"

#include <QSerialPort>

// RS485 串口驱动（通过 Qt SerialPort 实现）。
// 设备地址格式：COM3,115200,8,N,1[,N]
// 字段依次为：串口名、波特率、数据位、校验位、停止位、可选流控。
// 校验位：N 无校验、E 偶校验、O 奇校验、M 标记、S 空格。
// 流控：N 无、H 硬件 RTS/CTS、S 软件 XON/XOFF。
class Rs485Communication : public ICommunication
{
    Q_OBJECT

public:
    explicit Rs485Communication(QObject *parent = nullptr);
    ~Rs485Communication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data,
                  QString *errorMessage = nullptr) override;

private:
    struct SerialSettings
    {
        QString portName;
        qint32 baudRate = 115200;
        QSerialPort::DataBits dataBits = QSerialPort::Data8;
        QSerialPort::Parity parity = QSerialPort::NoParity;
        QSerialPort::StopBits stopBits = QSerialPort::OneStop;
        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
    };

    bool parseAddress(const QString &address,
                      SerialSettings &settings,
                      QString *errorMessage);
    bool setError(const QString &message, QString *errorMessage);

    QSerialPort m_serialPort;
    bool m_open = false;
};

#endif // RS485COMMUNICATION_H
