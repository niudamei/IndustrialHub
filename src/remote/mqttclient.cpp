#include "mqttclient.h"

#include <QRandomGenerator>
#include <utility>

MqttClient::MqttClient(QObject *parent) : QObject(parent), m_socket(new QSslSocket(this))
{
    m_pingTimer.setSingleShot(false);
    m_reconnectTimer.setSingleShot(true);
    connect(m_socket, &QSslSocket::connected, this, &MqttClient::onSocketConnected);
    connect(m_socket, &QSslSocket::readyRead, this, &MqttClient::onReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, &MqttClient::onSocketDisconnected);
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket->errorString());
        if (m_desiredConnection && m_socket->state() == QAbstractSocket::UnconnectedState)
            scheduleReconnect();
    });
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        for (const QSslError &error : errors)
            emit errorOccurred(QStringLiteral("TLS 校验失败：%1").arg(error.errorString()));
    });
    connect(&m_pingTimer, &QTimer::timeout, this, &MqttClient::sendPing);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_desiredConnection) return;
        emit logMessage(QStringLiteral("MQTT 正在尝试自动重连…"));
        if (m_useTls) m_socket->connectToHostEncrypted(m_host, m_port);
        else m_socket->connectToHost(m_host, m_port);
    });
}

bool MqttClient::isConnected() const { return m_mqttConnected; }

void MqttClient::connectToBroker(const QString &host, quint16 port, const QString &clientId,
                                 const QString &username, const QString &password,
                                 bool useTls, int keepAliveSeconds)
{
    disconnectFromBroker();
    m_socket->abort();
    m_buffer.clear();
    m_host = host.trimmed();
    m_port = port;
    m_clientId = clientId.trimmed().isEmpty()
        ? QStringLiteral("IndustrialHub-%1").arg(QString::number(QRandomGenerator::global()->generate(), 16))
        : clientId.trimmed();
    m_username = username;
    m_password = password;
    m_useTls = useTls;
    m_keepAlive = qBound(5, keepAliveSeconds, 3600);
    m_desiredConnection = true;
    m_reconnectAttempt = 0;
    if (m_host.isEmpty() || m_port == 0) {
        emit errorOccurred(QStringLiteral("MQTT 地址或端口无效。"));
        m_desiredConnection = false;
        return;
    }
    emit logMessage(QStringLiteral("连接 MQTT：%1:%2").arg(m_host).arg(m_port));
    if (m_useTls) m_socket->connectToHostEncrypted(m_host, m_port);
    else m_socket->connectToHost(m_host, m_port);
}

