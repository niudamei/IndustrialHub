#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QSslSocket>
#include <QUrl>

class WebSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    bool isConnected() const;
    void connectToUrl(const QUrl &url);
    void disconnectFromServer();
    bool sendText(const QString &text);
    bool sendBinary(const QByteArray &data);

signals:
    void connected();
    void disconnected();
    void textMessageReceived(const QString &message);
    void binaryMessageReceived(const QByteArray &message);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private slots:
    void onSocketConnected();
    void onReadyRead();
    void onSocketDisconnected();

private:
    void processFrames();
    bool sendFrame(quint8 opcode, const QByteArray &payload);
    void scheduleReconnect();
    QSslSocket *m_socket = nullptr;
    QTimer m_reconnectTimer;
    QUrl m_url;
    QByteArray m_buffer;
    QByteArray m_handshakeKey;
    int m_reconnectAttempt = 0;
    bool m_desiredConnection = false;
    bool m_upgraded = false;
};

#endif
