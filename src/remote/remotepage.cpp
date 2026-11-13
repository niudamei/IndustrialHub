#include "remotepage.h"

#include "mqttclient.h"
#include "websocketclient.h"
#include "httpclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QComboBox>
#include <QUrl>
#include <QtMath>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDateTime>

RemotePage::RemotePage(QWidget *parent)
    : QWidget(parent), m_mqtt(new MqttClient(this)), m_webSocket(new WebSocketClient(this)), m_http(new HttpClient(this))
{
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto *mqttTab = new QWidget(tabs);
    auto *mqttLayout = new QVBoxLayout(mqttTab);
    auto *broker = new QGroupBox(QStringLiteral("MQTT Broker 配置"), mqttTab);
    auto *brokerForm = new QFormLayout(broker);
    m_mqttHost = new QLineEdit(QStringLiteral("127.0.0.1"), broker);
    m_mqttPort = new QLineEdit(QStringLiteral("1883"), broker);
    m_mqttClientId = new QLineEdit(QStringLiteral("IndustrialHub-Client"), broker);
    m_mqttUsername = new QLineEdit(broker);
    m_mqttPassword = new QLineEdit(broker);
    m_mqttPassword->setEchoMode(QLineEdit::Password);
    m_mqttTls = new QCheckBox(QStringLiteral("启用 TLS（证书校验）"), broker);
    brokerForm->addRow(QStringLiteral("主机："), m_mqttHost);
    brokerForm->addRow(QStringLiteral("端口："), m_mqttPort);
    brokerForm->addRow(QStringLiteral("Client ID："), m_mqttClientId);
    brokerForm->addRow(QStringLiteral("用户名："), m_mqttUsername);
    brokerForm->addRow(QStringLiteral("密码："), m_mqttPassword);
    brokerForm->addRow(QString(), m_mqttTls);
    mqttLayout->addWidget(broker);

    auto *mqttButtons = new QHBoxLayout;
    auto *mqttConnect = new QPushButton(QStringLiteral("连接 Broker"), mqttTab);
    auto *mqttDisconnect = new QPushButton(QStringLiteral("断开"), mqttTab);
    mqttButtons->addWidget(mqttConnect);
    mqttButtons->addWidget(mqttDisconnect);
    mqttButtons->addStretch();
    mqttLayout->addLayout(mqttButtons);

    auto *topicGroup = new QGroupBox(QStringLiteral("主题订阅与发布（QoS 0 发布 / QoS 0 订阅）"), mqttTab);
    auto *topicForm = new QFormLayout(topicGroup);
    m_mqttTopic = new QLineEdit(QStringLiteral("industrialhub/demo"), topicGroup);
    m_mqttPayload = new QLineEdit(QStringLiteral("{\"source\":\"IndustrialHub\",\"value\":1}"), topicGroup);
    topicForm->addRow(QStringLiteral("Topic / Filter："), m_mqttTopic);
    topicForm->addRow(QStringLiteral("发布内容："), m_mqttPayload);
    auto *topicButtons = new QHBoxLayout;
    auto *subscribe = new QPushButton(QStringLiteral("订阅主题"), topicGroup);
    auto *publish = new QPushButton(QStringLiteral("发布消息"), topicGroup);
    topicButtons->addWidget(subscribe);
    topicButtons->addWidget(publish);
    topicButtons->addStretch();
    topicForm->addRow(topicButtons);
    mqttLayout->addWidget(topicGroup);

    m_mqttLog = new QPlainTextEdit(mqttTab);
    m_mqttLog->setReadOnly(true);
    m_mqttLog->setPlaceholderText(QStringLiteral("MQTT 连接状态、收发消息和错误将在此显示。"));
    mqttLayout->addWidget(m_mqttLog, 1);
    tabs->addTab(mqttTab, QStringLiteral("MQTT"));

    auto *wsTab = new QWidget(tabs);
    auto *wsLayout = new QVBoxLayout(wsTab);
    auto *wsGroup = new QGroupBox(QStringLiteral("WebSocket 服务端"), wsTab);
    auto *wsForm = new QFormLayout(wsGroup);
    m_wsUrl = new QLineEdit(QStringLiteral("ws://127.0.0.1:8080/"), wsGroup);
    m_wsPayload = new QLineEdit(QStringLiteral("Hello IndustrialHub"), wsGroup);
    wsForm->addRow(QStringLiteral("WebSocket URL："), m_wsUrl);
    wsForm->addRow(QStringLiteral("发送文本："), m_wsPayload);
    auto *wsButtons = new QHBoxLayout;
    auto *wsConnect = new QPushButton(QStringLiteral("连接"), wsGroup);
    auto *wsDisconnect = new QPushButton(QStringLiteral("断开"), wsGroup);
    auto *wsSend = new QPushButton(QStringLiteral("发送文本"), wsGroup);
    wsButtons->addWidget(wsConnect);
    wsButtons->addWidget(wsDisconnect);
    wsButtons->addWidget(wsSend);
    wsButtons->addStretch();
    wsForm->addRow(wsButtons);
    wsLayout->addWidget(wsGroup);
    m_wsLog = new QPlainTextEdit(wsTab);
    m_wsLog->setReadOnly(true);
    m_wsLog->setPlaceholderText(QStringLiteral("WebSocket 握手、收发消息和错误将在此显示。"));
    wsLayout->addWidget(m_wsLog, 1);
    tabs->addTab(wsTab, QStringLiteral("WebSocket"));
    auto *httpTab = new QWidget(tabs);
    auto *httpLayout = new QVBoxLayout(httpTab);
    auto *httpGroup = new QGroupBox(QStringLiteral("HTTP / HTTPS API 客户端"), httpTab);
    auto *httpForm = new QFormLayout(httpGroup);
    m_httpMethod = new QComboBox(httpGroup);
    m_httpMethod->addItems({QStringLiteral("GET"), QStringLiteral("POST"),
                            QStringLiteral("PUT"), QStringLiteral("PATCH"),
                            QStringLiteral("DELETE")});
    m_httpUrl = new QLineEdit(QStringLiteral("http://127.0.0.1:8080/api/telemetry"), httpGroup);
    m_httpToken = new QLineEdit(httpGroup);
    m_httpToken->setEchoMode(QLineEdit::Password);
    m_httpBody = new QPlainTextEdit(httpGroup);
    m_httpBody->setMaximumHeight(110);
    m_httpBody->setPlainText(QStringLiteral("{\n  \"deviceId\": \"HTTP-DEMO-001\",\n  \"variableId\": \"temperature\",\n  \"name\": \"温度\",\n  \"value\": 25.3,\n  \"unit\": \"°C\"\n}"));
    auto *httpSend = new QPushButton(QStringLiteral("发送 HTTP 请求"), httpGroup);
    httpForm->addRow(QStringLiteral("方法："), m_httpMethod);
    httpForm->addRow(QStringLiteral("URL："), m_httpUrl);
    httpForm->addRow(QStringLiteral("Bearer Token："), m_httpToken);
    httpForm->addRow(QStringLiteral("JSON 请求体："), m_httpBody);
    httpForm->addRow(httpSend);
    httpLayout->addWidget(httpGroup);
    m_httpLog = new QPlainTextEdit(httpTab);
    m_httpLog->setReadOnly(true);
    m_httpLog->setPlaceholderText(QStringLiteral("HTTP 状态码、响应内容和错误将在此显示。"));
    httpLayout->addWidget(m_httpLog, 1);
    tabs->addTab(httpTab, QStringLiteral("HTTP / HTTPS"));

    connect(httpSend, &QPushButton::clicked, this, [this]() {
        const QString method = m_httpMethod->currentText();
        const QByteArray body = m_httpBody->toPlainText().toUtf8();
        if (!m_http->sendRequest(method, QUrl(m_httpUrl->text().trimmed()),
                                 body, m_httpToken->text())) {
            appendLog(m_httpLog, QStringLiteral("请求未启动，请检查 URL、方法或请求状态。"));
        }
    });
    connect(m_http, &HttpClient::requestFinished, this, [this](int status, const QByteArray &body) {
        appendLog(m_httpLog, QStringLiteral("HTTP %1\n%2")
            .arg(status).arg(QString::fromUtf8(body)));
        if (status >= 200 && status < 300)
            handleRemotePayload(QStringLiteral("HTTP"), m_httpUrl->text(), body);
    });
    connect(m_http, &HttpClient::requestFailed, this, [this](const QString &message) {
        appendLog(m_httpLog, QStringLiteral("错误：") + message);
    });


    connect(mqttConnect, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const uint port = m_mqttPort->text().toUInt(&ok);
        if (!ok || port == 0 || port > 65535) {
            appendLog(m_mqttLog, QStringLiteral("端口必须为 1–65535。"));
            return;
        }
        m_mqtt->connectToBroker(m_mqttHost->text(), quint16(port), m_mqttClientId->text(),
                                m_mqttUsername->text(), m_mqttPassword->text(),
                                m_mqttTls->isChecked());
    });
    connect(mqttDisconnect, &QPushButton::clicked, m_mqtt, &MqttClient::disconnectFromBroker);
    connect(subscribe, &QPushButton::clicked, this, [this]() {
        if (!m_mqtt->subscribe(m_mqttTopic->text()))
            appendLog(m_mqttLog, QStringLiteral("订阅失败：请先连接 Broker，并检查 Topic Filter。"));
    });
    connect(publish, &QPushButton::clicked, this, [this]() {
        if (!m_mqtt->publish(m_mqttTopic->text(), m_mqttPayload->text().toUtf8()))
            appendLog(m_mqttLog, QStringLiteral("发布失败：请先连接 Broker，且 Topic/消息长度有效。"));
    });
    connect(m_mqtt, &MqttClient::logMessage, this, [this](const QString &s) { appendLog(m_mqttLog, s); });
    connect(m_mqtt, &MqttClient::errorOccurred, this, [this](const QString &s) { appendLog(m_mqttLog, QStringLiteral("错误：") + s); });
    connect(m_mqtt, &MqttClient::messageReceived, this, [this](const QString &topic, const QByteArray &payload) {
        appendLog(m_mqttLog, QStringLiteral("RX [%1] %2").arg(topic, QString::fromUtf8(payload)));
        handleRemotePayload(QStringLiteral("MQTT"), topic, payload);
    });
    connect(m_mqtt, &MqttClient::connected, this, [this]() { appendLog(m_mqttLog, QStringLiteral("状态：已连接")); });
    connect(m_mqtt, &MqttClient::disconnected, this, [this]() { appendLog(m_mqttLog, QStringLiteral("状态：已断开")); });

    connect(wsConnect, &QPushButton::clicked, this, [this]() {
        m_webSocket->connectToUrl(QUrl(m_wsUrl->text().trimmed()));
    });
    connect(wsDisconnect, &QPushButton::clicked, m_webSocket, &WebSocketClient::disconnectFromServer);
    connect(wsSend, &QPushButton::clicked, this, [this]() {
        if (!m_webSocket->sendText(m_wsPayload->text()))
            appendLog(m_wsLog, QStringLiteral("发送失败：WebSocket 尚未连接或消息超长。"));
    });
    connect(m_webSocket, &WebSocketClient::logMessage, this, [this](const QString &s) { appendLog(m_wsLog, s); });
    connect(m_webSocket, &WebSocketClient::errorOccurred, this, [this](const QString &s) { appendLog(m_wsLog, QStringLiteral("错误：") + s); });
    connect(m_webSocket, &WebSocketClient::textMessageReceived, this, [this](const QString &s) {
        appendLog(m_wsLog, QStringLiteral("RX TEXT: ") + s);
        handleRemotePayload(QStringLiteral("WebSocket"), m_wsUrl->text(), s.toUtf8());
    });
    connect(m_webSocket, &WebSocketClient::binaryMessageReceived, this, [this](const QByteArray &b) {
        appendLog(m_wsLog, QStringLiteral("RX BINARY: %1 bytes | %2").arg(b.size()).arg(QString::fromLatin1(b.toHex(' '))));
    });
    connect(m_webSocket, &WebSocketClient::connected, this, [this]() { appendLog(m_wsLog, QStringLiteral("状态：已连接")); });
    connect(m_webSocket, &WebSocketClient::disconnected, this, [this]() { appendLog(m_wsLog, QStringLiteral("状态：已断开")); });
}

