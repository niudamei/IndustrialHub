#include "configpage.h"
#include "ui_configpage.h"

#include "src/pages/devicemanager.h"
#include "src/communication/communicationmanager.h"
#include "src/protocol/protocolmanager.h"
#include "src/protocol/businessdatapoint.h"
#include "src/core/communicationdiagnostics.h"
#include "src/core/devicehealthmonitor.h"
#include "src/core/businessdatamanager.h"
#include "src/core/alarmmanager.h"
#include "src/protocol/modbusmaster.h"
#include "src/protocol/modbuspoller.h"

#include <QDateTime>
#include <QComboBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QSplitter>
#include <QAbstractItemView>
#include <QTimer>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QFile>
#include <QFileDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTableWidgetItem>

ConfigPage::ConfigPage(DeviceManager *deviceManager,
                       CommunicationManager *communicationManager,
                       ProtocolManager *protocolManager,
                       QWidget *parent,
                       DeviceHealthMonitor *healthMonitor,
                       BusinessDataManager *businessDataManager,
                       AlarmManager *alarmManager)
    : QWidget(parent),
      ui(new Ui::ConfigPage),
      m_deviceManager(deviceManager),
      m_communicationManager(communicationManager),
      m_protocolManager(protocolManager),
      m_businessDataManager(businessDataManager),
      m_alarmManager(alarmManager)
{
    ui->setupUi(this);

    m_diagnostics = new CommunicationDiagnostics(m_communicationManager, this);
    auto *diagnosticsGroup = new QGroupBox(QStringLiteral("通信诊断中心"), this);
    auto *diagnosticsLayout = new QVBoxLayout(diagnosticsGroup);
    auto *splitter = new QSplitter(Qt::Vertical, diagnosticsGroup);
    m_statisticsTable = new QTableWidget(splitter);
    m_statisticsTable->setColumnCount(14);
    m_statisticsTable->setHorizontalHeaderLabels({
        QStringLiteral("设备"), QStringLiteral("TX 总次数"), QStringLiteral("RX 总次数"),
        QStringLiteral("TX 总字节"), QStringLiteral("RX 总字节"),
        QStringLiteral("TX 帧/秒"), QStringLiteral("RX 帧/秒"),
        QStringLiteral("TX 字节/秒"), QStringLiteral("RX 字节/秒"),
        QStringLiteral("最近 RX"), QStringLiteral("RX 静默秒数"),
        QStringLiteral("近 5 分钟错误"), QStringLiteral("错误率"), QStringLiteral("通信质量")});
    m_diagnosticsTable = new QTableWidget(splitter);
    m_diagnosticsTable->setColumnCount(5);
    m_diagnosticsTable->setHorizontalHeaderLabels({QStringLiteral("时间"),QStringLiteral("设备"),QStringLiteral("类别"),QStringLiteral("级别"),QStringLiteral("事件详情")});
    for (QTableWidget *table : {m_statisticsTable,m_diagnosticsTable}) { table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setAlternatingRowColors(true); table->verticalHeader()->setVisible(false); table->horizontalHeader()->setStretchLastSection(true); }
    splitter->addWidget(m_statisticsTable); splitter->addWidget(m_diagnosticsTable); splitter->setStretchFactor(0,1); splitter->setStretchFactor(1,2);
    auto *filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel(QStringLiteral("设备筛选："), diagnosticsGroup));
    m_diagnosticsDeviceFilter = new QLineEdit(diagnosticsGroup);
    m_diagnosticsDeviceFilter->setPlaceholderText(QStringLiteral("留空表示全部设备"));
    filterLayout->addWidget(m_diagnosticsDeviceFilter, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("开始："), diagnosticsGroup));
    m_diagnosticsStart = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7), diagnosticsGroup);
    m_diagnosticsStart->setCalendarPopup(true);
    filterLayout->addWidget(m_diagnosticsStart);
    filterLayout->addWidget(new QLabel(QStringLiteral("结束："), diagnosticsGroup));
    m_diagnosticsEnd = new QDateTimeEdit(QDateTime::currentDateTime(), diagnosticsGroup);
    m_diagnosticsEnd->setCalendarPopup(true);
    filterLayout->addWidget(m_diagnosticsEnd);
    auto *queryDiagnosticsButton = new QPushButton(QStringLiteral("查询"), diagnosticsGroup);
    filterLayout->addWidget(queryDiagnosticsButton);
    diagnosticsLayout->addLayout(filterLayout);
    auto *exportDiagnosticsButton = new QPushButton(QStringLiteral("导出筛选结果 CSV（最多 1000 条）"), diagnosticsGroup);
    diagnosticsLayout->addWidget(exportDiagnosticsButton);
    diagnosticsLayout->addWidget(splitter);
    ui->verticalLayout->insertWidget(2, diagnosticsGroup, 2);
    connect(queryDiagnosticsButton, &QPushButton::clicked, this, &ConfigPage::refreshDiagnostics);
    connect(exportDiagnosticsButton, &QPushButton::clicked, this, [this]() {
        if (!m_diagnostics) return;
        const auto events = m_diagnostics->recentEvents(1000, m_diagnosticsDeviceFilter->text(),
            m_diagnosticsStart->dateTime(), m_diagnosticsEnd->dateTime());
        if (events.isEmpty()) { QMessageBox::information(this, QStringLiteral("没有记录"), QStringLiteral("当前没有可导出的诊断事件。")); return; }
        const QString name = QStringLiteral("communication_diagnostics_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出通信诊断"), name, QStringLiteral("CSV 文件 (*.csv)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString()); return; }
        QByteArray output = QByteArray::fromHex("EFBBBF");
        output += QStringLiteral("时间,设备,类别,级别,详情\r\n").toUtf8();
        for (const auto &event : events) {
            QString message = event.message; message.replace(QStringLiteral("\""), QStringLiteral("\"\""));
            const QStringList columns = {event.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), event.deviceId, event.category, event.severity, QStringLiteral("\"%1\"").arg(message)};
            output += columns.join(QLatin1Char(',')).toUtf8() + "\r\n";
        }
        if (file.write(output) != output.size()) { QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString()); return; }
        QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("已导出最近 %1 条诊断记录。").arg(events.size()));
    });
    m_diagnosticsRefreshTimer = new QTimer(this);
    m_diagnosticsRefreshTimer->setSingleShot(true);
    m_diagnosticsRefreshTimer->setInterval(300);
    connect(m_diagnosticsRefreshTimer, &QTimer::timeout,
            this, &ConfigPage::refreshDiagnostics);
    m_qualityRefreshTimer = new QTimer(this);
    m_qualityRefreshTimer->setInterval(1000);
    connect(m_qualityRefreshTimer, &QTimer::timeout,
            this, &ConfigPage::refreshDiagnostics);
    m_qualityRefreshTimer->start();
    if (m_diagnostics) {
        connect(m_diagnostics, &CommunicationDiagnostics::diagnosticsChanged,
                this, [this]() { m_diagnosticsRefreshTimer->start(); });
    }
    if (healthMonitor) {
        connect(healthMonitor, &DeviceHealthMonitor::deviceTimedOut, m_diagnostics,
                [this](const QString &deviceId, qint64 silenceMs) {
            m_diagnostics->recordHealthTimeout(deviceId, silenceMs);
        });
    }
    refreshDiagnostics();

    // 协议绑定是设备级配置：默认不解析，用户明确选择后才启用解析器。
    auto *parserLayout = new QHBoxLayout;
    parserLayout->addWidget(new QLabel(QStringLiteral("业务协议："), this));
    m_parserCombo = new QComboBox(this);
    m_parserCombo->addItem(QStringLiteral("不解析原始数据"), QString());
    m_parserCombo->addItem(QStringLiteral("JSON 示例协议"), QStringLiteral("json"));
    parserLayout->addWidget(m_parserCombo, 1);
    auto *applyParserButton = new QPushButton(QStringLiteral("应用到当前设备"), this);
    parserLayout->addWidget(applyParserButton);
    ui->verticalLayout->insertLayout(1, parserLayout);
    connect(applyParserButton, &QPushButton::clicked,
            this, &ConfigPage::onApplyParserClicked);

    auto *modbusGroup = new QGroupBox(QStringLiteral("Modbus RTU / TCP 主站轮询"), this);
    modbusGroup->setMaximumHeight(360);
    auto *modbusLayout = new QVBoxLayout(modbusGroup);
    auto *modbusOptions = new QGridLayout;
    m_modbusUnitIdSpin = new QSpinBox(modbusGroup);
    m_modbusUnitIdSpin->setRange(1, 247);
    m_modbusUnitIdSpin->setValue(1);
    m_modbusIntervalSpin = new QSpinBox(modbusGroup);
    m_modbusIntervalSpin->setRange(100, 60000);
    m_modbusIntervalSpin->setValue(1000);
    m_modbusIntervalSpin->setSuffix(QStringLiteral(" ms"));
    m_modbusTimeoutSpin = new QSpinBox(modbusGroup);
    m_modbusTimeoutSpin->setRange(50, 60000);
    m_modbusTimeoutSpin->setValue(1000);
    m_modbusTimeoutSpin->setSuffix(QStringLiteral(" ms"));
    modbusOptions->addWidget(new QLabel(QStringLiteral("单元地址："), modbusGroup), 0, 0);
    modbusOptions->addWidget(m_modbusUnitIdSpin, 0, 1);
    modbusOptions->addWidget(new QLabel(QStringLiteral("轮询间隔："), modbusGroup), 0, 2);
    modbusOptions->addWidget(m_modbusIntervalSpin, 0, 3);
    modbusOptions->addWidget(new QLabel(QStringLiteral("响应超时："), modbusGroup), 0, 4);
    modbusOptions->addWidget(m_modbusTimeoutSpin, 0, 5);

    m_modbusVariableIdEdit = new QLineEdit(modbusGroup);
    m_modbusVariableIdEdit->setPlaceholderText(QStringLiteral("temperature"));
    m_modbusNameEdit = new QLineEdit(modbusGroup);
    m_modbusNameEdit->setPlaceholderText(QStringLiteral("温度"));
    m_modbusUnitEdit = new QLineEdit(modbusGroup);
    m_modbusUnitEdit->setPlaceholderText(QStringLiteral("°C"));
    m_modbusAddressSpin = new QSpinBox(modbusGroup);
    m_modbusAddressSpin->setRange(0, 65535);
    m_modbusFunctionCombo = new QComboBox(modbusGroup);
    m_modbusFunctionCombo->addItem(QStringLiteral("03 保持寄存器"), 0x03);
    m_modbusFunctionCombo->addItem(QStringLiteral("04 输入寄存器"), 0x04);
    m_modbusDataTypeCombo = new QComboBox(modbusGroup);
    m_modbusDataTypeCombo->addItem(QStringLiteral("UInt16"), static_cast<int>(ModbusRegisterMap::DataType::UInt16));
    m_modbusDataTypeCombo->addItem(QStringLiteral("Int16"), static_cast<int>(ModbusRegisterMap::DataType::Int16));
    m_modbusDataTypeCombo->addItem(QStringLiteral("UInt32"), static_cast<int>(ModbusRegisterMap::DataType::UInt32));
    m_modbusDataTypeCombo->addItem(QStringLiteral("Int32"), static_cast<int>(ModbusRegisterMap::DataType::Int32));
    m_modbusDataTypeCombo->addItem(QStringLiteral("Float32"), static_cast<int>(ModbusRegisterMap::DataType::Float32));
    m_modbusWordOrderCombo = new QComboBox(modbusGroup);
    m_modbusWordOrderCombo->addItem(QStringLiteral("ABCD"), static_cast<int>(ModbusRegisterMap::WordOrder::ABCD));
    m_modbusWordOrderCombo->addItem(QStringLiteral("CDAB"), static_cast<int>(ModbusRegisterMap::WordOrder::CDAB));
    m_modbusWordOrderCombo->addItem(QStringLiteral("BADC"), static_cast<int>(ModbusRegisterMap::WordOrder::BADC));
    m_modbusWordOrderCombo->addItem(QStringLiteral("DCBA"), static_cast<int>(ModbusRegisterMap::WordOrder::DCBA));
    m_modbusScaleSpin = new QDoubleSpinBox(modbusGroup);
    m_modbusScaleSpin->setRange(-1000000000.0, 1000000000.0);
    m_modbusScaleSpin->setDecimals(6);
    m_modbusScaleSpin->setValue(1.0);
    m_modbusOffsetSpin = new QDoubleSpinBox(modbusGroup);
    m_modbusOffsetSpin->setRange(-1000000000.0, 1000000000.0);
    m_modbusOffsetSpin->setDecimals(6);

    modbusOptions->addWidget(new QLabel(QStringLiteral("变量 ID："), modbusGroup), 1, 0);
    modbusOptions->addWidget(m_modbusVariableIdEdit, 1, 1);
    modbusOptions->addWidget(new QLabel(QStringLiteral("名称："), modbusGroup), 1, 2);
    modbusOptions->addWidget(m_modbusNameEdit, 1, 3);
    modbusOptions->addWidget(new QLabel(QStringLiteral("单位："), modbusGroup), 1, 4);
    modbusOptions->addWidget(m_modbusUnitEdit, 1, 5);
    modbusOptions->addWidget(new QLabel(QStringLiteral("寄存器地址："), modbusGroup), 2, 0);
    modbusOptions->addWidget(m_modbusAddressSpin, 2, 1);
    modbusOptions->addWidget(new QLabel(QStringLiteral("功能码："), modbusGroup), 2, 2);
    modbusOptions->addWidget(m_modbusFunctionCombo, 2, 3);
    modbusOptions->addWidget(new QLabel(QStringLiteral("数据类型："), modbusGroup), 2, 4);
    modbusOptions->addWidget(m_modbusDataTypeCombo, 2, 5);
    modbusOptions->addWidget(new QLabel(QStringLiteral("字序："), modbusGroup), 3, 0);
    modbusOptions->addWidget(m_modbusWordOrderCombo, 3, 1);
    modbusOptions->addWidget(new QLabel(QStringLiteral("缩放："), modbusGroup), 3, 2);
    modbusOptions->addWidget(m_modbusScaleSpin, 3, 3);
    modbusOptions->addWidget(new QLabel(QStringLiteral("偏移："), modbusGroup), 3, 4);
    modbusOptions->addWidget(m_modbusOffsetSpin, 3, 5);
    modbusLayout->addLayout(modbusOptions);

    auto *modbusButtons = new QHBoxLayout;
    m_modbusAddButton = new QPushButton(QStringLiteral("添加映射"), modbusGroup);
    m_modbusRemoveButton = new QPushButton(QStringLiteral("删除选中映射"), modbusGroup);
    m_modbusLoadSampleButton = new QPushButton(QStringLiteral("加载模拟示例映射"), modbusGroup);
    m_modbusStartButton = new QPushButton(QStringLiteral("启动轮询"), modbusGroup);
    m_modbusStopButton = new QPushButton(QStringLiteral("停止轮询"), modbusGroup);
    modbusButtons->addWidget(m_modbusAddButton);
    modbusButtons->addWidget(m_modbusRemoveButton);
    modbusButtons->addWidget(m_modbusLoadSampleButton);
    modbusButtons->addStretch(1);
    modbusButtons->addWidget(m_modbusStartButton);
    modbusButtons->addWidget(m_modbusStopButton);
    modbusLayout->addLayout(modbusButtons);

    m_modbusMappingTable = new QTableWidget(modbusGroup);
    m_modbusMappingTable->setColumnCount(9);
    m_modbusMappingTable->setHorizontalHeaderLabels({
        QStringLiteral("变量 ID"), QStringLiteral("名称"), QStringLiteral("地址"),
        QStringLiteral("功能码"), QStringLiteral("类型"), QStringLiteral("字序"),
        QStringLiteral("缩放"), QStringLiteral("偏移"), QStringLiteral("单位")
    });
    m_modbusMappingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_modbusMappingTable->horizontalHeader()->setStretchLastSection(true);
    m_modbusMappingTable->verticalHeader()->setVisible(false);
    m_modbusMappingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modbusMappingTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_modbusMappingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_modbusMappingTable->setMaximumHeight(150);
    modbusLayout->addWidget(m_modbusMappingTable);

    m_modbusStatusLabel = new QLabel(
        QStringLiteral("请选择已连接的 TCP / RS485 / MODBUS-SIM 设备；真实设备映射请以设备手册为准。"),
        modbusGroup);
    m_modbusStatusLabel->setWordWrap(true);
    modbusLayout->addWidget(m_modbusStatusLabel);
    ui->verticalLayout->insertWidget(2, modbusGroup, 1);

    connect(m_modbusAddButton, &QPushButton::clicked, this, &ConfigPage::addModbusMapping);
    connect(m_modbusRemoveButton, &QPushButton::clicked, this, &ConfigPage::removeModbusMapping);
    connect(m_modbusLoadSampleButton, &QPushButton::clicked, this, &ConfigPage::loadSimulatedModbusMappings);
    connect(m_modbusStartButton, &QPushButton::clicked, this, &ConfigPage::startModbusPolling);
    connect(m_modbusStopButton, &QPushButton::clicked, this, &ConfigPage::stopModbusPolling);
    connect(m_modbusMappingTable, &QTableWidget::itemSelectionChanged,
            this, &ConfigPage::updateModbusControls);
    connect(m_modbusUnitIdSpin, &QSpinBox::valueChanged, this, [this](int value) {
        const QString deviceId = selectedDeviceId();
        if (deviceId.isEmpty()) return;
        QSettings settings;
        settings.beginGroup(QStringLiteral("ModbusUnitIds"));
        settings.setValue(deviceId.toCaseFolded(), value);
        settings.endGroup();
    });

    ui->disconnectButton->setEnabled(false);
    ui->sendButton->setEnabled(false);

    connect(ui->connectButton, &QPushButton::clicked,
            this, &ConfigPage::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked,
            this, &ConfigPage::onDisconnectClicked);
    connect(ui->sendButton, &QPushButton::clicked,
            this, &ConfigPage::onSendClicked);
    connect(ui->deviceCombo, &QComboBox::currentIndexChanged,
            this, &ConfigPage::updateConnectionDisplay);
    connect(ui->deviceCombo, &QComboBox::currentIndexChanged,
            this, &ConfigPage::refreshModbusMappings);

    if (m_deviceManager) {
        connect(m_deviceManager, &DeviceManager::devicesChanged,
                this, &ConfigPage::refreshDeviceList);
        connect(m_deviceManager, &DeviceManager::deviceRemoved,
                this, [this](const QString &deviceId) {
            destroyModbusSession(deviceId);
            if (m_communicationManager) m_communicationManager->releaseDevice(deviceId);
            updateModbusControls();
        });
        connect(m_deviceManager, &DeviceManager::deviceUpdated,
                this, [this](const DeviceInfo &device) {
            const QString key = device.commType.toCaseFolded() + QChar(0x1f) + device.devAddr;
            const QString previous = m_communicationConfigByDevice.value(device.devId.toCaseFolded());
            if (!device.enabled || (!previous.isEmpty() && previous != key)) {
                destroyModbusSession(device.devId);
                if (m_communicationManager)
                    m_communicationManager->releaseDevice(device.devId);
            }
            m_communicationConfigByDevice.insert(device.devId.toCaseFolded(), key);
        });
    }

    if (m_communicationManager) {
        connect(m_communicationManager,
                &CommunicationManager::deviceConnectionStateChanged,
                this, &ConfigPage::onConnectionStateChanged);
        connect(m_communicationManager,
                &CommunicationManager::deviceDataReceived,
                this, &ConfigPage::onDataReceived);
        connect(m_communicationManager,
                &CommunicationManager::deviceErrorOccurred,
                this, &ConfigPage::onCommunicationError);
        connect(m_communicationManager,
                &CommunicationManager::deviceReconnecting,
                this, [this](const QString &deviceId, int attempt, int delayMs) {
            appendLog(QStringLiteral("[自动重连][%1] 第 %2 次尝试，%3 秒后执行。")
                          .arg(deviceId).arg(attempt)
                          .arg(QString::number(delayMs / 1000.0, 'f', 1)));
        });
    }

    if (m_protocolManager) {
        connect(m_protocolManager, &ProtocolManager::businessDataDecoded,
                this, &ConfigPage::onBusinessDataDecoded);
        connect(m_protocolManager, &ProtocolManager::parseFailed,
                this, &ConfigPage::onParseFailed);
    }

    refreshDeviceList();
    refreshDiagnostics();
}

