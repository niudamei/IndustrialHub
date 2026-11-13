#include "MonitorPage.h"
#include "ui_monitorpage.h"

#include "src/communication/communicationmanager.h"
#include "src/pages/devicemanager.h"

#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTimer>

namespace {
QString metricKey(const QString &deviceId)
{
    return deviceId.trimmed().toCaseFolded();
}

QString intervalText(qint64 value)
{
    return value < 0 ? QStringLiteral("--") : QStringLiteral("%1 ms").arg(value);
}
}

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

    ui->metricsTable->setColumnCount(13);
    ui->metricsTable->setHorizontalHeaderLabels({
        QStringLiteral("设备名称 / 编号"), QStringLiteral("通信类型"),
        QStringLiteral("连接状态"), QStringLiteral("接收状态"),
        QStringLiteral("最后接收时间"), QStringLiteral("距上次接收"),
        QStringLiteral("最近帧间隔"), QStringLiteral("平均间隔"), QStringLiteral("最小间隔"),
        QStringLiteral("最大间隔"), QStringLiteral("TX帧数"),
        QStringLiteral("RX帧数"), QStringLiteral("错误数")
    });
    ui->metricsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->metricsTable->horizontalHeader()->setStretchLastSection(true);
    ui->metricsTable->verticalHeader()->setVisible(false);
    ui->metricsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->metricsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->metricsTable->setAlternatingRowColors(true);

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

    m_statisticsTimer = new QTimer(this);
    m_statisticsTimer->setInterval(250);
    connect(m_statisticsTimer, &QTimer::timeout,
            this, &MonitorPage::updateLiveStatistics);
    m_statisticsTimer->start();
    updateLiveStatistics();
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

    QHash<QString, DeviceMetrics> refreshedMetrics;
    if (m_deviceManager) {
        const QList<DeviceInfo> devices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : devices) {
            const QString key = metricKey(device.devId);
            DeviceMetrics metrics = m_deviceMetrics.value(key);
            metrics.deviceName = device.devName;
            metrics.commType = device.commType;
            if (m_communicationManager)
                metrics.connected = m_communicationManager->isConnected(device.devId);
            refreshedMetrics.insert(key, metrics);

            ui->deviceFilterCombo->addItem(
                QStringLiteral("%1 | %2").arg(device.devId, device.devName),
                device.devId);
        }
    }
    m_deviceMetrics = refreshedMetrics;

    const int index = ui->deviceFilterCombo->findData(previousId);
    if (index >= 0)
        ui->deviceFilterCombo->setCurrentIndex(index);

    ui->deviceCountLabel->setText(
        QStringLiteral("设备数量：%1").arg(m_deviceMetrics.size()));
    refreshMetricsTable();
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
    const QString key = metricKey(deviceId);
    DeviceMetrics &metrics = m_deviceMetrics[key];
    if (metrics.deviceName.isEmpty())
        metrics.deviceName = deviceId;
    metrics.connected = connected;
    appendEvent(deviceId, QStringLiteral("状态"),
                connected ? QStringLiteral("通信通道已打开")
                          : QStringLiteral("通信通道已关闭"));
    updateLiveStatistics();
}

void MonitorPage::onDataSent(const QString &deviceId, const QByteArray &data)
{
    ++m_deviceMetrics[metricKey(deviceId)].txCount;
    appendEvent(deviceId, QStringLiteral("TX"), formatData(deviceId, data));
    updateLiveStatistics();
}

void MonitorPage::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    DeviceMetrics &metrics = m_deviceMetrics[metricKey(deviceId)];
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (metrics.lastReceiveMs > 0) {
        const qint64 interval = qMax<qint64>(0, nowMs - metrics.lastReceiveMs);
        metrics.lastIntervalMs = interval;
        metrics.intervalSumMs += interval;
        ++metrics.intervalCount;
        metrics.minIntervalMs = metrics.minIntervalMs < 0
            ? interval : qMin(metrics.minIntervalMs, interval);
        metrics.maxIntervalMs = qMax(metrics.maxIntervalMs, interval);
    }
    metrics.lastReceiveMs = nowMs;
    metrics.lastReceiveTime = QDateTime::currentDateTime();
    ++metrics.rxCount;
    appendEvent(deviceId, QStringLiteral("RX"), formatData(deviceId, data));
    updateLiveStatistics();
}

