#ifndef TCPCOMMUNICATION_H
#define TCPCOMMUNICATION_H

#include "icommunication.h"

#include <QTcpSocket>

// TCP 客户端通信实现。
// 设备地址格式：主机名:端口 或 IPv4:端口；IPv6 请使用 [地址]:端口。
// open() 表示连接请求已发起，实际连接成功后通过 connectionStateChanged(true) 通知。
class TcpCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit TcpCommunication(QObject *parent = nullptr);
    ~TcpCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data,
                  QString *errorMessage = nullptr) override;

private:
    bool parseAddress(const QString &address,
                      QString &host,
                      quint16 &port,
                      QString *errorMessage);
    bool setError(const QString &message, QString *errorMessage);

    QTcpSocket m_socket;
    QString m_host;
    quint16 m_port = 0;
    bool m_connected = false;
};

#endif // TCPCOMMUNICATION_H
