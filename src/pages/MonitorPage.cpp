#include "MonitorPage.h"
#include "ui_monitorpage.h"

#include "src/communication/communicationmanager.h"
#include "src/pages/devicemanager.h"

#include <QDateTime>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTableWidgetItem>

MonitorPage::MonitorPage(DeviceManager *deviceManager,
                         CommunicationManager *communicationManager,
                         QWidget *parent)
    : QWidget(parent),
      ui(new Ui::MonitorPage),
      m_deviceManager(deviceManager),
      m_communicationManager(communicationManager)
{
    ui->setupUi(this);

    ui->eventTable->setColumnCount(4);
    ui->eventTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("设备编号"),
        QStringLiteral("方向/事件"),
        QStringLiteral("数据 / 详情")
    });
    ui->eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->eventTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->eventTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->eventTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->eventTable->verticalHeader()->setVisible(false);
    ui->eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->eventTable->setAlternatingRowColors(true);

    ui->directionFilterCombo->addItems({
        QStringLiteral("全部事件"),
        QStringLiteral("TX"),
        QStringLiteral("RX"),
        QStringLiteral("状态"),
        QStringLiteral("错误")
    });

    connect(ui->deviceFilterCombo, &QComboBox::currentIndexChanged,
            this, &MonitorPage::applyFilters);
    connect(ui->directionFilterCombo, &QComboBox::currentIndexChanged,
            this, &MonitorPage::applyFilters);
    connect(ui->clearButton, &QPushButton::clicked, this, [this]() {
        ui->eventTable->setRowCount(0);
        applyFilters();
    });
    connect(ui->pauseButton, &QPushButton::clicked,
            this, &MonitorPage::onPauseClicked);

    if (m_deviceManager) {
        connect(m_deviceManager, &DeviceManager::devicesChanged,
                this, &MonitorPage::refreshDeviceFilter);
        refreshDeviceFilter();
    }

    if (m_communicationManager) {
        connect(m_communicationManager, &CommunicationManager::deviceConnectionStateChanged,
                this, &MonitorPage::onConnectionStateChanged);
        connect(m_communicationManager, &CommunicationManager::deviceDataSent,
                this, &MonitorPage::onDataSent);
        connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
                this, &MonitorPage::onDataReceived);
        connect(m_communicationManager, &CommunicationManager::deviceErrorOccurred,
                this, &MonitorPage::onCommunicationError);
    }
}

MonitorPage::~MonitorPage()
{
    delete ui;
}

void MonitorPage::refreshDeviceFilter()
{
    const QString previousId = ui->deviceFilterCombo->currentData().toString();
    const QSignalBlocker blocker(ui->deviceFilterCombo);
    ui->deviceFilterCombo->clear();
    ui->deviceFilterCombo->addItem(QStringLiteral("全部设备"), QString());

    if (m_deviceManager) {
        const QList<DeviceInfo> devices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : devices) {
            ui->deviceFilterCombo->addItem(
                QStringLiteral("%1 | %2").arg(device.devId, device.devName),
                device.devId);
        }
    }

    const int index = ui->deviceFilterCombo->findData(previousId);
    if (index >= 0)
        ui->deviceFilterCombo->setCurrentIndex(index);

    ui->deviceCountLabel->setText(
        QStringLiteral("设备数量：%1").arg(m_deviceManager
            ? m_deviceManager->getAllDevices().size() : 0));
    applyFilters();
}

void MonitorPage::applyFilters()
{
    const QString selectedId = ui->deviceFilterCombo->currentData().toString();
    const QString selectedDirection = ui->directionFilterCombo->currentText();

    int visibleRows = 0;
    for (int row = 0; row < ui->eventTable->rowCount(); ++row) {
        const QString rowDevice = ui->eventTable->item(row, 1)
            ? ui->eventTable->item(row, 1)->text() : QString();
        const QString rowDirection = ui->eventTable->item(row, 2)
            ? ui->eventTable->item(row, 2)->text() : QString();

        const bool deviceMatches = selectedId.isEmpty()
            || rowDevice.compare(selectedId, Qt::CaseInsensitive) == 0;
        const bool directionMatches = selectedDirection == QStringLiteral("全部事件")
            || rowDirection == selectedDirection;
        const bool visible = deviceMatches && directionMatches;
        ui->eventTable->setRowHidden(row, !visible);
        if (visible)
            ++visibleRows;
    }

    ui->eventCountLabel->setText(
        QStringLiteral("显示事件：%1").arg(visibleRows));
}

void MonitorPage::onConnectionStateChanged(const QString &deviceId, bool connected)
{
    appendEvent(deviceId, QStringLiteral("状态"),
                connected ? QStringLiteral("通信通道已打开")
                          : QStringLiteral("通信通道已关闭"));
}

void MonitorPage::onDataSent(const QString &deviceId, const QByteArray &data)
{
    appendEvent(deviceId, QStringLiteral("TX"), formatData(deviceId, data));
}

void MonitorPage::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    appendEvent(deviceId, QStringLiteral("RX"), formatData(deviceId, data));
}

void MonitorPage::onCommunicationError(const QString &deviceId, const QString &message)
{
    appendEvent(deviceId.isEmpty() ? QStringLiteral("--") : deviceId,
                QStringLiteral("错误"), message);
}

void MonitorPage::onPauseClicked()
{
    m_paused = !m_paused;
    ui->pauseButton->setText(m_paused
        ? QStringLiteral("继续记录") : QStringLiteral("暂停记录"));
    ui->monitorStateLabel->setText(m_paused
        ? QStringLiteral("记录已暂停") : QStringLiteral("正在记录"));
}

void MonitorPage::appendEvent(const QString &deviceId,
                              const QString &direction,
                              const QString &content)
{
    if (m_paused)
        return;

    ui->eventTable->insertRow(0);
    const QStringList values = {
        QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
        deviceId,
        direction,
        content
    };

    for (int column = 0; column < values.size(); ++column) {
        auto *item = new QTableWidgetItem(values.at(column));
        if (column == 3)
            item->setToolTip(values.at(column));
        ui->eventTable->setItem(0, column, item);
    }

    while (ui->eventTable->rowCount() > MaxRows)
        ui->eventTable->removeRow(ui->eventTable->rowCount() - 1);

    applyFilters();
}

QString MonitorPage::formatData(const QString &deviceId, const QByteArray &data) const
{
    if (m_deviceManager) {
        DeviceInfo device;
        if (m_deviceManager->getDeviceById(deviceId, device)
            && (device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
                || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0)) {
            return QString::fromLatin1(data);
        }
    }

    return QString::fromLatin1(data.toHex(' ').toUpper());
}
