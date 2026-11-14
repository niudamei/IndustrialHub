#include "remotepage.h"

#include "mqttclient.h"
#include "websocketclient.h"
#include "httpclient.h"
#include "remotetelemetryparser.h"
#include "remoteconfigstore.h"

#include <QComboBox>
#include <QUrl>

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDateTime>

RemotePage::RemotePage(QWidget *parent)
    : QWidget(parent), m_mqtt(new MqttClient(this)), m_webSocket(new WebSocketClient(this)),
      m_http(new HttpClient(this)), m_configStore(new RemoteConfigStore)
{
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto *mappingGroup = new QGroupBox(QStringLiteral("远程 Topic / URL → 设备变量映射（支持 * 通配符）"), this);
    auto *mappingLayout = new QVBoxLayout(mappingGroup);
    m_mappingTable = new QTableWidget(mappingGroup);
    m_mappingTable->setColumnCount(6);
    m_mappingTable->setHorizontalHeaderLabels({QStringLiteral("协议"), QStringLiteral("Topic / URL"),
        QStringLiteral("设备 ID"), QStringLiteral("默认变量 ID"), QStringLiteral("变量名称"),
        QStringLiteral("单位")});
    m_mappingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_mappingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mappingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mappingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mappingLayout->addWidget(m_mappingTable);
    auto *mappingForm = new QFormLayout;
    m_mappingProtocol = new QComboBox(mappingGroup);
    m_mappingProtocol->addItems({QStringLiteral("MQTT"), QStringLiteral("WebSocket"), QStringLiteral("HTTP")});
    m_mappingChannel = new QLineEdit(mappingGroup);
    m_mappingChannel->setPlaceholderText(QStringLiteral("例如 factory/line1/* 或 ws://host/device/*"));
    m_mappingDevice = new QLineEdit(mappingGroup);
    m_mappingVariable = new QLineEdit(mappingGroup);
    m_mappingName = new QLineEdit(mappingGroup);
    m_mappingUnit = new QLineEdit(mappingGroup);
    mappingForm->addRow(QStringLiteral("协议："), m_mappingProtocol);
    mappingForm->addRow(QStringLiteral("Topic / URL 模式："), m_mappingChannel);
    mappingForm->addRow(QStringLiteral("设备 ID："), m_mappingDevice);
    mappingForm->addRow(QStringLiteral("默认变量 ID（单变量消息可省略）："), m_mappingVariable);
    mappingForm->addRow(QStringLiteral("默认名称："), m_mappingName);
    mappingForm->addRow(QStringLiteral("默认单位："), m_mappingUnit);
    mappingLayout->addLayout(mappingForm);
    auto *mappingButtons = new QHBoxLayout;
    auto *saveMapping = new QPushButton(QStringLiteral("保存 / 更新映射"), mappingGroup);
    auto *deleteMapping = new QPushButton(QStringLiteral("删除选中映射"), mappingGroup);
    mappingButtons->addWidget(saveMapping);
    mappingButtons->addWidget(deleteMapping);
    mappingButtons->addStretch();
    mappingLayout->addLayout(mappingButtons);
    root->addWidget(mappingGroup);
    refreshMappingTable();
    connect(saveMapping, &QPushButton::clicked, this, &RemotePage::saveCurrentMapping);
    connect(deleteMapping, &QPushButton::clicked, this, &RemotePage::removeSelectedMapping);
    connect(m_mappingTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_mappingTable->currentRow();
        if (row < 0) return;
        m_mappingProtocol->setCurrentText(m_mappingTable->item(row, 0)->text());
        m_mappingChannel->setText(m_mappingTable->item(row, 1)->text());
        m_mappingDevice->setText(m_mappingTable->item(row, 2)->text());
        m_mappingVariable->setText(m_mappingTable->item(row, 3)->text());
        m_mappingName->setText(m_mappingTable->item(row, 4)->text());
        m_mappingUnit->setText(m_mappingTable->item(row, 5)->text());
    });

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
    m_mqttPayload = new QLineEdit(QStringLiteral("{\"deviceId\":\"MQTT-DEMO-001\",\"variableId\":\"temperature\",\"name\":\"温度\",\"value\":25.3,\"unit\":\"°C\"}"), topicGroup);
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
    m_wsPayload = new QLineEdit(QStringLiteral("{\"deviceId\":\"WS-DEMO-001\",\"variableId\":\"speed\",\"name\":\"转速\",\"value\":1200,\"unit\":\"rpm\"}"), wsGroup);
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
    m_httpMethod->setCurrentText(QStringLiteral("POST"));
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
    delete m_configStore;
    m_configStore = nullptr;
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
    QString deviceId;
    QVector<BusinessDataPoint> points;
    QString error;
    RemoteTelemetryMapping mapping;
    const bool hasMapping = m_configStore
        && m_configStore->resolveMapping(protocol, channel, mapping);
    if (!RemoteTelemetryParser::parse(protocol, channel, payload, deviceId, points, &error,
            hasMapping ? mapping.deviceId : QString(),
            hasMapping ? mapping.variableId : QString(),
            hasMapping ? mapping.name : QString(),
            hasMapping ? mapping.unit : QString())) {
        QPlainTextEdit *log = protocol == QStringLiteral("MQTT") ? m_mqttLog
            : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog;
        appendLog(log, QStringLiteral("远程数据解析失败：") + error);
        return;
    }
    emit businessDataReceived(deviceId, points);
    QPlainTextEdit *log = protocol == QStringLiteral("MQTT") ? m_mqttLog
        : protocol == QStringLiteral("WebSocket") ? m_wsLog : m_httpLog;
    appendLog(log, QStringLiteral("已解析 %1 个业务变量，设备：%2，已送入数据中心。")
        .arg(points.size()).arg(deviceId));
}

void RemotePage::refreshMappingTable()
{
    if (!m_mappingTable || !m_configStore) return;
    const QList<RemoteTelemetryMapping> mappings = m_configStore->mappings();
    m_mappingTable->setRowCount(mappings.size());
    for (int row = 0; row < mappings.size(); ++row) {
        const RemoteTelemetryMapping &mapping = mappings.at(row);
        const QStringList values{mapping.protocol, mapping.channelPattern, mapping.deviceId,
            mapping.variableId, mapping.name, mapping.unit};
        for (int column = 0; column < values.size(); ++column)
            m_mappingTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
}

void RemotePage::saveCurrentMapping()
{
    RemoteTelemetryMapping mapping;
    mapping.protocol = m_mappingProtocol->currentText();
    mapping.channelPattern = m_mappingChannel->text();
    mapping.deviceId = m_mappingDevice->text();
    mapping.variableId = m_mappingVariable->text();
    mapping.name = m_mappingName->text();
    mapping.unit = m_mappingUnit->text();
    QString error;
    if (!m_configStore->upsertMapping(mapping, &error)) {
        QMessageBox::warning(this, QStringLiteral("映射保存失败"), error);
        return;
    }
    refreshMappingTable();
    appendLog(m_mqttLog, QStringLiteral("远程映射已保存：%1 → %2")
        .arg(mapping.channelPattern, mapping.deviceId));
}

void RemotePage::removeSelectedMapping()
{
    const int row = m_mappingTable ? m_mappingTable->currentRow() : -1;
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条映射。"));
        return;
    }
    QString error;
    if (!m_configStore->removeMapping(m_mappingTable->item(row, 0)->text(),
                                      m_mappingTable->item(row, 1)->text(), &error)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    refreshMappingTable();
}
