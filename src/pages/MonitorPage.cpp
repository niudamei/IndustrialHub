#include "MonitorPage.h"
#include "ui_monitorpage.h"

#include "src/communication/communicationmanager.h"
#include "src/pages/devicemanager.h"

#include <QComboBox>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QLegend>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QSizePolicy>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTimer>
#include <QtMath>
#include <QVBoxLayout>

namespace {
QString metricKey(const QString &deviceId)
{
    return deviceId.trimmed().toCaseFolded();
}

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
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

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("总接收帧率趋势（最近 15 秒）"));
    chart->legend()->hide();
    m_receiveRateSeries = new QLineSeries(chart);
    m_receiveRateSeries->setName(QStringLiteral("RX FPS"));
    chart->addSeries(m_receiveRateSeries);

    m_receiveRateAxisX = new QValueAxis(chart);
    m_receiveRateAxisX->setTitleText(QStringLiteral("时间（秒）"));
    m_receiveRateAxisX->setRange(0.0, 15.0);
    m_receiveRateAxisX->setLabelFormat("%.1f");
    chart->addAxis(m_receiveRateAxisX, Qt::AlignBottom);
    m_receiveRateSeries->attachAxis(m_receiveRateAxisX);

    m_receiveRateAxisY = new QValueAxis(chart);
    m_receiveRateAxisY->setTitleText(QStringLiteral("帧/秒"));
    m_receiveRateAxisY->setRange(0.0, 10.0);
    chart->addAxis(m_receiveRateAxisY, Qt::AlignLeft);
    m_receiveRateSeries->attachAxis(m_receiveRateAxisY);

    m_trafficChartView = new QChartView(chart, ui->trafficChartHost);
    m_trafficChartView->setRenderHint(QPainter::Antialiasing);
    auto *chartLayout = new QVBoxLayout(ui->trafficChartHost);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_trafficChartView);


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

    ui->metricsTable->setColumnCount(16);
    ui->metricsTable->setHorizontalHeaderLabels({
        QStringLiteral("设备名称 / 编号"), QStringLiteral("通信类型"),
        QStringLiteral("连接状态"), QStringLiteral("接收状态"),
        QStringLiteral("最后接收时间"), QStringLiteral("距上次接收"),
        QStringLiteral("最近帧间隔"), QStringLiteral("平均间隔"), QStringLiteral("最小间隔"),
        QStringLiteral("最大间隔"), QStringLiteral("TX帧数"),
        QStringLiteral("RX帧数"), QStringLiteral("接收帧率 FPS"),
        QStringLiteral("超时次数"), QStringLiteral("恢复次数"), QStringLiteral("错误数")
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
    connect(ui->exportButton, &QPushButton::clicked,
            this, &MonitorPage::exportEventsToCsv);
    connect(ui->demoButton, &QPushButton::clicked,
            this, &MonitorPage::toggleDemoMode);
    ui->timeoutSpinBox->setValue(m_receiveTimeoutMs / 1000);
    connect(ui->timeoutSpinBox, &QSpinBox::valueChanged,
            this, &MonitorPage::onReceiveTimeoutChanged);

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
            if (m_communicationManager) {
                const bool isConnected = m_communicationManager->isConnected(device.devId);
                if (isConnected && (!metrics.connected || metrics.connectedSinceMs <= 0)) {
                    metrics.connectedSinceMs = QDateTime::currentMSecsSinceEpoch();
                    metrics.lastReceiveMs = 0;
                    metrics.receiveTimedOut = false;
                } else if (!isConnected) {
                    metrics.connectedSinceMs = 0;
                }
                metrics.connected = isConnected;
            }
            refreshedMetrics.insert(key, metrics);

            ui->deviceFilterCombo->addItem(
                QStringLiteral("%1 | %2").arg(device.devId, device.devName),
                device.devId);
        }
    }
    if (m_demoMode) {
        const QStringList demoIds = {
            QStringLiteral("DEMO-CAN-001"),
            QStringLiteral("DEMO-CANFD-002"),
            QStringLiteral("DEMO-TCP-003")
        };
        const QStringList demoNames = {
            QStringLiteral("模拟 CAN 采集盒 1"),
            QStringLiteral("模拟 CAN-FD 采集盒 2"),
            QStringLiteral("模拟导航控制器")
        };
        for (int i = 0; i < demoIds.size(); ++i) {
            const QString &id = demoIds.at(i);
            const QString key = metricKey(id);
            if (!m_deviceMetrics.contains(key))
                continue;
            DeviceMetrics metrics = m_deviceMetrics.value(key);
            refreshedMetrics.insert(key, metrics);
            ui->deviceFilterCombo->addItem(
                QStringLiteral("%1 | %2").arg(id, demoNames.at(i)), id);
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
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    metrics.connected = connected;
    if (connected) {
        metrics.connectedSinceMs = nowMs;
        metrics.lastReceiveMs = 0;
        metrics.receiveTimedOut = false;
    } else {
        metrics.connectedSinceMs = 0;
        metrics.lastReceiveMs = 0;
        metrics.receiveTimedOut = false;
    }
    appendEvent(deviceId, QStringLiteral("状态"),
                connected ? QStringLiteral("通信通道已打开，等待首帧数据")
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
    if (metrics.receiveTimedOut) {
        metrics.receiveTimedOut = false;
        ++metrics.recoveryCount;
        appendEvent(deviceId, QStringLiteral("状态"), QStringLiteral("数据接收已恢复"));
    }
    metrics.receiveTimestamps.enqueue(nowMs);
    while (!metrics.receiveTimestamps.isEmpty()
           && nowMs - metrics.receiveTimestamps.head() > 1000)
        metrics.receiveTimestamps.dequeue();
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

void MonitorPage::toggleDemoMode()
{
    if (!m_demoTimer) {
        m_demoTimer = new QTimer(this);
        m_demoTimer->setInterval(300);
        connect(m_demoTimer, &QTimer::timeout,
                this, &MonitorPage::generateDemoTraffic);
    }

    m_demoMode = !m_demoMode;
    if (m_demoMode) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const QStringList ids = {
            QStringLiteral("DEMO-CAN-001"),
            QStringLiteral("DEMO-CANFD-002"),
            QStringLiteral("DEMO-TCP-003")
        };
        const QStringList names = {
            QStringLiteral("模拟 CAN 采集盒 1"),
            QStringLiteral("模拟 CAN-FD 采集盒 2"),
            QStringLiteral("模拟导航控制器")
        };
        const QStringList types = {
            QStringLiteral("CAN"),
            QStringLiteral("CANFD"),
            QStringLiteral("TCP")
        };

        for (int i = 0; i < ids.size(); ++i) {
            DeviceMetrics &metrics = m_deviceMetrics[metricKey(ids.at(i))];
            metrics.deviceName = names.at(i);
            metrics.commType = types.at(i);
            metrics.connected = true;
            metrics.connectedSinceMs = nowMs;
            metrics.lastReceiveMs = 0;
            metrics.receiveTimedOut = false;
            ui->deviceFilterCombo->addItem(
                QStringLiteral("%1 | %2").arg(ids.at(i), names.at(i)), ids.at(i));
            appendEvent(ids.at(i), QStringLiteral("状态"),
                        QStringLiteral("模拟设备已连接（演示数据，非真实硬件）"));
        }

        ui->demoButton->setText(QStringLiteral("停止模拟"));
        ui->monitorStateLabel->setText(QStringLiteral("模拟数据运行中（非真实设备）"));
        m_demoDeviceIndex = 0;
        m_demoTimer->start();
        generateDemoTraffic();
    } else {
        m_demoTimer->stop();
        const QStringList ids = {
            QStringLiteral("DEMO-CAN-001"),
            QStringLiteral("DEMO-CANFD-002"),
            QStringLiteral("DEMO-TCP-003")
        };
        for (const QString &id : ids) {
            DeviceMetrics &metrics = m_deviceMetrics[metricKey(id)];
            metrics.connected = false;
            metrics.connectedSinceMs = 0;
            metrics.receiveTimedOut = false;
            appendEvent(id, QStringLiteral("状态"),
                        QStringLiteral("模拟数据已停止"));
        }
        ui->demoButton->setText(QStringLiteral("开始模拟"));
        ui->monitorStateLabel->setText(QStringLiteral("模拟数据已停止"));
    }

    refreshDeviceFilter();
    updateLiveStatistics();
}

void MonitorPage::generateDemoTraffic()
{
    if (!m_demoMode)
        return;

    const QStringList ids = {
        QStringLiteral("DEMO-CAN-001"),
        QStringLiteral("DEMO-CANFD-002"),
        QStringLiteral("DEMO-TCP-003")
    };

    for (int index = 0; index < ids.size(); ++index) {
        const QString &id = ids.at(index);
        const quint64 sequence = ++m_demoSequence;
        QByteArray txData;
        QByteArray rxData;

        if (index == 0) {
            txData = QByteArray("t1238A1B2C3D4E5F60708\r");
            rxData = QByteArray("t1248") + QByteArray::number(sequence % 16, 16).toUpper()
                + QByteArray("123456789ABCDEF\r");
        } else if (index == 1) {
            txData = QByteArray("D0018A1B2C3D4E5F60708\r");
            rxData = QByteArray("D0028") + QByteArray::number(sequence % 16, 16).toUpper()
                + QByteArray("123456789ABCDEF\r");
        } else {
            txData = QByteArray::fromHex("AA55010203040506");
            rxData = QByteArray::fromHex("AA55060708090A0B");
        }

        onDataSent(id, txData);
        onDataReceived(id, rxData);
    }
}

void MonitorPage::exportEventsToCsv()
{
    const QString defaultName = QStringLiteral("communication_events_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString fileName = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出通信事件记录"), defaultName,
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty())
        return;

    QStringList lines;
    lines << QStringList{
        csvEscape(QStringLiteral("时间")),
        csvEscape(QStringLiteral("设备编号")),
        csvEscape(QStringLiteral("方向/事件")),
        csvEscape(QStringLiteral("数据/详情"))
    }.join(QLatin1Char(','));

    int exportedRows = 0;
    // 表格最新事件在最上方，导出时反向遍历，保持时间正序。
    // 仅导出当前筛选后可见的记录，方便按设备或事件类型生成报表。
    for (int row = ui->eventTable->rowCount() - 1; row >= 0; --row) {
        if (ui->eventTable->isRowHidden(row))
            continue;

        QStringList fields;
        for (int column = 0; column < ui->eventTable->columnCount(); ++column) {
            const QTableWidgetItem *item = ui->eventTable->item(row, column);
            fields << csvEscape(item ? item->text() : QString());
        }
        lines << fields.join(QLatin1Char(','));
        ++exportedRows;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return;
    }

    // UTF-8 BOM 便于 Excel 正确识别中文。
    file.write(QByteArray::fromHex("efbbbf"));
    file.write((lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8());
    file.close();

    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出 %1 条当前可见事件记录。").arg(exportedRows));
}

void MonitorPage::onReceiveTimeoutChanged(int seconds)
{
    m_receiveTimeoutMs = seconds * 1000;
    updateLiveStatistics();
}

void MonitorPage::updateLiveStatistics()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_deviceMetrics.begin(); it != m_deviceMetrics.end(); ++it) {
        DeviceMetrics &metrics = it.value();
        const qint64 timeoutBaseMs = metrics.lastReceiveMs > 0
            ? metrics.lastReceiveMs : metrics.connectedSinceMs;
        if (metrics.connected && timeoutBaseMs > 0
            && !metrics.receiveTimedOut
            && nowMs - timeoutBaseMs > m_receiveTimeoutMs) {
            metrics.receiveTimedOut = true;
            ++metrics.timeoutCount;
            const QString detail = metrics.lastReceiveMs > 0
                ? QStringLiteral("接收数据超时（距最近一帧超过 %1 ms）").arg(m_receiveTimeoutMs)
                : QStringLiteral("连接后 %1 ms 内未收到首帧数据").arg(m_receiveTimeoutMs);
            appendEvent(it.key(), QStringLiteral("错误"), detail);
        }
    }
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

    double totalReceiveRate = 0.0;
    for (auto it = m_deviceMetrics.cbegin(); it != m_deviceMetrics.cend(); ++it)
        totalReceiveRate += receiveRate(it.value(), nowMs);
    if (m_receiveRateSeries && m_receiveRateAxisX && m_receiveRateAxisY) {
        const qreal x = static_cast<qreal>(m_chartSampleIndex++) * 0.25;
        m_receiveRateSeries->append(x, totalReceiveRate);
        while (m_receiveRateSeries->count() > 61)
            m_receiveRateSeries->remove(0);
        const qreal left = qMax<qreal>(0.0, x - 15.0);
        m_receiveRateAxisX->setRange(left, qMax<qreal>(15.0, x));
        qreal maxRate = 10.0;
        for (const QPointF &point : m_receiveRateSeries->points())
            maxRate = qMax(maxRate, point.y());
        m_receiveRateAxisY->setRange(0.0, qMax<qreal>(10.0, qCeil(maxRate / 5.0) * 5.0));
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
            QStringLiteral("%1").arg(receiveRate(metrics, nowMs), 0, 'f', 1),
            QString::number(metrics.timeoutCount),
            QString::number(metrics.recoveryCount),
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

double MonitorPage::receiveRate(const DeviceMetrics &metrics, qint64 nowMs) const
{
    int recentFrames = 0;
    for (qint64 timestamp : metrics.receiveTimestamps) {
        if (nowMs - timestamp <= 1000)
            ++recentFrames;
    }
    return static_cast<double>(recentFrames);
}

QString MonitorPage::receiveStateText(const DeviceMetrics &metrics, qint64 nowMs) const
{
    if (!metrics.connected)
        return QStringLiteral("未连接");
    if (metrics.lastReceiveMs <= 0) {
        if (metrics.receiveTimedOut
            || (metrics.connectedSinceMs > 0
                && nowMs - metrics.connectedSinceMs > m_receiveTimeoutMs))
            return QStringLiteral("接收超时（无首帧）");
        return QStringLiteral("等待首帧");
    }
    if (nowMs - metrics.lastReceiveMs > m_receiveTimeoutMs)
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
    if (deviceId.startsWith(QStringLiteral("DEMO-CAN"), Qt::CaseInsensitive))
        return QString::fromLatin1(data);
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
