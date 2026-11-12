#ifndef UDPCOMMUNICATION_H
#define UDPCOMMUNICATION_H

#include "icommunication.h"

#include <QHostAddress>
#include <QUdpSocket>

// UDP 通信实现。设备地址使用 IPv4:端口 或 [IPv6]:端口。
// UDP 无连接握手；open() 成功表示本地 UDP socket 已绑定并可收发数据报。
class UdpCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit UdpCommunication(QObject *parent = nullptr);
    ~UdpCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data,
                  QString *errorMessage = nullptr) override;

private:
    bool parseAddress(const QString &address,
                      QHostAddress &host,
                      quint16 &port,
                      QString *errorMessage);
    bool setError(const QString &message, QString *errorMessage);
    void readPendingDatagrams();

    QUdpSocket m_socket;
    QHostAddress m_remoteAddress;
    quint16 m_remotePort = 0;
    bool m_open = false;
};

#endif // UDPCOMMUNICATION_H
