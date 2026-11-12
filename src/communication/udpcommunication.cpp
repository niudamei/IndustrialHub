#include "udpcommunication.h"

#include <QAbstractSocket>

UdpCommunication::UdpCommunication(QObject *parent)
    : ICommunication(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead,
            this, &UdpCommunication::readPendingDatagrams);
    connect(&m_socket, &QUdpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        emit errorOccurred(QStringLiteral("UDP 通信错误：%1").arg(m_socket.errorString()));
    });
}

UdpCommunication::~UdpCommunication()
{
    close();
}

bool UdpCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (device.devId.trimmed().isEmpty())
        return setError(QStringLiteral("UDP 打开失败：设备编号不能为空。"), errorMessage);

    QHostAddress host;
    quint16 port = 0;
    if (!parseAddress(device.devAddr, host, port, errorMessage))
        return false;

    if (m_open && m_remoteAddress == host && m_remotePort == port)
        return true;

    close();

    const QHostAddress bindAddress = host.protocol() == QAbstractSocket::IPv6Protocol
                                         ? QHostAddress::AnyIPv6
                                         : QHostAddress::AnyIPv4;
    if (!m_socket.bind(bindAddress, 0)) {
        return setError(QStringLiteral("UDP 本地端口绑定失败：%1")
                            .arg(m_socket.errorString()), errorMessage);
    }

    m_remoteAddress = host;
    m_remotePort = port;
    m_open = true;
    emit connectionStateChanged(true);
    return true;
}

void UdpCommunication::close()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState || m_socket.isOpen())
        m_socket.close();

    const bool wasOpen = m_open;
    m_open = false;
    m_remoteAddress.clear();
    m_remotePort = 0;

    if (wasOpen)
        emit connectionStateChanged(false);
}

bool UdpCommunication::isOpen() const
{
    return m_open && m_socket.isOpen();
}

bool UdpCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("UDP 发送失败：socket 尚未打开。"), errorMessage);

    const qint64 written = m_socket.writeDatagram(data, m_remoteAddress, m_remotePort);
    if (written != data.size()) {
        return setError(QStringLiteral("UDP 数据报发送失败：%1")
                            .arg(m_socket.errorString()), errorMessage);
    }
    return true;
}

bool UdpCommunication::parseAddress(const QString &address,
                                    QHostAddress &host,
                                    quint16 &port,
                                    QString *errorMessage)
{
    const QString value = address.trimmed();
    QString hostText;
    QString portText;

    if (value.startsWith(QLatin1Char('['))) {
        const qsizetype closingBracket = value.indexOf(QLatin1Char(']'));
        if (closingBracket <= 1 || closingBracket + 1 >= value.size()
            || value.at(closingBracket + 1) != QLatin1Char(':')) {
            return setError(QStringLiteral("UDP IPv6 地址格式错误，应为 [IPv6]:端口。"),
                            errorMessage);
        }
        hostText = value.mid(1, closingBracket - 1);
        portText = value.mid(closingBracket + 2);
    } else {
        const qsizetype separator = value.lastIndexOf(QLatin1Char(':'));
        if (separator <= 0 || separator == value.size() - 1) {
            return setError(QStringLiteral("UDP 地址格式错误，应为 IPv4:端口。"),
                            errorMessage);
        }
        hostText = value.left(separator).trimmed();
        portText = value.mid(separator + 1).trimmed();
    }

    bool ok = false;
    const uint parsedPort = portText.toUInt(&ok);
    if (!ok || parsedPort == 0 || parsedPort > 65535
        || !host.setAddress(hostText)) {
        return setError(QStringLiteral("UDP 地址无效：请使用有效的 IPv4/IPv6 数字地址和 1–65535 端口。"),
                        errorMessage);
    }

    port = static_cast<quint16>(parsedPort);
    return true;
}

bool UdpCommunication::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;
    emit errorOccurred(message);
    return false;
}

void UdpCommunication::readPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<qsizetype>(m_socket.pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort = 0;
        const qint64 received = m_socket.readDatagram(datagram.data(), datagram.size(),
                                                       &sender, &senderPort);
        if (received < 0) {
            emit errorOccurred(QStringLiteral("UDP 数据报接收失败：%1")
                                   .arg(m_socket.errorString()));
            continue;
        }

        datagram.resize(static_cast<qsizetype>(received));
        if (sender == m_remoteAddress && senderPort == m_remotePort)
            emit dataReceived(datagram);
    }
}
