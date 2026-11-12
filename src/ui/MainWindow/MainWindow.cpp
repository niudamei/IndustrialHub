#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "src/pages/DashboardPage.h"
#include "src/pages/devicepage.h"
#include "src/pages/devicemanager.h"
#include "src/pages/AlarmPage.h"
#include "src/pages/ConfigPage.h"
#include "src/pages/HistoryPage.h"
#include "src/pages/MonitorPage.h"
#include "src/pages/SettingsPage.h"
#include <QListWidgetItem>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_deviceManager(new DeviceManager(this))
{
    ui->setupUi(this);

    if (!m_deviceManager->isDatabaseReady()) {
        QMessageBox::warning(this, QStringLiteral("数据库初始化失败"),
                             m_deviceManager->lastError());
    } else if (m_deviceManager->getAllDevices().isEmpty()) {
        // 仅首次运行且数据库没有设备时写入演示数据，之后不再覆盖用户数据。
        m_deviceManager->addDevice(DeviceInfo("DEV001", "导航控制器", "控制器", "TCP",
                                               "192.168.2.125:5600", DeviceStatus::Online));
        m_deviceManager->addDevice(DeviceInfo("DEV002", "CAN采集盒", "采集设备", "CAN",
                                               "0x06069001", DeviceStatus::Offline));
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
    alarmPage = new AlarmPage(this);
    configPage = new ConfigPage(this);
    historyPage = new HistoryPage(this);
    monitorPage = new MonitorPage(this);
    settingsPage = new SettingsPage(this);

    ui->stackedWidget->addWidget(dashboardPage);
    ui->stackedWidget->addWidget(devicePage);
    ui->stackedWidget->addWidget(alarmPage);
    ui->stackedWidget->addWidget(configPage);
    ui->stackedWidget->addWidget(historyPage);
    ui->stackedWidget->addWidget(monitorPage);
    ui->stackedWidget->addWidget(settingsPage);

    ui->listWidget->setObjectName("navigationList");
    ui->listWidget->setFixedWidth(210);
    ui->listWidget->addItems({QStringLiteral("运行总览"), QStringLiteral("设备管理"),
        QStringLiteral("实时监控"), QStringLiteral("报警中心"), QStringLiteral("历史数据"),
        QStringLiteral("通信配置"), QStringLiteral("系统设置")});
    for (int i = 0; i < ui->listWidget->count(); ++i)
        ui->listWidget->item(i)->setTextAlignment(Qt::AlignCenter);

    m_statusLabel = new CustomLabel(this);
    m_statusLabel->setText(QStringLiteral("系统就绪 | V0.4.0 | SQLite + 通信接口"));
    m_statusLabel->setMinimumSize(220, 24);
    statusBar()->addPermanentWidget(m_statusLabel);
    ui->listWidget->setCurrentRow(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}
