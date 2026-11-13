#include "remotepage.h"

#include "mqttclient.h"
#include "websocketclient.h"

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
    : QWidget(parent), m_mqtt(new MqttClient(this)), m_webSocket(new WebSocketClient(this))
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
}

void RemotePage::appendLog(QPlainTextEdit *view, const QString &message)
{
    if (!view) return;
    view->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
                          + QStringLiteral("  ") + message);
}
