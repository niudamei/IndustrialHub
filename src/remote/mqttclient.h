#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QSslSocket>
#include <QStringList>

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(QObject *parent = nullptr);
    bool isConnected() const;
    void connectToBroker(const QString &host, quint16 port, const QString &clientId,
                         const QString &username, const QString &password,
                         bool useTls, int keepAliveSeconds = 30);
    void disconnectFromBroker();
    bool publish(const QString &topic, const QByteArray &payload, bool retain = false);
    bool subscribe(const QString &topicFilter);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &topic, const QByteArray &payload);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onSocketConnected();
    void onReadyRead();
    void onSocketDisconnected();
    void sendPing();

private:
    static QByteArray encodeRemainingLength(int length);
    static QByteArray encodeString(const QString &value);
    QByteArray packet(quint8 header, const QByteArray &body) const;
    void sendConnectPacket();
    void sendSubscribePacket(const QString &topicFilter);
    void scheduleReconnect();
    QSslSocket *m_socket = nullptr;
    QTimer m_pingTimer;
    QTimer m_reconnectTimer;
    QByteArray m_buffer;
    QString m_host, m_clientId, m_username, m_password;
    QStringList m_subscriptions;
    quint16 m_port = 1883;
    int m_keepAlive = 30;
    int m_reconnectAttempt = 0;
    quint16 m_packetId = 1;
    bool m_useTls = false;
    bool m_desiredConnection = false;
    bool m_mqttConnected = false;
    bool m_waitingPingResponse = false;
};

#endif
