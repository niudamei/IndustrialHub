#include "websocketclient.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSslError>
#include <QUuid>

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent), m_socket(new QSslSocket(this))
{
    m_reconnectTimer.setSingleShot(true);
    connect(m_socket, &QSslSocket::connected, this, &WebSocketClient::onSocketConnected);
    connect(m_socket, &QSslSocket::readyRead, this, &WebSocketClient::onReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, &WebSocketClient::onSocketDisconnected);
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_socket->errorString());
    });
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        for (const QSslError &error : errors)
            emit errorOccurred(QStringLiteral("WebSocket TLS 校验失败：%1").arg(error.errorString()));
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!m_desiredConnection) return;
        emit logMessage(QStringLiteral("WebSocket 正在自动重连…"));
        if (m_url.scheme().compare(QStringLiteral("wss"), Qt::CaseInsensitive) == 0)
            m_socket->connectToHostEncrypted(m_url.host(), quint16(m_url.port(443)));
        else
            m_socket->connectToHost(m_url.host(), quint16(m_url.port(80)));
    });
}

bool WebSocketClient::isConnected() const
{
    return m_upgraded && m_socket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketClient::connectToUrl(const QUrl &url)
{
    disconnectFromServer();
    m_socket->abort();
    if (!url.isValid() || (url.scheme() != QStringLiteral("ws") && url.scheme() != QStringLiteral("wss"))
        || url.host().isEmpty()) {
        emit errorOccurred(QStringLiteral("WebSocket URL 无效，必须使用 ws:// 或 wss://。"));
        return;
    }
    m_url = url;
    m_desiredConnection = true;
    m_reconnectAttempt = 0;
    m_buffer.clear();
    m_upgraded = false;
    emit logMessage(QStringLiteral("连接 WebSocket：%1").arg(url.toString()));
    if (url.scheme() == QStringLiteral("wss"))
        m_socket->connectToHostEncrypted(url.host(), quint16(url.port(443)));
    else
        m_socket->connectToHost(url.host(), quint16(url.port(80)));
}

void WebSocketClient::disconnectFromServer()
{
    m_desiredConnection = false;
    m_reconnectTimer.stop();
    if (m_upgraded)
        sendFrame(0x8, QByteArray());
    m_upgraded = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

bool WebSocketClient::sendText(const QString &text)
{
    const QByteArray payload = text.toUtf8();
    if (payload.size() > 1024 * 1024) {
        emit errorOccurred(QStringLiteral("WebSocket 消息超过 1 MiB 限制。"));
        return false;
    }
    return sendFrame(0x1, payload);
}

bool WebSocketClient::sendBinary(const QByteArray &data)
{
    if (data.size() > 1024 * 1024) {
        emit errorOccurred(QStringLiteral("WebSocket 消息超过 1 MiB 限制。"));
        return false;
    }
    return sendFrame(0x2, data);
}

void WebSocketClient::onSocketConnected()
{
    if (!m_desiredConnection) return;
    m_handshakeKey = QUuid::createUuid().toRfc4122().toBase64(QByteArray::Base64Encoding);
    QString path = m_url.path(QUrl::FullyEncoded);
    if (path.isEmpty()) path = QStringLiteral("/");
    if (!m_url.query().isEmpty()) path += QStringLiteral("?") + m_url.query(QUrl::FullyEncoded);
    const int defaultPort = m_url.scheme() == QStringLiteral("wss") ? 443 : 80;
    QString hostHeader = m_url.host();
    if (m_url.port(defaultPort) != defaultPort)
        hostHeader += QStringLiteral(":%1").arg(m_url.port());
    QByteArray request = "GET " + path.toUtf8() + " HTTP/1.1\r\n";
    request += "Host: " + hostHeader.toUtf8() + "\r\n";
    request += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + m_handshakeKey + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "Origin: " + (m_url.scheme() + QStringLiteral("://") + m_url.host()).toUtf8() + "\r\n\r\n";
    m_socket->write(request);
    m_buffer.clear();
}

void WebSocketClient::onReadyRead()
{
    m_buffer += m_socket->readAll();
    if (!m_upgraded) {
        const int end = m_buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            if (m_buffer.size() > 16384) {
                emit errorOccurred(QStringLiteral("WebSocket 握手响应头过大。"));
                m_socket->abort();
            }
            return;
        }
        const QByteArray headers = m_buffer.left(end + 4);
        m_buffer.remove(0, end + 4);
        const QByteArray expected = QCryptographicHash::hash(
            m_handshakeKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
            QCryptographicHash::Sha1).toBase64();
        const QByteArray lowerHeaders = headers.toLower();
        const int acceptStart = lowerHeaders.indexOf("sec-websocket-accept:");
        bool acceptMatches = false;
        if (acceptStart >= 0) {
            const int lineEnd = headers.indexOf("\r\n", acceptStart);
            const QByteArray acceptLine = headers.mid(acceptStart, lineEnd >= 0
                ? lineEnd - acceptStart : headers.size() - acceptStart);
            const int colon = acceptLine.indexOf(':');
            acceptMatches = colon >= 0 && acceptLine.mid(colon + 1).trimmed() == expected;
        }
        if (!headers.startsWith("HTTP/1.1 101") || !lowerHeaders.contains("upgrade: websocket")
            || !lowerHeaders.contains("connection: upgrade") || !acceptMatches) {
            emit errorOccurred(QStringLiteral("WebSocket 握手失败，服务端未返回有效的 101 Upgrade。"));
            m_socket->abort();
            return;
        }
        m_upgraded = true;
        m_reconnectAttempt = 0;
        emit logMessage(QStringLiteral("WebSocket 握手成功。"));
        emit connected();
    }
    processFrames();
}

void WebSocketClient::processFrames()
{
    while (m_buffer.size() >= 2) {
        const quint8 first = quint8(m_buffer.at(0));
        const quint8 second = quint8(m_buffer.at(1));
        const bool fin = (first & 0x80) != 0;
        const quint8 opcode = first & 0x0f;
        const bool masked = (second & 0x80) != 0;
        quint64 length = second & 0x7f;
        int offset = 2;
        if (length == 126) {
            if (m_buffer.size() < offset + 2) return;
            length = (quint8(m_buffer.at(offset)) << 8) | quint8(m_buffer.at(offset + 1));
            offset += 2;
        } else if (length == 127) {
            if (m_buffer.size() < offset + 8) return;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | quint8(m_buffer.at(offset + i));
            offset += 8;
        }
        if (length > 1024 * 1024) {
            emit errorOccurred(QStringLiteral("WebSocket 帧超过 1 MiB 限制。"));
            m_socket->abort(); return;
        }
        QByteArray mask;
        if (masked) {
            if (m_buffer.size() < offset + 4) return;
            mask = m_buffer.mid(offset, 4);
            offset += 4;
        }
        if (m_buffer.size() < offset + qint64(length)) return;
        QByteArray payload = m_buffer.mid(offset, int(length));
        m_buffer.remove(0, offset + int(length));
        if (masked) {
            for (int i = 0; i < payload.size(); ++i)
                payload[i] = char(static_cast<unsigned char>(payload.at(i))
                    ^ static_cast<unsigned char>(mask.at(i % 4)));
        }
        if (!fin && opcode < 8) {
            emit errorOccurred(QStringLiteral("当前 WebSocket 客户端不支持分片消息。"));
            m_socket->abort(); return;
        }
        if (opcode == 0x1) emit textMessageReceived(QString::fromUtf8(payload));
        else if (opcode == 0x2) emit binaryMessageReceived(payload);
        else if (opcode == 0x8) {
            sendFrame(0x8, QByteArray());
            m_socket->disconnectFromHost();
            return;
        } else if (opcode == 0x9) sendFrame(0xA, payload);
        else if (opcode == 0xA) emit logMessage(QStringLiteral("收到 WebSocket Pong。"));
    }
}

bool WebSocketClient::sendFrame(quint8 opcode, const QByteArray &payload)
{
    if (!m_upgraded || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;
    QByteArray frame;
    frame.append(char(0x80 | (opcode & 0x0f)));
    const quint64 size = quint64(payload.size());
    if (size < 126) frame.append(char(0x80 | size));
    else if (size <= 65535) {
        frame.append(char(0x80 | 126));
        frame.append(char((size >> 8) & 0xff));
        frame.append(char(size & 0xff));
    } else {
        frame.append(char(0x80 | 127));
        for (int i = 7; i >= 0; --i) frame.append(char((size >> (i * 8)) & 0xff));
    }
    QByteArray mask(4, Qt::Uninitialized);
    for (int i = 0; i < 4; ++i)
        mask[i] = char(QRandomGenerator::global()->generate() & 0xff);
    frame += mask;
    for (int i = 0; i < payload.size(); ++i)
        frame.append(char(static_cast<unsigned char>(payload.at(i))
            ^ static_cast<unsigned char>(mask.at(i % 4))));
    return m_socket->write(frame) == frame.size();
}

void WebSocketClient::onSocketDisconnected()
{
    const bool wasConnected = m_upgraded;
    m_upgraded = false;
    m_buffer.clear();
    if (wasConnected) emit disconnected();
    if (m_desiredConnection) scheduleReconnect();
}

void WebSocketClient::scheduleReconnect()
{
    if (!m_desiredConnection || m_reconnectTimer.isActive()) return;
    const int delay = qMin(1000 * (1 << qMin(m_reconnectAttempt, 5)), 30000);
    ++m_reconnectAttempt;
    emit logMessage(QStringLiteral("WebSocket 将在 %1 秒后重连（第 %2 次）。")
        .arg(delay / 1000).arg(m_reconnectAttempt));
    m_reconnectTimer.start(delay);
}