void MonitorPage::onCommunicationError(const QString &deviceId, const QString &message)
{
    if (!deviceId.isEmpty())
        ++m_deviceMetrics[metricKey(deviceId)].errorCount;
    appendEvent(deviceId.isEmpty() ? QStringLiteral("--") : deviceId,
                QStringLiteral("错误"), message);
    updateLiveStatistics();
}

void MonitorPage::onPauseClicked()
{
    m_paused = !m_paused;
    ui->pauseButton->setText(m_paused
        ? QStringLiteral("继续记录") : QStringLiteral("暂停记录"));
    ui->monitorStateLabel->setText(m_paused
        ? QStringLiteral("事件记录已暂停（统计继续）")
        : QStringLiteral("实时统计中"));
}

void MonitorPage::updateLiveStatistics()
{
    refreshMetricsTable();

    quint64 totalTx = 0;
    quint64 totalRx = 0;
    quint64 totalErrors = 0;
    int connectedCount = 0;
    for (auto it = m_deviceMetrics.cbegin(); it != m_deviceMetrics.cend(); ++it) {
        totalTx += it->txCount;
        totalRx += it->rxCount;
        totalErrors += it->errorCount;
        if (it->connected)
            ++connectedCount;
    }

    ui->connectedCountLabel->setText(
        QStringLiteral("已连接：%1 / %2").arg(connectedCount).arg(m_deviceMetrics.size()));
    ui->trafficSummaryLabel->setText(
        QStringLiteral("累计 TX：%1    RX：%2    错误：%3")
            .arg(totalTx).arg(totalRx).arg(totalErrors));
    ui->monitorStateLabel->setText(m_paused
        ? QStringLiteral("事件记录已暂停（统计继续）")
        : QStringLiteral("实时统计中"));
}

void MonitorPage::refreshMetricsTable()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    ui->metricsTable->setRowCount(m_deviceMetrics.size());

    QStringList keys = m_deviceMetrics.keys();
    keys.sort(Qt::CaseInsensitive);
    int row = 0;
    for (const QString &key : keys) {
        const DeviceMetrics &metrics = m_deviceMetrics[key];
        const QString lastTime = metrics.lastReceiveTime.isValid()
            ? metrics.lastReceiveTime.toString(QStringLiteral("HH:mm:ss.zzz"))
            : QStringLiteral("--");
        const QString receiveAge = metrics.lastReceiveMs > 0
            ? intervalText(nowMs - metrics.lastReceiveMs) : QStringLiteral("--");
        const QString latestInterval = intervalText(metrics.lastIntervalMs);
        const QString averageInterval = metrics.intervalCount > 0
            ? QStringLiteral("%1 ms").arg(
                  static_cast<double>(metrics.intervalSumMs) / metrics.intervalCount, 0, 'f', 1)
            : QStringLiteral("--");

        const QStringList values = {
            metrics.deviceName.isEmpty() ? key : QStringLiteral("%1 (%2)").arg(metrics.deviceName, key),
            metrics.commType,
            metrics.connected ? QStringLiteral("已连接") : QStringLiteral("未连接"),
            receiveStateText(metrics, nowMs),
            lastTime,
            receiveAge,
            latestInterval,
            averageInterval,
            intervalText(metrics.minIntervalMs),
            metrics.intervalCount > 0 ? intervalText(metrics.maxIntervalMs) : QStringLiteral("--"),
            QString::number(metrics.txCount),
            QString::number(metrics.rxCount),
            QString::number(metrics.errorCount)
        };

        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 2 || column == 3)
                item->setTextAlignment(Qt::AlignCenter);
            ui->metricsTable->setItem(row, column, item);
        }
        ++row;
    }
}

QString MonitorPage::receiveStateText(const DeviceMetrics &metrics, qint64 nowMs) const
{
    if (!metrics.connected)
        return QStringLiteral("未连接");
    if (metrics.lastReceiveMs <= 0)
        return QStringLiteral("等待数据");
    if (nowMs - metrics.lastReceiveMs > ReceiveTimeoutMs)
        return QStringLiteral("接收超时");
    return QStringLiteral("通信正常");
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
