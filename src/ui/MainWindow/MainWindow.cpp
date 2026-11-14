#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "src/pages/DashboardPage.h"
#include "src/pages/devicepage.h"
#include "src/pages/devicemanager.h"
#include "src/communication/communicationmanager.h"
#include "src/core/devicehealthmonitor.h"
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
#include "src/security/authmanager.h"
#include "src/security/logindialog.h"
#include "src/security/usermanagementpage.h"
#include "src/remote/remotepage.h"
#include "src/plugins/pluginmanager.h"
#include "src/plugins/pluginpage.h"\n#ifdef INDUSTRIALHUB_HAS_SERIALBUS\n#include "src/pages/SimulatorPage.h"\n#endif
#include <QListWidgetItem>
#include <QMessageBox>
#include <QDebug>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <QDialog>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>
#include <QStyle>
#include <QApplication>
#include <QSettings>
#include <memory>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_deviceManager(new DeviceManager(this)),
      m_communicationManager(new CommunicationManager(this))
{
    ui->setupUi(this);

    if (!m_deviceManager->isDatabaseReady()) {
        QMessageBox::critical(this, QStringLiteral("数据库初始化失败"),
                               m_deviceManager->lastError());
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }
    m_authManager = new AuthManager(m_deviceManager->databasePath(), this);
    if (!m_authManager->isReady()) {
        QMessageBox::critical(this, QStringLiteral("权限服务初始化失败"), m_authManager->lastError());
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }
    LoginDialog loginDialog(m_authManager, this);
    if (loginDialog.exec() != QDialog::Accepted) {
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

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

    m_deviceHealthMonitor = new DeviceHealthMonitor(this);
    for (const DeviceInfo &device : m_deviceManager->getAllDevices())
        m_deviceHealthMonitor->registerDevice(device.devId);

    connect(m_deviceManager, &DeviceManager::deviceAdded,
            m_deviceHealthMonitor, [this](const DeviceInfo &device) {
        m_deviceHealthMonitor->registerDevice(device.devId);
    });
    connect(m_deviceManager, &DeviceManager::deviceRemoved,
            m_deviceHealthMonitor, &DeviceHealthMonitor::removeDevice);
    connect(m_communicationManager, &CommunicationManager::deviceConnectionStateChanged,
            m_deviceHealthMonitor, &DeviceHealthMonitor::setConnected);
    connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
            m_deviceHealthMonitor, [this](const QString &deviceId, const QByteArray &) {
        m_deviceHealthMonitor->markActivity(deviceId);
    });
    connect(m_deviceHealthMonitor, &DeviceHealthMonitor::deviceOnlineChanged,
            this, [this](const QString &deviceId, bool online) {
        QString error;
        const DeviceStatus status = online ? DeviceStatus::Online : DeviceStatus::Offline;
        if (!m_deviceManager->updateDeviceStatus(deviceId, status, &error))
            qWarning() << "更新设备在线状态失败：" << deviceId << error;
    });
    connect(m_deviceHealthMonitor, &DeviceHealthMonitor::deviceTimedOut,
            this, [](const QString &deviceId, qint64 silenceMs) {
        qInfo() << "设备通信超时：" << deviceId << "静默时间(ms):" << silenceMs;
    });

    init_class();
    connect(settingsPage, &SettingsPage::themeChanged,
            this, &MainWindow::setDarkTheme);
    connect(settingsPage, &SettingsPage::minimizeToTrayChanged,
            this, [this](bool enabled) { m_minimizeToTray = enabled; });
    setDarkTheme(QSettings().value(QStringLiteral("ui/darkTheme"), false).toBool());
    m_minimizeToTray = QSettings().value(QStringLiteral("ui/minimizeToTray"), true).toBool();
    initSystemTray();

    auto *userLabel = new QLabel(this);
    auto *logoutButton = new QPushButton(QStringLiteral("退出登录"), this);
    ui->mainToolbar->addWidget(userLabel);
    ui->mainToolbar->addSeparator();
    ui->mainToolbar->addWidget(logoutButton);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::showLoginDialog);
    connect(m_authManager, &AuthManager::authenticated, this, [userLabel](const AuthUser &user) {
        userLabel->setText(QStringLiteral("当前用户：%1（%2）").arg(user.displayName, user.role));
    });
    userLabel->setText(QStringLiteral("当前用户：%1（%2）")
        .arg(m_authManager->currentUser().displayName, m_authManager->currentRole()));

    applyPermissions();
    connect(ui->listWidget, &QListWidget::currentRowChanged,
            this, &MainWindow::onNavigationClicked);
}

void MainWindow::onNavigationClicked(int index)
{
    static const QStringList required = {
        QStringLiteral("dashboard.view"), QStringLiteral("device.view"),
        QStringLiteral("monitor.view"), QStringLiteral("trend.view"),
        QStringLiteral("alarm.view"), QStringLiteral("history.view"),
        QStringLiteral("communication.view"), QStringLiteral("system.manage"),
        QStringLiteral("remote.manage"), QStringLiteral("users.manage"),
        QStringLiteral("plugins.manage")
#ifdef INDUSTRIALHUB_HAS_SERIALBUS
        , QStringLiteral("system.manage")
#endif
    };
    if (index < 0 || index >= ui->stackedWidget->count()
        || index >= required.size() || !m_authManager
        || !m_authManager->hasPermission(required.at(index))) {
        if (m_authManager)
            m_authManager->audit(QStringLiteral("permission.denied"),
                                 QString::number(index), QStringLiteral("navigation"));
        return;
    }
    ui->stackedWidget->setCurrentIndex(index);
    m_authManager->audit(QStringLiteral("navigation"), required.at(index));
}