ConfigPage::~ConfigPage()
{
    const auto pollerKeys = m_modbusPollersByDevice.keys();
    for (const QString &key : pollerKeys) {
        if (auto *poller = m_modbusPollersByDevice.value(key))
            poller->stop();
    }
    const auto masterKeys = m_modbusMastersByDevice.keys();
    for (const QString &key : masterKeys) {
        if (auto *master = m_modbusMastersByDevice.value(key))
            master->cancelAll(QStringLiteral("通信配置页面正在关闭。"));
    }
    delete ui;
}

QString ConfigPage::selectedDeviceId() const
{
    return ui->deviceCombo->currentData().toString();
}

void ConfigPage::refreshDeviceList()
{
    const QString previousId = selectedDeviceId();
    const QSignalBlocker blocker(ui->deviceCombo);
    ui->deviceCombo->clear();

    m_communicationConfigByDevice.clear();
    if (m_deviceManager) {
        const QList<DeviceInfo> devices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : devices) {
            m_communicationConfigByDevice.insert(device.devId.toCaseFolded(),
                device.commType.toCaseFolded() + QChar(0x1f) + device.devAddr);
            if (!device.enabled) continue;
            const QString label = QStringLiteral("%1 | %2 | %3")
                                      .arg(device.devId, device.devName, device.commType);
            ui->deviceCombo->addItem(label, device.devId);
        }
    }

    const int previousIndex = ui->deviceCombo->findData(previousId);
    if (previousIndex >= 0)
        ui->deviceCombo->setCurrentIndex(previousIndex);

    updateConnectionDisplay();
    refreshModbusMappings();
}

