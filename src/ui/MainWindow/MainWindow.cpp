#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "src/pages/DashboardPage.h"
#include "src/pages/DevicePage.h"
#include "src/pages/AlarmPage.h"
#include "src/pages/ConfigPage.h"
#include "src/pages/HistoryPage.h"
#include "src/pages/MonitorPage.h"
#include "src/pages/SettingsPage.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init_class();
    connect(ui->listWidget, &QListWidget::currentRowChanged, this, &MainWindow::onNavigationClicked);
}

void MainWindow::onNavigationClicked(int index){
    ui->stackedWidget->setCurrentIndex(index);
}
void MainWindow::init_class(){
    dashboardPage = new DashboardPage();
    devicePage    = new DevicePage();
    alarmPage     = new AlarmPage();
    configPage    = new ConfigPage();
    historyPage   = new HistoryPage();
    monitorPage   = new MonitorPage();
    settingsPage  = new SettingsPage();
    ui->stackedWidget->addWidget(dashboardPage);
    ui->stackedWidget->addWidget(devicePage);
    ui->stackedWidget->addWidget(alarmPage);
    ui->stackedWidget->addWidget(configPage);
    ui->stackedWidget->addWidget(historyPage);
    ui->stackedWidget->addWidget(monitorPage);
    ui->stackedWidget->addWidget(settingsPage);
    ui->listWidget->setObjectName("navigationList");
    ui->listWidget->setFixedWidth(210);
    ui->listWidget->addItem("运行总览");
    ui->listWidget->addItem("设备管理");
    ui->listWidget->addItem("实时监控");
    ui->listWidget->addItem("报警中心");
    ui->listWidget->addItem("历史数据");
    ui->listWidget->addItem("通信配置");
    ui->listWidget->addItem("系统设置");
    ui->listWidget->setCurrentRow(0);
    for(int i = 0; i < ui->listWidget->count(); i++)
    {
        QListWidgetItem* item = ui->listWidget->item(i);
        item->setTextAlignment(Qt::AlignCenter);
    }
    m_statusLabel = new CustomLabel(this);
    m_statusLabel->setText("系统就绪 | V0.1.0");
    m_statusLabel->setMinimumSize(180, 24);
    statusBar()->addPermanentWidget(m_statusLabel);
}
MainWindow::~MainWindow()
{
    delete ui;
}