void MqttClient::disconnectFromBroker()
{
    m_desiredConnection = false;
    m_reconnectTimer.stop();
    m_pingTimer.stop();
    m_waitingPingResponse = false;
    if (m_mqttConnected) {
        m_socket->write(packet(0xE0, QByteArray())); // MQTT DISCONNECT
        m_socket->flush();
    }
    m_mqttConnected = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

bool MqttClient::publish(const QString &topic, const QByteArray &payload, bool retain)
{
    if (!m_mqttConnected || topic.isEmpty() || topic.contains(QLatin1Char('#'))
        || topic.contains(QLatin1Char('+')) || payload.size() > 1024 * 1024)
        return false;
    QByteArray body = encodeString(topic) + payload;
    m_socket->write(packet(quint8(0x30 | (retain ? 1 : 0)), body));
    return true;
}

bool MqttClient::subscribe(const QString &topicFilter)
{
    const QString filter = topicFilter.trimmed();
    if (!m_mqttConnected || filter.isEmpty())
        return false;
    if (!m_subscriptions.contains(filter))
        m_subscriptions.append(filter);
    sendSubscribePacket(filter);
    return true;
}

void MqttClient::sendSubscribePacket(const QString &topicFilter)
{
    const quint16 id = m_packetId++;
    QByteArray body;
    body.append(char(id >> 8));
    body.append(char(id & 0xff));
    body += encodeString(topicFilter);
    body.append(char(0)); // requested QoS 0
    m_socket->write(packet(0x82, body));
}

void MqttClient::onSocketConnected()
{
    if (!m_desiredConnection) return;
    sendConnectPacket();
}

void MqttClient::sendConnectPacket()
{
    QByteArray body = encodeString(QStringLiteral("MQTT"));
    body.append(char(4)); // MQTT 3.1.1
    quint8 flags = 0x02; // clean session
    if (!m_username.isEmpty()) flags |= 0x80;
    if (!m_password.isEmpty()) flags |= 0x40;
    body.append(char(flags));
    body.append(char(m_keepAlive >> 8));
    body.append(char(m_keepAlive & 0xff));
    body += encodeString(m_clientId);
    if (!m_username.isEmpty()) body += encodeString(m_username);
    if (!m_password.isEmpty()) body += encodeString(m_password);
    m_socket->write(packet(0x10, body));
}

void MqttClient::onReadyRead()
{
    m_buffer += m_socket->readAll();
    while (m_buffer.size() >= 2) {
        int multiplier = 1, remaining = 0, index = 1;
        quint8 digit = 0;
        do {
            if (index >= m_buffer.size()) return;
            digit = quint8(m_buffer.at(index++));
            remaining += (digit & 127) * multiplier;
            multiplier *= 128;
            if (multiplier > 128 * 128 * 128 * 128) {
                emit errorOccurred(QStringLiteral("MQTT 剩余长度字段非法。"));
                m_socket->abort(); return;
            }
        } while (digit & 128);
        if (remaining > 1024 * 1024 + 4096) {
            emit errorOccurred(QStringLiteral("MQTT 数据包超过 1 MiB 限制。"));
            m_socket->abort(); return;
        }
        if (m_buffer.size() < index + remaining) return;
        const quint8 header = quint8(m_buffer.at(0));
        const quint8 type = header >> 4;
        const QByteArray body = m_buffer.mid(index, remaining);
        m_buffer.remove(0, index + remaining);
        if (type == 2) {
            if (body.size() != 2 || quint8(body.at(1)) != 0) {
                emit errorOccurred(QStringLiteral("MQTT CONNACK 拒绝连接，返回码：%1")
                    .arg(body.size() > 1 ? quint8(body.at(1)) : 255));
                m_socket->abort(); return;
            }
            m_mqttConnected = true;
            m_reconnectAttempt = 0;
            m_waitingPingResponse = false;
            m_pingTimer.start(qMax(1000, m_keepAlive * 500));
            emit logMessage(QStringLiteral("MQTT 已连接，协议版本 3.1.1。"));
            for (const QString &filter : std::as_const(m_subscriptions))
                sendSubscribePacket(filter);
            emit connected();
        } else if (type == 3) {
            if (body.size() < 2) continue;
            const int topicLength = (quint8(body.at(0)) << 8) | quint8(body.at(1));
            if (topicLength <= 0 || body.size() < 2 + topicLength) continue;
            const QString topic = QString::fromUtf8(body.constData() + 2, topicLength);
            const int qos = (header >> 1) & 0x03;
            int payloadOffset = 2 + topicLength;
            quint16 packetId = 0;
            if (qos > 0) {
                if (body.size() < payloadOffset + 2) continue;
                packetId = (quint8(body.at(payloadOffset)) << 8) | quint8(body.at(payloadOffset + 1));
                payloadOffset += 2;
            }
            emit messageReceived(topic, body.mid(payloadOffset));
            if (qos == 1) {
                QByteArray ack;
                ack.append(char(packetId >> 8)); ack.append(char(packetId & 0xff));
                m_socket->write(packet(0x40, ack));
            } else if (qos > 1) {
                emit errorOccurred(QStringLiteral("当前客户端只支持 QoS 0/1 消息。"));
            }
        } else if (type == 13) {
            m_waitingPingResponse = false;
        } else if (type == 9) {
            emit logMessage(QStringLiteral("MQTT 订阅请求已确认。"));
        } else if (type == 14) {
            m_mqttConnected = false;
            emit logMessage(QStringLiteral("MQTT Broker 已发送 DISCONNECT。"));
            m_socket->disconnectFromHost();
        }
    }
}

void MqttClient::onSocketDisconnected()
{
    const bool wasConnected = m_mqttConnected;
    m_mqttConnected = false;
    m_pingTimer.stop();
    m_buffer.clear();
    if (wasConnected) emit disconnected();
    if (m_desiredConnection) scheduleReconnect();
}

void MqttClient::sendPing()
{
    if (!m_mqttConnected) return;
    if (m_waitingPingResponse) {
        emit errorOccurred(QStringLiteral("MQTT 心跳响应超时。"));
        m_socket->abort();
        return;
    }
    m_waitingPingResponse = true;
    m_socket->write(QByteArray::fromHex("c000"));
}

void MqttClient::scheduleReconnect()
{
    if (!m_desiredConnection || m_reconnectTimer.isActive()) return;
    const int delay = qMin(1000 * (1 << qMin(m_reconnectAttempt, 5)), 30000);
    ++m_reconnectAttempt;
    emit logMessage(QStringLiteral("MQTT 将在 %1 秒后重连（第 %2 次）。")
        .arg(delay / 1000).arg(m_reconnectAttempt));
    m_reconnectTimer.start(delay);
}

QByteArray MqttClient::encodeRemainingLength(int length)
{
    QByteArray result;
    do {
        int digit = length % 128;
        length /= 128;
        if (length > 0) digit |= 128;
        result.append(char(digit));
    } while (length > 0);
    return result;
}

QByteArray MqttClient::encodeString(const QString &value)
{
    const QByteArray bytes = value.toUtf8();
    QByteArray result;
    result.append(char((bytes.size() >> 8) & 0xff));
    result.append(char(bytes.size() & 0xff));
    result += bytes;
    return result;
}

QByteArray MqttClient::packet(quint8 header, const QByteArray &body) const
{
    QByteArray result;
    result.append(char(header));
    result += encodeRemainingLength(body.size());
    result += body;
    return result;
}