void ConfigPage::onConnectClicked()
{
    if (!m_deviceManager || !m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    DeviceInfo device;
    if (id.isEmpty() || !m_deviceManager->getDeviceById(id, device)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个有效设备。"));
        return;
    }

    if (!device.enabled) {
        QMessageBox::warning(this, QStringLiteral("设备已禁用"), QStringLiteral("请先在设备管理中启用该设备配置。"));
        return;
    }

    QString error;
    if (!m_communicationManager->connectDevice(device, &error)) {
        appendLog(QStringLiteral("[错误] %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("连接失败"), error);
        return;
    }

    appendLog(QStringLiteral("[连接请求] %1 -> %2").arg(device.devId, device.devAddr));
    updateConnectionDisplay();
}

void ConfigPage::onDisconnectClicked()
{
    if (!m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    if (id.isEmpty())
        return;

    m_communicationManager->disconnectDevice(id);
    appendLog(QStringLiteral("[断开请求] %1").arg(id));
    updateConnectionDisplay();
}

void ConfigPage::onSendClicked()
{
    if (!m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    DeviceInfo device;
    if (id.isEmpty() || !m_deviceManager
        || !m_deviceManager->getDeviceById(id, device)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个有效设备。"));
        return;
    }

    QByteArray data;
    const bool slcanMode =
        device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
        || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0;

    if (slcanMode) {
        data = ui->sendHexEdit->text().trimmed().toLatin1();
        if (data.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("数据格式错误"),
                                 QStringLiteral("请输入 SLCAN 帧命令，例如 T18AABBCC03112233。"));
            return;
        }
    } else {
        QString hex = ui->sendHexEdit->text();
        hex.remove(QRegularExpression(QStringLiteral("\\s+")));

        if (hex.isEmpty() || (hex.size() % 2) != 0
            || !QRegularExpression(QStringLiteral("^[0-9A-Fa-f]+$")).match(hex).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("数据格式错误"),
                                 QStringLiteral("请输入偶数个十六进制字符，可使用空格分隔。"));
            return;
        }
        data = QByteArray::fromHex(hex.toLatin1());
    }

    QString error;
    if (!m_communicationManager->sendData(id, data, &error)) {
        appendLog(QStringLiteral("[发送失败] %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("发送失败"), error);
        return;
    }

    const QString displayData = slcanMode
        ? QString::fromLatin1(data)
        : QString::fromLatin1(data.toHex(' ').toUpper());
    appendLog(QStringLiteral("[TX请求已入队][%1] %2").arg(id, displayData));
}

void ConfigPage::onConnectionStateChanged(const QString &deviceId, bool connected)
{
    appendLog(QStringLiteral("[%1] 设备 %2")
                  .arg(connected ? QStringLiteral("已连接") : QStringLiteral("已断开"),
                       deviceId));

    // 在线/离线状态统一由 MainWindow 中的 DeviceHealthMonitor 判定，
    // 避免 TCP 握手或本地串口打开被误判为远端设备在线。

    if (!connected) {
        const QString key = deviceId.toCaseFolded();
        if (auto *poller = m_modbusPollersByDevice.value(key)) {
            poller->stop();
            if (auto *master = m_modbusMastersByDevice.value(key))
                master->cancelAll(QStringLiteral("Modbus 设备连接已断开。"));
        }
    }
    if (deviceId.compare(selectedDeviceId(), Qt::CaseInsensitive) == 0) {
        updateConnectionDisplay();
        updateModbusControls();
    }
}

void ConfigPage::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    QString displayData = QString::fromLatin1(data.toHex(' ').toUpper());
    if (m_deviceManager) {
        DeviceInfo device;
        if (m_deviceManager->getDeviceById(deviceId, device)
            && (device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
                || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0)) {
            displayData = QString::fromLatin1(data);
        }
        // 设备在线状态由 DeviceHealthMonitor 统一更新。
    }
    appendLog(QStringLiteral("[RX][%1] %2").arg(deviceId, displayData));
}

void ConfigPage::onApplyParserClicked()
{
    if (!m_protocolManager) {
        appendLog(QStringLiteral("[协议] 协议管理器不可用。"));
        return;
    }

    const QString deviceId = selectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择设备。"));
        return;
    }

    const QString parserId = m_parserCombo->currentData().toString();
    if (parserId.isEmpty()) {
        m_protocolManager->unbindDevice(deviceId);
        appendLog(QStringLiteral("[协议] 设备 %1 已设置为不解析。").arg(deviceId));
        return;
    }

    QString error;
    if (!m_protocolManager->bindDevice(deviceId, parserId, &error)) {
        QMessageBox::warning(this, QStringLiteral("协议绑定失败"), error);
        appendLog(QStringLiteral("[协议错误] %1").arg(error));
        return;
    }

    appendLog(QStringLiteral("[协议] 设备 %1 已绑定 %2 解析器。")
                  .arg(deviceId, parserId.toUpper()));
}

void ConfigPage::onBusinessDataDecoded(
    const QString &deviceId,
    const QVector<BusinessDataPoint> &points)
{
    for (const BusinessDataPoint &point : points) {
        QString quality;
        switch (point.quality) {
        case DataQuality::Good: quality = QStringLiteral("Good"); break;
        case DataQuality::Uncertain: quality = QStringLiteral("Uncertain"); break;
        case DataQuality::Bad: quality = QStringLiteral("Bad"); break;
        }

        appendLog(QStringLiteral("[业务数据][%1] %2 (%3) = %4 %5 [%6]")
                      .arg(deviceId, point.name, point.variableId,
                           QString::number(point.value, 'g', 12),
                           point.unit, quality));
    }
}

void ConfigPage::onParseFailed(const QString &deviceId, const QString &message)
{
    appendLog(QStringLiteral("[协议解析失败][%1] %2").arg(deviceId, message));
}

void ConfigPage::onCommunicationError(const QString &deviceId, const QString &message)
{
    const QString prefix = deviceId.isEmpty()
        ? QStringLiteral("[通信管理错误]")
        : QStringLiteral("[通信错误][%1]").arg(deviceId);
    appendLog(prefix + QStringLiteral(" ") + message);
}

void ConfigPage::updateConnectionDisplay()
{
    const QString id = selectedDeviceId();
    bool connected = m_communicationManager && !id.isEmpty()
                     && m_communicationManager->isConnected(id);

    QString stateText = QStringLiteral("未连接");
    if (connected) {
        DeviceInfo selectedDevice;
        if (m_deviceManager && m_deviceManager->getDeviceById(id, selectedDevice)
            && selectedDevice.commType.compare(QStringLiteral("UDP"), Qt::CaseInsensitive) == 0) {
            stateText = QStringLiteral("UDP 本地已就绪");
        } else if (m_deviceManager
                   && m_deviceManager->getDeviceById(id, selectedDevice)
                   && (selectedDevice.commType.compare(QStringLiteral("RS485"), Qt::CaseInsensitive) == 0
                       || selectedDevice.commType.compare(QStringLiteral("SERIAL"), Qt::CaseInsensitive) == 0)) {
            stateText = QStringLiteral("串口已打开");
        } else if (m_deviceManager
                   && m_deviceManager->getDeviceById(id, selectedDevice)
                   && selectedDevice.commType.compare(QStringLiteral("MODBUS-SIM"), Qt::CaseInsensitive) == 0) {
            stateText = QStringLiteral("Modbus RTU 模拟设备已就绪");
        } else if (m_deviceManager
                   && m_deviceManager->getDeviceById(id, selectedDevice)
                   && (selectedDevice.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
                       || selectedDevice.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0)) {
            stateText = selectedDevice.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("CAN-FD 适配器已就绪") : QStringLiteral("CAN 适配器已就绪");
        } else {
            stateText = QStringLiteral("已连接");
        }
    }
    if (m_parserCombo && m_protocolManager) {
        const QSignalBlocker blocker(m_parserCombo);
        const QString parserId = m_protocolManager->parserForDevice(id);
        const int parserIndex = m_parserCombo->findData(parserId);
        m_parserCombo->setCurrentIndex(parserIndex >= 0 ? parserIndex : 0);
    }

    ui->connectionStateLabel->setText(stateText);
    ui->connectButton->setEnabled(!id.isEmpty() && !connected);
    ui->disconnectButton->setEnabled(connected);
    ui->sendButton->setEnabled(connected);

    DeviceInfo device;
    if (m_deviceManager && !id.isEmpty()
        && m_deviceManager->getDeviceById(id, device)) {
        ui->deviceAddressLabel->setText(device.devAddr);
        const bool slcanMode =
            device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
            || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0;
        ui->sendHexEdit->setPlaceholderText(slcanMode
            ? QStringLiteral("SLCAN 帧：T18AABBCC03112233 / D000000018...")
            : QStringLiteral("HEX 数据：01 03 00 00 00 02"));
    } else {
        ui->deviceAddressLabel->setText(QStringLiteral("--"));
        ui->sendHexEdit->setPlaceholderText(QStringLiteral("请选择设备"));
    }
    updateModbusControls();
}

void ConfigPage::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    ui->communicationLog->append(QStringLiteral("[%1] %2").arg(time, message));
}


void ConfigPage::refreshModbusMappings()
{
    const QString deviceId = selectedDeviceId();
    const QString key = deviceId.toCaseFolded();
    m_modbusMappingTable->setRowCount(0);
    if (deviceId.isEmpty()) {
        updateModbusControls();
        return;
    }

    QSettings unitSettings;
    unitSettings.beginGroup(QStringLiteral("ModbusUnitIds"));
    m_modbusUnitIdSpin->setValue(unitSettings.value(key, 1).toInt());
    unitSettings.endGroup();

    if (!m_modbusMappingsByDevice.contains(key)) {
        QSettings settings;
        settings.beginGroup(QStringLiteral("ModbusRegisterMaps"));
        const QByteArray json = settings.value(key).toByteArray();
        settings.endGroup();
        QVector<ModbusRegisterMap::Mapping> loaded;
        const QJsonDocument document = QJsonDocument::fromJson(json);
        if (document.isArray()) {
            for (const QJsonValue &value : document.array()) {
                if (!value.isObject()) continue;
                const QJsonObject object = value.toObject();
                ModbusRegisterMap::Mapping mapping;
                mapping.variableId = object.value(QStringLiteral("variableId")).toString();
                mapping.name = object.value(QStringLiteral("name")).toString();
                mapping.unit = object.value(QStringLiteral("unit")).toString();
                mapping.address = static_cast<quint16>(object.value(QStringLiteral("address")).toInt());
                mapping.function = static_cast<quint8>(object.value(QStringLiteral("function")).toInt(3));
                mapping.dataType = static_cast<ModbusRegisterMap::DataType>(
                    object.value(QStringLiteral("dataType")).toInt(0));
                mapping.wordOrder = static_cast<ModbusRegisterMap::WordOrder>(
                    object.value(QStringLiteral("wordOrder")).toInt(0));
                mapping.scale = object.value(QStringLiteral("scale")).toDouble(1.0);
                mapping.offset = object.value(QStringLiteral("offset")).toDouble(0.0);
                if (ModbusRegisterMap::validate(mapping))
                    loaded.append(mapping);
            }
        }
        m_modbusMappingsByDevice.insert(key, loaded);
    }

    DeviceInfo device;
    if (m_deviceManager && m_deviceManager->getDeviceById(deviceId, device)
        && device.commType.compare(QStringLiteral("MODBUS-SIM"), Qt::CaseInsensitive) == 0) {
        bool ok = true;
        const int unitId = device.devAddr.trimmed().isEmpty()
            ? 1 : device.devAddr.trimmed().toInt(&ok);
        if (ok && unitId >= 1 && unitId <= 247)
            m_modbusUnitIdSpin->setValue(unitId);
    }

    const auto mappings = m_modbusMappingsByDevice.value(key);
    m_modbusMappingTable->setRowCount(mappings.size());
    for (int row = 0; row < mappings.size(); ++row) {
        const auto &mapping = mappings.at(row);
        const QStringList values = {
            mapping.variableId, mapping.name, QString::number(mapping.address),
            QStringLiteral("0x%1").arg(mapping.function, 2, 16, QLatin1Char('0')).toUpper(),
            mapping.dataType == ModbusRegisterMap::DataType::UInt16 ? QStringLiteral("UInt16")
                : mapping.dataType == ModbusRegisterMap::DataType::Int16 ? QStringLiteral("Int16")
                : mapping.dataType == ModbusRegisterMap::DataType::UInt32 ? QStringLiteral("UInt32")
                : mapping.dataType == ModbusRegisterMap::DataType::Int32 ? QStringLiteral("Int32")
                : QStringLiteral("Float32"),
            mapping.wordOrder == ModbusRegisterMap::WordOrder::ABCD ? QStringLiteral("ABCD")
                : mapping.wordOrder == ModbusRegisterMap::WordOrder::CDAB ? QStringLiteral("CDAB")
                : mapping.wordOrder == ModbusRegisterMap::WordOrder::BADC ? QStringLiteral("BADC")
                : QStringLiteral("DCBA"),
            QString::number(mapping.scale, 'g', 12),
            QString::number(mapping.offset, 'g', 12), mapping.unit
        };
        for (int column = 0; column < values.size(); ++column)
            m_modbusMappingTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    updateModbusControls();
}

void ConfigPage::saveModbusMappings(const QString &deviceId)
{
    if (deviceId.trimmed().isEmpty()) return;
    const auto mappings = m_modbusMappingsByDevice.value(deviceId.toCaseFolded());
    QJsonArray array;
    for (const auto &mapping : mappings) {
        QJsonObject object;
        object.insert(QStringLiteral("variableId"), mapping.variableId);
        object.insert(QStringLiteral("name"), mapping.name);
        object.insert(QStringLiteral("unit"), mapping.unit);
        object.insert(QStringLiteral("address"), static_cast<int>(mapping.address));
        object.insert(QStringLiteral("function"), static_cast<int>(mapping.function));
        object.insert(QStringLiteral("dataType"), static_cast<int>(mapping.dataType));
        object.insert(QStringLiteral("wordOrder"), static_cast<int>(mapping.wordOrder));
        object.insert(QStringLiteral("scale"), mapping.scale);
        object.insert(QStringLiteral("offset"), mapping.offset);
        array.append(object);
    }
    QSettings settings;
    settings.beginGroup(QStringLiteral("ModbusRegisterMaps"));
    settings.setValue(deviceId.toCaseFolded(),
                      QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings.endGroup();
    settings.sync();
}

void ConfigPage::addModbusMapping()
{
    const QString deviceId = selectedDeviceId();
    if (deviceId.isEmpty()) return;
    ModbusRegisterMap::Mapping mapping;
    mapping.variableId = m_modbusVariableIdEdit->text().trimmed();
    mapping.name = m_modbusNameEdit->text().trimmed();
    mapping.unit = m_modbusUnitEdit->text().trimmed();
    mapping.address = static_cast<quint16>(m_modbusAddressSpin->value());
    mapping.function = static_cast<quint8>(m_modbusFunctionCombo->currentData().toInt());
    mapping.dataType = static_cast<ModbusRegisterMap::DataType>(
        m_modbusDataTypeCombo->currentData().toInt());
    mapping.wordOrder = static_cast<ModbusRegisterMap::WordOrder>(
        m_modbusWordOrderCombo->currentData().toInt());
    mapping.scale = m_modbusScaleSpin->value();
    mapping.offset = m_modbusOffsetSpin->value();

    QString error;
    if (!ModbusRegisterMap::validate(mapping, &error)) {
        QMessageBox::warning(this, QStringLiteral("映射配置无效"), error);
        return;
    }
    const QString key = deviceId.toCaseFolded();
    auto mappings = m_modbusMappingsByDevice.value(key);
    for (const auto &existing : mappings) {
        if (existing.variableId.compare(mapping.variableId, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, QStringLiteral("变量 ID 重复"),
                                 QStringLiteral("当前设备已经存在变量 ID：%1").arg(mapping.variableId));
            return;
        }
    }
    mappings.append(mapping);
    m_modbusMappingsByDevice.insert(key, mappings);
    saveModbusMappings(deviceId);
    refreshModbusMappings();
    appendLog(QStringLiteral("[Modbus映射] 已添加 %1，寄存器地址 %2。")
                  .arg(mapping.variableId).arg(mapping.address));
}

void ConfigPage::removeModbusMapping()
{
    const QString deviceId = selectedDeviceId();
    const int row = m_modbusMappingTable->currentRow();
    if (deviceId.isEmpty() || row < 0) return;
    auto mappings = m_modbusMappingsByDevice.value(deviceId.toCaseFolded());
    if (row >= mappings.size()) return;
    const QString variableId = mappings.at(row).variableId;
    mappings.removeAt(row);
    m_modbusMappingsByDevice.insert(deviceId.toCaseFolded(), mappings);
    saveModbusMappings(deviceId);
    refreshModbusMappings();
    appendLog(QStringLiteral("[Modbus映射] 已删除 %1。").arg(variableId));
}

void ConfigPage::loadSimulatedModbusMappings()
{
    const QString deviceId = selectedDeviceId();
    DeviceInfo device;
    if (deviceId.isEmpty() || !m_deviceManager
        || !m_deviceManager->getDeviceById(deviceId, device)
        || device.commType.compare(QStringLiteral("MODBUS-SIM"), Qt::CaseInsensitive) != 0) {
        QMessageBox::information(this, QStringLiteral("仅用于模拟设备"),
            QStringLiteral("示例寄存器地址仅适用于 MODBUS-SIM，不要直接用于真实设备。"));
        return;
    }

    if (!m_modbusMappingsByDevice.value(deviceId.toCaseFolded()).isEmpty()
        && QMessageBox::question(this, QStringLiteral("覆盖现有映射"),
            QStringLiteral("加载模拟示例会替换当前设备已有的寄存器映射，是否继续？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QVector<ModbusRegisterMap::Mapping> mappings;
    auto add = [&mappings](const QString &id, const QString &name, const QString &unit,
                           quint16 address, ModbusRegisterMap::DataType type, double scale) {
        ModbusRegisterMap::Mapping mapping;
        mapping.variableId = id;
        mapping.name = name;
        mapping.unit = unit;
        mapping.address = address;
        mapping.function = 0x03;
        mapping.dataType = type;
        mapping.scale = scale;
        mappings.append(mapping);
    };
    add(QStringLiteral("temperature"), QStringLiteral("温度"), QStringLiteral("°C"),
        0, ModbusRegisterMap::DataType::UInt16, 0.1);
    add(QStringLiteral("current"), QStringLiteral("电流"), QStringLiteral("A"),
        1, ModbusRegisterMap::DataType::UInt16, 0.01);
    add(QStringLiteral("rpm"), QStringLiteral("转速"), QStringLiteral("rpm"),
        2, ModbusRegisterMap::DataType::UInt16, 1.0);
    add(QStringLiteral("status"), QStringLiteral("状态"), QString(),
        3, ModbusRegisterMap::DataType::UInt16, 1.0);
    add(QStringLiteral("float_demo"), QStringLiteral("浮点示例"), QString(),
        10, ModbusRegisterMap::DataType::Float32, 1.0);
    m_modbusMappingsByDevice.insert(deviceId.toCaseFolded(), mappings);
    saveModbusMappings(deviceId);
    refreshModbusMappings();
    appendLog(QStringLiteral("[Modbus模拟] 已加载 5 条示例寄存器映射。"));
}

void ConfigPage::startModbusPolling()
{
    const QString deviceId = selectedDeviceId();
    DeviceInfo device;
    if (deviceId.isEmpty() || !m_deviceManager
        || !m_deviceManager->getDeviceById(deviceId, device)
        || !m_communicationManager || !m_communicationManager->isConnected(deviceId)) {
        QMessageBox::warning(this, QStringLiteral("设备未连接"),
                             QStringLiteral("请先连接所选 Modbus 设备，再启动轮询。"));
        return;
    }

    const QString type = device.commType.trimmed().toUpper();
    ModbusCodec::Transport transport;
    if (type == QStringLiteral("TCP")) {
        transport = ModbusCodec::Transport::Tcp;
    } else if (type == QStringLiteral("RS485") || type == QStringLiteral("SERIAL")
               || type == QStringLiteral("MODBUS-SIM")) {
        transport = ModbusCodec::Transport::Rtu;
    } else {
        QMessageBox::warning(this, QStringLiteral("不支持的设备类型"),
                             QStringLiteral("Modbus 轮询仅支持 TCP、RS485 或 MODBUS-SIM。"));
        return;
    }

    const auto mappings = m_modbusMappingsByDevice.value(deviceId.toCaseFolded());
    if (mappings.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("尚无寄存器映射"),
                                 QStringLiteral("请先添加映射，或为 MODBUS-SIM 加载示例映射。"));
        return;
    }

    destroyModbusSession(deviceId);
    int unitId = m_modbusUnitIdSpin->value();
    if (type == QStringLiteral("MODBUS-SIM")) {
        bool ok = true;
        const int configured = device.devAddr.trimmed().isEmpty()
            ? 1 : device.devAddr.trimmed().toInt(&ok);
        if (!ok || configured < 1 || configured > 247) {
            QMessageBox::warning(this, QStringLiteral("模拟设备地址无效"),
                                 QStringLiteral("MODBUS-SIM 的设备地址必须是 1–247。"));
            return;
        }
        unitId = configured;
    }
    auto *master = new ModbusMaster(transport, this);
    master->setTimeoutMs(m_modbusTimeoutSpin->value());
    if (!master->bindDevice(m_communicationManager, deviceId,
                            static_cast<quint8>(unitId))) {
        delete master;
        QMessageBox::warning(this, QStringLiteral("Modbus 绑定失败"),
            QStringLiteral("设备连接状态或通信类型与 Modbus RTU/TCP 不匹配。"));
        return;
    }

    auto *poller = new ModbusPoller(master, deviceId, this);
    QString error;
    if (!poller->setMappings(mappings, &error)) {
        delete poller;
        master->unbindDevice();
        delete master;
        QMessageBox::warning(this, QStringLiteral("映射配置无效"), error);
        return;
    }
    if (m_businessDataManager)
        poller->bindBusinessDataManager(m_businessDataManager);
    if (m_alarmManager) {
        connect(poller, &ModbusPoller::pointsReady,
                m_alarmManager, &AlarmManager::evaluateDataPoints);
    }
    connect(poller, &ModbusPoller::pollError, this,
            [this](const QString &id, const QString &variableId, const QString &message) {
        appendLog(QStringLiteral("[Modbus轮询错误][%1][%2] %3").arg(id, variableId, message));
    });
    connect(poller, &ModbusPoller::cycleFinished, this,
            [this, deviceId](int success, int failed) {
        m_modbusStatusLabel->setText(
            QStringLiteral("%1：最近一轮轮询成功 %2 项，失败 %3 项。")
                .arg(deviceId).arg(success).arg(failed));
    });

    m_modbusMastersByDevice.insert(deviceId.toCaseFolded(), master);
    m_modbusPollersByDevice.insert(deviceId.toCaseFolded(), poller);
    poller->start(m_modbusIntervalSpin->value());
    m_modbusStatusLabel->setText(QStringLiteral("%1：Modbus 轮询已启动。").arg(deviceId));
    appendLog(QStringLiteral("[Modbus轮询] %1 已启动，映射 %2 项，周期 %3 ms。")
                  .arg(deviceId).arg(mappings.size()).arg(m_modbusIntervalSpin->value()));
    updateModbusControls();
}

void ConfigPage::stopModbusPolling()
{
    const QString deviceId = selectedDeviceId();
    const QString key = deviceId.toCaseFolded();
    if (auto *poller = m_modbusPollersByDevice.value(key)) {
        poller->stop();
        if (auto *master = m_modbusMastersByDevice.value(key))
            master->cancelAll(QStringLiteral("用户停止 Modbus 轮询。"));
        m_modbusStatusLabel->setText(QStringLiteral("%1：Modbus 轮询已停止。").arg(deviceId));
        appendLog(QStringLiteral("[Modbus轮询] %1 已停止。").arg(deviceId));
    }
    updateModbusControls();
}

void ConfigPage::destroyModbusSession(const QString &deviceId)
{
    const QString key = deviceId.toCaseFolded();
    if (auto *poller = m_modbusPollersByDevice.take(key)) {
        poller->stop();
        delete poller;
    }
    if (auto *master = m_modbusMastersByDevice.take(key)) {
        master->unbindDevice();
        delete master;
    }
}

void ConfigPage::updateModbusControls()
{
    if (!m_modbusAddButton) return;
    const QString deviceId = selectedDeviceId();
    DeviceInfo device;
    const bool hasDevice = !deviceId.isEmpty() && m_deviceManager
        && m_deviceManager->getDeviceById(deviceId, device);
    const QString type = hasDevice ? device.commType.trimmed().toUpper() : QString();
    const bool supported = type == QStringLiteral("TCP")
        || type == QStringLiteral("RS485") || type == QStringLiteral("SERIAL")
        || type == QStringLiteral("MODBUS-SIM");
    const QString key = deviceId.toCaseFolded();
    const bool running = m_modbusPollersByDevice.contains(key)
        && m_modbusPollersByDevice.value(key)->isRunning();
    const bool connected = hasDevice && m_communicationManager
        && m_communicationManager->isConnected(deviceId);
    const bool hasMappings = !m_modbusMappingsByDevice.value(key).isEmpty();
    m_modbusAddButton->setEnabled(supported && !running);
    m_modbusRemoveButton->setEnabled(supported && !running
        && m_modbusMappingTable->currentRow() >= 0);
    m_modbusLoadSampleButton->setEnabled(type == QStringLiteral("MODBUS-SIM") && !running);
    m_modbusStartButton->setEnabled(supported && connected && hasMappings && !running);
    m_modbusStopButton->setEnabled(running);
    m_modbusUnitIdSpin->setEnabled(supported && !running && type != QStringLiteral("MODBUS-SIM"));
    m_modbusIntervalSpin->setEnabled(!running);
    m_modbusTimeoutSpin->setEnabled(!running);
}

void ConfigPage::refreshDiagnostics()
{
    if (!m_diagnostics || !m_diagnostics->isReady()) return;
    const auto stats = m_diagnostics->statistics();
    m_statisticsTable->setUpdatesEnabled(false);
    m_statisticsTable->setRowCount(0);
    int visibleRow = 0;
    for (const auto &s : stats) {
        if (!m_diagnosticsDeviceFilter->text().trimmed().isEmpty()
            && !s.deviceId.contains(m_diagnosticsDeviceFilter->text().trimmed(), Qt::CaseInsensitive))
            continue;
        m_statisticsTable->insertRow(visibleRow);
        const QString lastRx = s.lastRx.isValid()
            ? s.lastRx.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("尚未收到");
        const QString silence = s.lastRxSilenceMs >= 0
            ? QString::number(s.lastRxSilenceMs / 1000.0, 'f', 1)
            : QStringLiteral("-");
        const QStringList values = {
            s.deviceId, QString::number(s.txCount), QString::number(s.rxCount),
            QString::number(s.txBytes), QString::number(s.rxBytes),
            QString::number(s.txPerSecond, 'f', 2), QString::number(s.rxPerSecond, 'f', 2),
            QString::number(s.txBytesPerSecond, 'f', 1), QString::number(s.rxBytesPerSecond, 'f', 1),
            lastRx, silence, QString::number(s.recentErrorCount),
            QStringLiteral("%1%").arg(QString::number(s.recentErrorRatePercent, 'f', 1)),
            s.quality
        };
        for (int col = 0; col < values.size(); ++col)
            m_statisticsTable->setItem(visibleRow, col, new QTableWidgetItem(values.at(col)));
        if (auto *qualityItem = m_statisticsTable->item(visibleRow, 13))
            qualityItem->setToolTip(s.qualityReason);
        ++visibleRow;
    }
    m_statisticsTable->setUpdatesEnabled(true);
    const auto events=m_diagnostics->recentEvents(100, m_diagnosticsDeviceFilter->text(),
        m_diagnosticsStart->dateTime(), m_diagnosticsEnd->dateTime());
    m_diagnosticsTable->setUpdatesEnabled(false);
    m_diagnosticsTable->setRowCount(events.size());
    for(int row=0;row<events.size();++row) {
        const auto &e=events.at(row);
        const QStringList values={e.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            e.deviceId,e.category,e.severity,e.message};
        for(int col=0;col<values.size();++col)
            m_diagnosticsTable->setItem(row,col,new QTableWidgetItem(values.at(col)));
    }
    m_diagnosticsTable->setUpdatesEnabled(true);
}
