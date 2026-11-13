#include "src/remote/mqttclient.h"
#include "src/remote/websocketclient.h"
#include "src/remote/httpclient.h"
#include "src/remote/remotetelemetryparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QCryptographicHash>
#include <QDebug>

static QByteArray mqttPacket(quint8 header, const QByteArray &body)
{
    QByteArray out;
    out.append(char(header));
    int n = body.size();
    do {
        int d = n % 128;
        n /= 128;
        if (n) d |= 0x80;
        out.append(char(d));
    } while (n);
    out += body;
    return out;
}

static bool takeMqttPacket(QByteArray &buffer, quint8 &header, QByteArray &body)
{
    if (buffer.size() < 2) return false;
    int multiplier = 1, remaining = 0, i = 1;
    quint8 d = 0;
    do {
        if (i >= buffer.size()) return false;
        d = quint8(buffer.at(i++));
        remaining += (d & 127) * multiplier;
        multiplier *= 128;
    } while (d & 128);
    if (buffer.size() < i + remaining) return false;
    header = quint8(buffer.at(0));
    body = buffer.mid(i, remaining);
    buffer.remove(0, i + remaining);
    return true;
}

static QByteArray mqttString(const QByteArray &s)
{
    QByteArray out;
    out.append(char(s.size() >> 8));
    out.append(char(s.size() & 0xff));
    return out + s;
}

static bool testMqtt()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return false;
    MqttClient client;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool connected = false, received = false, published = false;
    QByteArray input;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer]() {
            input += peer->readAll();
            quint8 header = 0;
            QByteArray body;
            while (takeMqttPacket(input, header, body)) {
                const int type = header >> 4;
                if (type == 1) {
                    peer->write(QByteArray::fromHex("20020000"));
                } else if (type == 8) {
                    QByteArray ack = body.left(2);
                    ack.append(char(0));
                    peer->write(mqttPacket(0x90, ack));
                    peer->write(mqttPacket(0x30, mqttString(QByteArrayLiteral("test/topic"))
                                           + QByteArrayLiteral("incoming")));
                } else if (type == 3) {
                    published = true;
                    if (connected && received) loop.quit();
                }
            }
        });
    });
    QObject::connect(&client, &MqttClient::connected, &loop, [&]() {
        connected = true;
        client.subscribe(QStringLiteral("test/topic"));
    });
    QObject::connect(&client, &MqttClient::messageReceived, &loop,
                     [&](const QString &topic, const QByteArray &payload) {
        received = topic == QStringLiteral("test/topic") && payload == QByteArrayLiteral("incoming");
        client.publish(QStringLiteral("reply/topic"), QByteArrayLiteral("outgoing"));
        if (published && connected && received) loop.quit();
    });
    client.connectToBroker(QStringLiteral("127.0.0.1"), server.serverPort(),
                           QStringLiteral("remote-client-test"), QString(), QString(), false);
    timeout.start(5000);
    loop.exec();
    client.disconnectFromBroker();
    return connected && received && published;
}

static bool testWebSocket()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) return false;
    WebSocketClient client;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool handshake = false, sent = false, received = false;
    QByteArray input;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer]() {
            input += peer->readAll();
            if (!handshake) {
                const int end = input.indexOf("\r\n\r\n");
                if (end < 0) return;
                const QByteArray request = input.left(end + 4);
                input.remove(0, end + 4);
                const QByteArray prefix("Sec-WebSocket-Key: ");
                const int keyStart = request.indexOf(prefix);
                if (keyStart < 0) return;
                const int keyEnd = request.indexOf("\r\n", keyStart);
                const QByteArray key = request.mid(keyStart + prefix.size(), keyEnd - keyStart - prefix.size());
                const QByteArray accept = QCryptographicHash::hash(
                    key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                    QCryptographicHash::Sha1).toBase64();
                peer->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
                            + accept + "\r\n\r\n");
                handshake = true;
            }
            if (handshake && input.size() >= 6) {
                const quint8 first = quint8(input.at(0));
                const quint8 second = quint8(input.at(1));
                const int len = second & 0x7f;
                if ((first & 0x0f) == 1 && (second & 0x80) && len == 5 && input.size() >= 11) {
                    const QByteArray mask = input.mid(2, 4);
                    QByteArray payload = input.mid(6, 5);
                    for (int i = 0; i < payload.size(); ++i)
                        payload[i] = char(quint8(payload.at(i)) ^ quint8(mask.at(i % 4)));
                    sent = payload == QByteArrayLiteral("hello");
                    input.remove(0, 11);
                    peer->write(QByteArray::fromHex("8105776f726c64")); // world
                }
            }
            if (received && sent) loop.quit();
        });
    });
    QObject::connect(&client, &WebSocketClient::connected, &loop, [&]() {
        client.sendText(QStringLiteral("hello"));
    });
    QObject::connect(&client, &WebSocketClient::textMessageReceived, &loop, [&](const QString &text) {
        received = text == QStringLiteral("world");
        if (received && sent) loop.quit();
    });
    client.connectToUrl(QUrl(QStringLiteral("ws://127.0.0.1:%1/test").arg(server.serverPort())));
    timeout.start(5000);
    loop.exec();
    client.disconnectFromServer();
    return handshake && sent && received;
}