void MainWindow::showLoginDialog()
{
    if (!m_authManager)
        return;
    m_authManager->logout();
    LoginDialog dialog(m_authManager, this);
    if (dialog.exec() != QDialog::Accepted) {
        close();
        return;
    }
    applyPermissions();
}

void MainWindow::applyPermissions()
{
    if (!m_authManager || !ui)
        return;
    static const QStringList required = {
        QStringLiteral("dashboard.view"), QStringLiteral("device.view"),
        QStringLiteral("monitor.view"), QStringLiteral("trend.view"),
        QStringLiteral("alarm.view"), QStringLiteral("history.view"),
        QStringLiteral("communication.view"), QStringLiteral("system.manage"),
        QStringLiteral("remote.manage"), QStringLiteral("users.manage"),
        QStringLiteral("plugins.manage")
#ifdef INDUSTRIALHUB_HAS_SERIALBUS
        , QStringLiteral("system.manage")
#endif
    };
    for (int i = 0; i < ui->listWidget->count() && i < required.size(); ++i) {
        const bool allowed = m_authManager->hasPermission(required.at(i));
        ui->listWidget->item(i)->setHidden(!allowed);
    }

    const bool canManageDevices = m_authManager->hasPermission(QStringLiteral("device.manage"));
    if (devicePage) {
        for (QPushButton *button : devicePage->findChildren<QPushButton *>())
            button->setEnabled(canManageDevices);
    }
    const bool canManageCommunication = m_authManager->hasPermission(QStringLiteral("communication.manage"));
    if (configPage) {
        for (QPushButton *button : configPage->findChildren<QPushButton *>())
            button->setEnabled(canManageCommunication);
    }
    const bool canOperateMonitor = m_authManager->hasPermission(QStringLiteral("device.operate"));
    if (monitorPage) {
        for (QPushButton *button : monitorPage->findChildren<QPushButton *>())
            button->setEnabled(canOperateMonitor);
    }
    const bool canAcknowledge = m_authManager->hasPermission(QStringLiteral("alarm.acknowledge"));
    const bool canManageAlarmRules = m_authManager->hasPermission(QStringLiteral("alarm.manage"));
    if (alarmPage)
        alarmPage->setPermissions(canManageAlarmRules, canAcknowledge);
    const bool canMaintainHistory = m_authManager->hasPermission(QStringLiteral("history.maintenance"));
    if (historyPage && !canMaintainHistory) {
        for (QPushButton *button : historyPage->findChildren<QPushButton *>()) {
            const QString text = button->text();
            if (text.contains(QStringLiteral("清理")) || text.contains(QStringLiteral("维护")))
                button->setEnabled(canMaintainHistory);
        }
    }

    for (int i = 0; i < ui->listWidget->count(); ++i) {
        if (!ui->listWidget->item(i)->isHidden()) {
            ui->listWidget->setCurrentRow(i);
            ui->stackedWidget->setCurrentIndex(i);
            break;
        }
    }
}


void MainWindow::setDarkTheme(bool darkTheme)
{
    if (!qApp) return;
    if (!darkTheme) {
        qApp->setStyleSheet(QString());
        return;
    }
    qApp->setStyleSheet(QStringLiteral(
        "QWidget { background-color: #202124; color: #e8eaed; }"
        "QLineEdit, QPlainTextEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox,"
        "QTableWidget, QTreeWidget, QListWidget { background-color: #292a2d; color: #e8eaed;"
        " border: 1px solid #55585c; selection-background-color: #3d5a80; }"
        "QPushButton { background-color: #35363a; color: #f1f3f4; border: 1px solid #5f6368;"
        " padding: 5px 10px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #45474c; }"
        "QPushButton:disabled { color: #777; background-color: #2b2c30; }"
        "QHeaderView::section { background-color: #303134; color: #e8eaed; padding: 5px;"
        " border: 1px solid #55585c; }"
        "QGroupBox { border: 1px solid #5f6368; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QMenu { background-color: #292a2d; color: #e8eaed; border: 1px solid #55585c; }"
        "QMenu::item:selected { background-color: #3d5a80; }"
        "QToolTip { background-color: #303134; color: #fff; border: 1px solid #777; }"));
}