RemotePage::~RemotePage()
{
    if (m_mqtt) m_mqtt->disconnectFromBroker();
    if (m_webSocket) m_webSocket->disconnectFromServer();
    if (m_http) m_http->cancel();
}

void RemotePage::appendLog(QPlainTextEdit *view, const QString &message)
{
    if (!view) return;
    view->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                          + QStringLiteral("  ") + message);
}

void RemotePage::handleRemotePayload(const QString &protocol, const QString &channel,
                                     const QByteArray &payload)
{
    if (payload.isEmpty() || payload.size() > 1024 * 1024)
        return;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        appendLog(protocol == QStringLiteral("MQTT") ? m_mqttLog
                   : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog,
                   QStringLiteral("收到非 JSON 数据，不进入业务变量链路。"));
        return;
    }

    const QJsonObject root = document.object();
    const QString deviceId = root.value(QStringLiteral("deviceId")).toString().trimmed();
    if (deviceId.isEmpty()) {
        appendLog(protocol == QStringLiteral("MQTT") ? m_mqttLog
                   : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog,
                   QStringLiteral("JSON 缺少 deviceId，已拒绝业务数据映射。"));
        return;
    }

    QJsonArray pointsArray = root.value(QStringLiteral("points")).toArray();
    if (pointsArray.isEmpty())
        pointsArray = root.value(QStringLiteral("variables")).toArray();
    if (pointsArray.isEmpty())
        pointsArray.append(root);

    if (pointsArray.size() > 500) {
        appendLog(protocol == QStringLiteral("MQTT") ? m_mqttLog
                   : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog,
                   QStringLiteral("单条远程消息变量数超过 500，已拒绝。"));
        return;
    }

    QVector<BusinessDataPoint> points;
    points.reserve(pointsArray.size());
    for (const QJsonValue &value : pointsArray) {
        if (!value.isObject())
            continue;
        const QJsonObject object = value.toObject();
        const QString variableId = object.value(QStringLiteral("variableId")).toString(
            object.value(QStringLiteral("id")).toString()).trimmed();
        const QJsonValue numericValue = object.value(QStringLiteral("value"));
        if (variableId.isEmpty() || !numericValue.isDouble())
            continue;
        const double number = numericValue.toDouble();
        if (!qIsFinite(number))
            continue;

        BusinessDataPoint point;
        point.deviceId = deviceId;
        point.variableId = variableId;
        point.name = object.value(QStringLiteral("name")).toString(variableId);
        point.value = number;
        point.unit = object.value(QStringLiteral("unit")).toString();
        point.timestamp = QDateTime::fromString(
            object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
        if (!point.timestamp.isValid())
            point.timestamp = QDateTime::fromString(
                object.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        if (!point.timestamp.isValid())
            point.timestamp = QDateTime::currentDateTime();
        const QString quality = object.value(QStringLiteral("quality")).toString();
        point.quality = quality.compare(QStringLiteral("Bad"), Qt::CaseInsensitive) == 0
            ? DataQuality::Bad
            : quality.compare(QStringLiteral("Uncertain"), Qt::CaseInsensitive) == 0
                ? DataQuality::Uncertain : DataQuality::Good;
        point.sourceProtocol = protocol + (channel.isEmpty() ? QString() : QStringLiteral(":") + channel);
        points.append(point);
    }

    if (points.isEmpty()) {
        appendLog(protocol == QStringLiteral("MQTT") ? m_mqttLog
                   : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog,
                   QStringLiteral("JSON 中没有有效业务变量（需要 variableId/id 和数值 value）。"));
        return;
    }
    emit businessDataReceived(deviceId, points);
    appendLog(protocol == QStringLiteral("MQTT") ? m_mqttLog
               : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog,
               QStringLiteral("已解析 %1 个业务变量，设备：%2，已送入数据中心。")
                   .arg(points.size()).arg(deviceId));
}

