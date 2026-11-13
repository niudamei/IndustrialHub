#ifndef REMOTEPAGE_H
#define REMOTEPAGE_H

#include <QWidget>
#include "src/protocol/businessdatapoint.h"

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class MqttClient;
class WebSocketClient;
class HttpClient;
class QComboBox;

class RemotePage : public QWidget
{
    Q_OBJECT
public:
    explicit RemotePage(QWidget *parent = nullptr);
    ~RemotePage() override;

signals:
    void businessDataReceived(const QString &deviceId, const QVector<BusinessDataPoint> &points);

private:
    void handleRemotePayload(const QString &protocol, const QString &channel, const QByteArray &payload);
    void appendLog(QPlainTextEdit *view, const QString &message);
    MqttClient *m_mqtt = nullptr;
    WebSocketClient *m_webSocket = nullptr;
    HttpClient *m_http = nullptr;
    QComboBox *m_httpMethod = nullptr;
    QLineEdit *m_httpUrl = nullptr;
    QLineEdit *m_httpToken = nullptr;
    QPlainTextEdit *m_httpBody = nullptr;
    QPlainTextEdit *m_httpLog = nullptr;
    QLineEdit *m_mqttHost = nullptr;
    QLineEdit *m_mqttPort = nullptr;
    QLineEdit *m_mqttClientId = nullptr;
    QLineEdit *m_mqttUsername = nullptr;
    QLineEdit *m_mqttPassword = nullptr;
    QLineEdit *m_mqttTopic = nullptr;
    QLineEdit *m_mqttPayload = nullptr;
    QCheckBox *m_mqttTls = nullptr;
    QPlainTextEdit *m_mqttLog = nullptr;
    QLineEdit *m_wsUrl = nullptr;
    QLineEdit *m_wsPayload = nullptr;
    QPlainTextEdit *m_wsLog = nullptr;
};

#endif