void MainWindow::initSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_trayIcon = new QSystemTrayIcon(
        style()->standardIcon(QStyle::SP_ComputerIcon), this);
    m_trayIcon->setToolTip(QStringLiteral("IndustrialHub 工业设备监控平台"));
    m_trayMenu = new QMenu(this);
    QAction *restoreAction = m_trayMenu->addAction(QStringLiteral("打开 IndustrialHub"));
    QAction *quitAction = m_trayMenu->addAction(QStringLiteral("退出程序"));
    connect(restoreAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        m_allowClose = true;
        close();
    });
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger
            || reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    m_trayIcon->show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_minimizeToTray && !m_allowClose && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        m_trayIcon->showMessage(QStringLiteral("IndustrialHub"),
            QStringLiteral("程序已最小化到系统托盘，双击图标可重新打开。"),
            QSystemTrayIcon::Information, 3000);
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::init_class()
{
    dashboardPage = new DashboardPage(this);
    devicePage = new DevicePage(m_deviceManager, this);
    monitorPage = new MonitorPage(m_deviceManager, m_communicationManager,
                                    m_deviceHealthMonitor, this);
    connect(monitorPage, &MonitorPage::businessDataGenerated,
            m_businessDataManager, &BusinessDataManager::updateDataPoints);
    connect(monitorPage, &MonitorPage::businessDataGenerated,
            m_alarmManager, &AlarmManager::evaluateDataPoints);
    trendPage = new TrendPage(m_communicationManager, m_businessDataManager, this);
    alarmPage = new AlarmPage(m_alarmManager, this);
    historyPage = new HistoryPage(m_dataManager, this);
    configPage = new ConfigPage(m_deviceManager, m_communicationManager,
                                m_protocolManager, this, m_deviceHealthMonitor,
                                m_businessDataManager, m_alarmManager);
    settingsPage = new SettingsPage(this);
    m_remotePage = new RemotePage(this);
    connect(m_remotePage, &RemotePage::businessDataReceived,
            m_businessDataManager, &BusinessDataManager::updateDataPoints);
    connect(m_remotePage, &RemotePage::businessDataReceived,
            m_alarmManager, &AlarmManager::evaluateDataPoints);
    m_userManagementPage = new UserManagementPage(m_authManager, this);
    m_pluginManager = new PluginManager(this);
    m_pluginManager->setCommunicationManager(m_communicationManager);
    m_pluginPage = new PluginPage(m_pluginManager, this);
#ifdef INDUSTRIALHUB_HAS_SERIALBUS
    m_simulatorPage = new SimulatorPage(this);
#endif

    // 页面添加顺序必须与左侧导航菜单顺序一致。
    ui->stackedWidget->addWidget(dashboardPage); // 0 运行总览
    ui->stackedWidget->addWidget(devicePage);    // 1 设备管理
    ui->stackedWidget->addWidget(monitorPage);   // 2 实时监控
    ui->stackedWidget->addWidget(trendPage);     // 3 实时趋势
    ui->stackedWidget->addWidget(alarmPage);     // 4 报警中心
    ui->stackedWidget->addWidget(historyPage);   // 5 历史数据
    ui->stackedWidget->addWidget(configPage);    // 6 通信配置
    ui->stackedWidget->addWidget(settingsPage);  // 7 系统设置
    ui->stackedWidget->addWidget(m_remotePage);  // 8 MQTT / WebSocket
    ui->stackedWidget->addWidget(m_userManagementPage); // 9 用户与角色
    ui->stackedWidget->addWidget(m_pluginPage); // 10 插件管理
#ifdef INDUSTRIALHUB_HAS_SERIALBUS
    ui->stackedWidget->addWidget(m_simulatorPage); // 11 模拟设备
#endif

    ui->listWidget->setObjectName("navigationList");
    ui->listWidget->setFixedWidth(210);
    ui->listWidget->addItems({QStringLiteral("运行总览"), QStringLiteral("设备管理"),
        QStringLiteral("实时监控"), QStringLiteral("实时趋势"), QStringLiteral("报警中心"),
        QStringLiteral("历史数据"), QStringLiteral("通信配置"), QStringLiteral("系统设置"),
        QStringLiteral("远程通信"), QStringLiteral("用户与权限"), QStringLiteral("插件管理")
#ifdef INDUSTRIALHUB_HAS_SERIALBUS
        , QStringLiteral("模拟设备")
#endif
    });
    for (int i = 0; i < ui->listWidget->count(); ++i)
        ui->listWidget->item(i)->setTextAlignment(Qt::AlignCenter);

    m_statusLabel = new CustomLabel(this);
    m_statusLabel->setText(QStringLiteral("系统就绪 | V0.15.0 | SQLite + TCP/UDP/RS485/CAN/CAN-FD/VirtualCAN/Modbus/MQTT/WebSocket/HTTP/Plugins/Simulator"));
    m_statusLabel->setMinimumSize(220, 24);
    statusBar()->addPermanentWidget(m_statusLabel);
    ui->listWidget->setCurrentRow(0);
}

MainWindow::~MainWindow()
{
    // Plugin factories capture plugin instances; release all workers before unloading DLLs.
    if (m_communicationManager) {
        const QStringList deviceIds = m_communicationManager->managedDeviceIds();
        for (const QString &deviceId : deviceIds)
            m_communicationManager->releaseDevice(deviceId);
    }
    if (m_pluginManager) {
        m_pluginManager->unloadAll();
        delete m_pluginManager;
        m_pluginManager = nullptr;
    }
    delete ui;
}