static bool testHttp()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0))
        return false;

    HttpClient client;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool requestSeen = false;
    bool completed = false;
    QByteArray requestBuffer;
    const QByteArray responseBody = QByteArrayLiteral("{\"ok\":true,\"source\":\"local-test\"}");

    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        QTcpSocket *peer = server.nextPendingConnection();
        QObject::connect(peer, &QTcpSocket::readyRead, peer, [&, peer]() {
            requestBuffer += peer->readAll();
            if (requestBuffer.contains("\r\n\r\n")) {
                requestSeen = requestBuffer.startsWith("POST /api/test HTTP/1.1")
                    && requestBuffer.contains("Authorization: Bearer test-token")
                    && requestBuffer.contains("Content-Type: application/json")
                    && requestBuffer.endsWith(QByteArrayLiteral("{\"ping\":1}"));
                const QByteArray response = QByteArrayLiteral("HTTP/1.1 201 Created\r\n")
                    + QByteArrayLiteral("Content-Type: application/json\r\n")
                    + QByteArrayLiteral("Content-Length: ") + QByteArray::number(responseBody.size())
                    + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + responseBody;
                peer->write(response);
                peer->flush();
            }
        });
    });
    QObject::connect(&client, &HttpClient::requestFinished, &loop,
                     [&](int status, const QByteArray &body) {
        completed = status == 201 && body == responseBody;
        loop.quit();
    });
    QObject::connect(&client, &HttpClient::requestFailed, &loop,
                     [&](const QString &error) {
        qWarning() << "HTTP test request failed:" << error;
        loop.quit();
    });

    client.sendRequest(QStringLiteral("POST"),
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/test").arg(server.serverPort())),
        QByteArrayLiteral("{\"ping\":1}"), QStringLiteral("test-token"), 3000);
    timeout.start(5000);
    loop.exec();
    return requestSeen && completed;
}


static bool testRemoteTelemetryParser()
{
    const QByteArray json = QByteArrayLiteral(
        "{\\"deviceId\\":\\"MQTT-DEVICE-01\\","
        "\\"points\\":["
        "{\\"variableId\\":\\"temperature\\",\\"name\\":\\"温度\\",\\"value\\":26.5,\\"unit\\":\\"°C\\",\\"quality\\":\\"Good\\"},"
        "{\\"id\\":\\"current\\",\\"value\\":1.25,\\"unit\\":\\"A\\",\\"quality\\":\\"Bad\\"}"
        "]}");
    QString deviceId;
    QVector<BusinessDataPoint> points;
    QString error;
    if (!RemoteTelemetryParser::parse(QStringLiteral("MQTT"), QStringLiteral("plant/line1"),
                                      json, deviceId, points, &error))
        return false;
    if (deviceId != QStringLiteral("MQTT-DEVICE-01") || points.size() != 2
        || points.at(0).value != 26.5
        || points.at(0).sourceProtocol != QStringLiteral("MQTT:plant/line1")
        || points.at(1).quality != DataQuality::Bad)
        return false;

    const QByteArray single = QByteArrayLiteral(
        "{\\"deviceId\\":\\"HTTP-DEVICE-02\\",\\"variableId\\":\\"speed\\","
        "\\"value\\":1200,\\"unit\\":\\"rpm\\"}");
    if (!RemoteTelemetryParser::parse(QStringLiteral("HTTP"), QString(), single,
                                      deviceId, points, &error)
        || deviceId != QStringLiteral("HTTP-DEVICE-02") || points.size() != 1
        || points.first().variableId != QStringLiteral("speed"))
        return false;

    if (RemoteTelemetryParser::parse(QStringLiteral("MQTT"), QString(), QByteArrayLiteral("{"),
                                     deviceId, points, &error) || error.isEmpty())
        return false;

    QJsonArray tooMany;
    for (int i = 0; i < 501; ++i) {
        QJsonObject point;
        point.insert(QStringLiteral("variableId"), QStringLiteral("v%1").arg(i));
        point.insert(QStringLiteral("value"), i);
        tooMany.append(point);
    }
    QJsonObject root;
    root.insert(QStringLiteral("deviceId"), QStringLiteral("LIMIT-DEVICE"));
    root.insert(QStringLiteral("points"), tooMany);
    if (RemoteTelemetryParser::parse(QStringLiteral("WebSocket"), QString(),
            QJsonDocument(root).toJson(QJsonDocument::Compact), deviceId, points, &error))
        return false;
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (!testMqtt()) {
        qCritical() << "FAIL: MQTT local broker handshake/subscribe/publish.";
        return 1;
    }
    if (!testWebSocket()) {
        qCritical() << "FAIL: WebSocket local handshake/text frame.";
        return 2;
    }
    if (!testHttp()) {\n        qCritical() << "FAIL: HTTP POST, bearer token and JSON response.";\n        return 3;\n    }\n    if (!testRemoteTelemetryParser()) {\n        qCritical() << "FAIL: remote telemetry JSON mapping and validation.";\n        return 4;\n    }\n    qInfo() << "PASS: MQTT, WebSocket, HTTP and telemetry parser local tests.";
    return 0;
}
