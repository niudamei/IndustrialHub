#include "tcpcommunication.h"

#include <QAbstractSocket>

TcpCommunication::TcpCommunication(QObject *parent)
    : ICommunication(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_connected = true;
        emit connectionStateChanged(true);
    });

    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        if (m_connected) {
            m_connected = false;
            emit connectionStateChanged(false);
        }
    });

    connect(&m_socket, &QTcpSocket::readyRead, this, [this]() {
        const QByteArray data = m_socket.readAll();
        if (!data.isEmpty())
            emit dataReceived(data);
    });

    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        emit errorOccurred(QStringLiteral("TCP 通信错误：%1").arg(m_socket.errorString()));
    });
}

TcpCommunication::~TcpCommunication()
{
    close();
}

bool TcpCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (device.devId.trimmed().isEmpty())
        return setError(QStringLiteral("TCP 连接失败：设备编号不能为空。"), errorMessage);

    QString host;
    quint16 port = 0;
    if (!parseAddress(device.devAddr, host, port, errorMessage))
        return false;

    if (m_socket.state() == QAbstractSocket::ConnectedState
        && m_host == host && m_port == port) {
        return true;
    }

    if (m_socket.state() == QAbstractSocket::ConnectingState
        && m_host == host && m_port == port) {
        return true;
    }

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();

    m_host = host;
    m_port = port;
    m_socket.connectToHost(m_host, m_port);
    return true;
}

void TcpCommunication::close()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();

    m_host.clear();
    m_port = 0;
}

bool TcpCommunication::isOpen() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool TcpCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("TCP 发送失败：设备尚未连接。"), errorMessage);

    const qint64 written = m_socket.write(data);
    if (written < 0)
        return setError(QStringLiteral("TCP 发送失败：%1").arg(m_socket.errorString()),
                        errorMessage);

    if (written != data.size())
        return setError(QStringLiteral("TCP 发送失败：数据未能完整写入发送缓冲区。"),
                        errorMessage);

    return true;
}

bool TcpCommunication::parseAddress(const QString &address,
                                    QString &host,
                                    quint16 &port,
                                    QString *errorMessage)
{
    const QString value = address.trimmed();
    QString portText;

    if (value.startsWith(QLatin1Char('['))) {
        const qsizetype closingBracket = value.indexOf(QLatin1Char(']'));
        if (closingBracket <= 1 || closingBracket + 1 >= value.size()
            || value.at(closingBracket + 1) != QLatin1Char(':')) {
            return setError(QStringLiteral("TCP 地址格式错误，应为 [IPv6]:端口。"),
                            errorMessage);
        }
        host = value.mid(1, closingBracket - 1);
        portText = value.mid(closingBracket + 2);
    } else {
        const qsizetype separator = value.lastIndexOf(QLatin1Char(':'));
        if (separator <= 0 || separator == value.size() - 1) {
            return setError(QStringLiteral("TCP 地址格式错误，应为 主机名:端口。"),
                            errorMessage);
        }
        host = value.left(separator).trimmed();
        portText = value.mid(separator + 1).trimmed();
    }

    bool ok = false;
    const uint parsedPort = portText.toUInt(&ok);
    if (host.isEmpty() || !ok || parsedPort == 0 || parsedPort > 65535) {
        return setError(QStringLiteral("TCP 地址无效：主机名不能为空，端口范围为 1–65535。"),
                        errorMessage);
    }

    port = static_cast<quint16>(parsedPort);
    return true;
}

bool TcpCommunication::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;

    emit errorOccurred(message);
    return false;
}
