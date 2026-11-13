#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "src/pages/DashboardPage.h"
#include "src/pages/devicepage.h"
#include "src/pages/devicemanager.h"
#include "src/communication/communicationmanager.h"
#include "src/core/datamanager.h"
#include "src/core/businessdatamanager.h"
#include "src/core/alarmmanager.h"
#include "src/protocol/protocolmanager.h"
#include "src/protocol/jsonprotocolparser.h"
#include "src/pages/AlarmPage.h"
#include "src/pages/ConfigPage.h"
#include "src/pages/HistoryPage.h"
#include "src/pages/MonitorPage.h"
#include "src/pages/TrendPage.h"
#include "src/pages/SettingsPage.h"
#include <QListWidgetItem>
#include <QMessageBox>
#include <QDebug>
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_deviceManager(new DeviceManager(this)),
      m_communicationManager(new CommunicationManager(this))
{
    ui->setupUi(this);

    m_dataManager = new DataManager(m_communicationManager, this);
    m_protocolManager = new ProtocolManager(this);
    m_businessDataManager = new BusinessDataManager(this);
    connect(m_businessDataManager, &BusinessDataManager::valuesUpdated,
            m_dataManager, &DataManager::appendBusinessDataPoints);
    m_alarmManager = new AlarmManager(this);
    if (m_alarmManager->isReady()) {
        // 首次运行时为三台虚拟设备建立演示温度高限规则；已有用户规则不覆盖。
        const QStringList demoDeviceIds = {
            QStringLiteral("DEMO-CAN-001"),
            QStringLiteral("DEMO-CANFD-002"),
            QStringLiteral("DEMO-TCP-003")
        };
        const QList<AlarmRule> existingRules = m_alarmManager->rules();
        for (const QString &deviceId : demoDeviceIds) {
            const QString ruleId = deviceId + QStringLiteral("-TEMP-HIGH");
            bool exists = false;
            for (const AlarmRule &existing : existingRules) {
                if (existing.ruleId.compare(ruleId, Qt::CaseInsensitive) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                AlarmRule rule;
                rule.ruleId = ruleId;
                rule.deviceId = deviceId;
                rule.variableId = QStringLiteral("temperature");
                rule.name = QStringLiteral("模拟温度高限");
                rule.highEnabled = true;
                rule.highLimit = 80.0;
                rule.severity = QStringLiteral("Warning");
                QString alarmRuleError;
                if (!m_alarmManager->addRule(rule, &alarmRuleError))
                    qWarning() << "创建模拟报警规则失败：" << alarmRuleError;
            }
        }
    }

    QString protocolError;
    if (!m_protocolManager->registerParser(
            QStringLiteral("json"), std::make_shared<JsonProtocolParser>(),
            &protocolError)) {
        QMessageBox::warning(this, QStringLiteral("协议解析器初始化失败"), protocolError);
    }

    // RX 原始帧仍先进入 DataManager 历史存储；只有显式绑定协议的设备才会进一步解析。
    connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
            m_protocolManager,
            [this](const QString &deviceId, const QByteArray &payload) {
        m_protocolManager->parseFrame(deviceId, payload);
    });

    connect(m_protocolManager, &ProtocolManager::businessDataDecoded,
            m_businessDataManager, &BusinessDataManager::updateDataPoints);
    connect(m_protocolManager, &ProtocolManager::businessDataDecoded,
            m_alarmManager, &AlarmManager::evaluateDataPoints);
    if (!m_dataManager->isReady()) {
        QMessageBox::warning(this, QStringLiteral("历史数据服务初始化失败"),
                             m_dataManager->lastError());
    }

    if (!m_deviceManager->isDatabaseReady()) {
        QMessageBox::warning(this, QStringLiteral("数据库初始化失败"),
                             m_deviceManager->lastError());
    } else {
        // 进程重启后不会自动恢复旧 TCP 会话，因此启动时清除上次运行遗留的在线状态。
        const QList<DeviceInfo> storedDevices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : storedDevices) {
            if (device.status != DeviceStatus::Offline)
                m_deviceManager->updateDeviceStatus(device.devId, DeviceStatus::Offline);
        }

        // 仅首次运行且数据库没有设备时写入演示数据，之后不再覆盖用户数据。
        if (m_deviceManager->getAllDevices().isEmpty()) {
            m_deviceManager->addDevice(DeviceInfo("DEV001", "导航控制器", "控制器", "TCP",
                                                   "192.168.2.125:5600", DeviceStatus::Offline));
            m_deviceManager->addDevice(DeviceInfo("DEV002", "CAN采集盒", "采集设备", "CAN",
                                                   "0x06069001", DeviceStatus::Offline));
        }
    }

    init_class();
    connect(ui->listWidget, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavigationClicked);
}

void MainWindow::onNavigationClicked(int index)
{
    if (index >= 0 && index < ui->stackedWidget->count())
        ui->stackedWidget->setCurrentIndex(index);
}

void MainWindow::init_class()
{
    dashboardPage = new DashboardPage(this);
    devicePage = new DevicePage(m_deviceManager, this);
    monitorPage = new MonitorPage(m_deviceManager, m_communicationManager, this);
    connect(monitorPage, &MonitorPage::businessDataGenerated,
            m_businessDataManager, &BusinessDataManager::updateDataPoints);
    connect(monitorPage, &MonitorPage::businessDataGenerated,
            m_alarmManager, &AlarmManager::evaluateDataPoints);
    trendPage = new TrendPage(m_communicationManager, m_businessDataManager, this);
    alarmPage = new AlarmPage(m_alarmManager, this);
    historyPage = new HistoryPage(m_dataManager, this);
    configPage = new ConfigPage(m_deviceManager, m_communicationManager,
                                m_protocolManager, this);
    settingsPage = new SettingsPage(this);

    // 页面添加顺序必须与左侧导航菜单顺序一致。
    ui->stackedWidget->addWidget(dashboardPage); // 0 运行总览
    ui->stackedWidget->addWidget(devicePage);    // 1 设备管理
    ui->stackedWidget->addWidget(monitorPage);   // 2 实时监控
    ui->stackedWidget->addWidget(trendPage);     // 3 实时趋势
    ui->stackedWidget->addWidget(alarmPage);     // 4 报警中心
    ui->stackedWidget->addWidget(historyPage);   // 5 历史数据
    ui->stackedWidget->addWidget(configPage);    // 6 通信配置
    ui->stackedWidget->addWidget(settingsPage);  // 7 系统设置

    ui->listWidget->setObjectName("navigationList");
    ui->listWidget->setFixedWidth(210);
    ui->listWidget->addItems({QStringLiteral("运行总览"), QStringLiteral("设备管理"),
        QStringLiteral("实时监控"), QStringLiteral("实时趋势"), QStringLiteral("报警中心"),
        QStringLiteral("历史数据"), QStringLiteral("通信配置"), QStringLiteral("系统设置")});
    for (int i = 0; i < ui->listWidget->count(); ++i)
        ui->listWidget->item(i)->setTextAlignment(Qt::AlignCenter);

    m_statusLabel = new CustomLabel(this);
    m_statusLabel->setText(QStringLiteral("系统就绪 | V0.9.0 | SQLite + TCP/UDP/RS485/CAN-FD"));
    m_statusLabel->setMinimumSize(220, 24);
    statusBar()->addPermanentWidget(m_statusLabel);
    ui->listWidget->setCurrentRow(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}
