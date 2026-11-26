#include "TrendPage.h"

#include "src/communication/communicationmanager.h"
#include "src/core/businessdatamanager.h"
#include "src/protocol/businessdatapoint.h"
#include "src/monitoring/trendhistorybuffer.h"
#include "src/monitoring/monitorstatistics.h"
#include "src/simulation/demofaultpolicy.h"

#include <QChart>
#include <QChartView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <limits>
#include <QTabWidget>
#include <QComboBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QLegend>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QSignalBlocker>
#include <QTimer>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QValueAxis>
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTimeAxis>
#include <QDateTimeAxis>
#include <QtMath>
#include <QVBoxLayout>

TrendPage::TrendPage(CommunicationManager *communicationManager,
                     BusinessDataManager *businessDataManager,
                     QWidget *parent)
    : QWidget(parent),
      ui(new Ui::TrendPage),
      m_communicationManager(communicationManager),
      m_businessDataManager(businessDataManager)
{
    m_monotonicClock.start();
    ui->setupUi(this);
    m_diagnostics = new CommunicationDiagnostics(nullptr, this);
    setupBusinessTrend();
    setupCommunicationHistoryTrend();

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("真实 / 演示 TX/RX 帧率（最近 15 秒）"));
    chart->legend()->setVisible(true);

    m_txSeries = new QLineSeries(chart);
    m_txSeries->setName(QStringLiteral("TX 发送帧率"));
    chart->addSeries(m_txSeries);

    m_rxSeries = new QLineSeries(chart);
    m_rxSeries->setName(QStringLiteral("真实 RX"));
    chart->addSeries(m_rxSeries);

    m_txSeries->setName(QStringLiteral("真实 TX"));
    m_demoTxSeries = new QLineSeries(chart);
    m_demoTxSeries->setName(QStringLiteral("演示 TX"));
    chart->addSeries(m_demoTxSeries);
    m_demoRxSeries = new QLineSeries(chart);
    m_demoRxSeries->setName(QStringLiteral("演示 RX"));
    chart->addSeries(m_demoRxSeries);
    QPen demoTxPen = m_demoTxSeries->pen();
    demoTxPen.setStyle(Qt::DashLine);
    m_demoTxSeries->setPen(demoTxPen);
    QPen demoRxPen = m_demoRxSeries->pen();
    demoRxPen.setStyle(Qt::DashLine);
    m_demoRxSeries->setPen(demoRxPen);

    m_axisX = new QValueAxis(chart);
    m_axisX->setTitleText(QStringLiteral("时间（秒）"));
    m_axisX->setRange(0.0, 15.0);
    m_axisX->setLabelFormat("%.1f");
    chart->addAxis(m_axisX, Qt::AlignBottom);
    m_txSeries->attachAxis(m_axisX);
    m_rxSeries->attachAxis(m_axisX);
    m_demoTxSeries->attachAxis(m_axisX);
    m_demoRxSeries->attachAxis(m_axisX);

    m_axisY = new QValueAxis(chart);
    m_axisY->setTitleText(QStringLiteral("帧 / 秒（FPS）"));
    m_axisY->setRange(0.0, 10.0);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_txSeries->attachAxis(m_axisY);
    m_rxSeries->attachAxis(m_axisY);
    m_demoTxSeries->attachAxis(m_axisY);
    m_demoRxSeries->attachAxis(m_axisY);

    m_chartView = new QChartView(chart, ui->chartHost);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    auto *chartLayout = new QVBoxLayout(ui->chartHost);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_chartView);

    ui->analysisTable->setColumnCount(10);
    ui->analysisTable->setHorizontalHeaderLabels({
        QStringLiteral("设备编号"), QStringLiteral("数据来源"), QStringLiteral("连接状态"),
        QStringLiteral("TX 帧数"), QStringLiteral("RX 帧数"),
        QStringLiteral("TX 字节"), QStringLiteral("RX 字节"),
        QStringLiteral("错误数"), QStringLiteral("最近 RX 时间"),
        QStringLiteral("距最近 RX")
    });
    ui->analysisTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->analysisTable->horizontalHeader()->setStretchLastSection(true);
    ui->analysisTable->verticalHeader()->setVisible(false);
    ui->analysisTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->analysisTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->analysisTable->setAlternatingRowColors(true);

    ui->deviceCombo->addItem(QStringLiteral("全部设备"), QString());
    connect(ui->deviceCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_txSeries->clear();
        m_rxSeries->clear();
        m_demoTxSeries->clear();
        m_demoRxSeries->clear();
        m_sampleIndex = 0;
        updateTrend();
        refreshCommunicationHistoryTrend();
    });

    if (m_communicationManager) {
        connect(m_communicationManager, &CommunicationManager::deviceDataSent,
                this, &TrendPage::onDataSent);
        connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
                this, &TrendPage::onDataReceived);
        connect(m_communicationManager, &CommunicationManager::deviceConnectionStateChanged,
                this, &TrendPage::onConnectionStateChanged);
        connect(m_communicationManager, &CommunicationManager::deviceErrorOccurred,
                this, &TrendPage::onCommunicationError);
    }

    refreshDeviceList();
    m_timer = new QTimer(this);
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &TrendPage::updateTrend);
    m_timer->start();
    updateTrend();
}

TrendPage::~TrendPage()
{
    delete ui;
}

qint64 TrendPage::monotonicNowMs() const
{
    // +1 preserves the positive sentinel used for an unset receive timestamp.
    return m_monotonicClock.elapsed() + 1;
}

void TrendPage::onDataSent(const QString &deviceId, const QByteArray &data)
{
    recordTraffic(deviceId, true);
    m_traffic[deviceId.trimmed()].txBytes += static_cast<quint64>(data.size());
}

void TrendPage::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    recordTraffic(deviceId, false);
    m_traffic[deviceId.trimmed()].rxBytes += static_cast<quint64>(data.size());
}

void TrendPage::onDemoTrafficGenerated(const QString &deviceId,
                                       bool isTx,
                                       qint64 byteCount)
{
    const QString id = deviceId.trimmed();
    if (!DemoFaultPolicy::isDemoDeviceId(id) || byteCount < 0)
        return;

    const bool isNewDevice = !m_traffic.contains(id);
    m_traffic[id].demoData = true;
    if (isNewDevice)
        refreshDeviceList();

    recordTraffic(id, isTx);
    DeviceTraffic &traffic = m_traffic[id];
    if (isTx)
        traffic.txBytes += static_cast<quint64>(byteCount);
    else
        traffic.rxBytes += static_cast<quint64>(byteCount);
}

void TrendPage::onDemoDeviceConnectionChanged(const QString &deviceId, bool connected)
{
    const QString id = deviceId.trimmed();
    if (!DemoFaultPolicy::isDemoDeviceId(id))
        return;

    const bool isNewDevice = !m_traffic.contains(id);
    DeviceTraffic &traffic = m_traffic[id];
    traffic.demoData = true;
    traffic.connected = connected;
    if (isNewDevice)
        refreshDeviceList();
}

void TrendPage::onDemoBusinessDataGenerated(
    const QString &deviceId,
    const QVector<BusinessDataPoint> &points)
{
    const QString id = deviceId.trimmed();
    if (!DemoFaultPolicy::isDemoDeviceId(id))
        return;

    static const QStringList variableIds = {
        QStringLiteral("temperature"), QStringLiteral("rpm"), QStringLiteral("current")
    };

    for (const BusinessDataPoint &point : points) {
        if (!point.isDemoData()
            || point.deviceId.compare(id, Qt::CaseInsensitive) != 0
            || point.quality != DataQuality::Good
            || !point.timestamp.isValid()
            || !qIsFinite(point.value)) {
            continue;
        }

        const int index = variableIds.indexOf(point.variableId.toCaseFolded());
        if (index < 0 || index >= m_businessCharts.size())
            continue;

        const QString seriesKey = id + QLatin1Char('|') + point.variableId.toCaseFolded();
        const qint64 timestamp = point.timestamp.toMSecsSinceEpoch();
        if (m_lastDemoBusinessTimestamp.value(seriesKey, -1) == timestamp)
            continue;

        QLineSeries *series = m_demoBusinessSeries[index].value(id);
        if (!series) {
            series = new QLineSeries(m_businessCharts[index]);
            series->setName(QStringLiteral("演示 | %1").arg(id));
            QPen pen = series->pen();
            pen.setStyle(Qt::DashLine);
            series->setPen(pen);
            m_businessCharts[index]->addSeries(series);
            series->attachAxis(m_businessCharts[index]->axes(Qt::Horizontal).first());
            series->attachAxis(m_businessAxisY[index]);
            m_demoBusinessSeries[index].insert(id, series);
        }

        TrendHistoryBuffer::appendBounded(
            series, static_cast<qreal>(timestamp), point.value);
        m_lastDemoBusinessTimestamp.insert(seriesKey, timestamp);
    }
}

void TrendPage::onConnectionStateChanged(const QString &deviceId, bool connected)
{
    const QString id = deviceId.trimmed();
    if (id.isEmpty())
        return;
    const bool isNewDevice = !m_traffic.contains(id);
    m_traffic[id].connected = connected;
    if (isNewDevice)
        refreshDeviceList();
}

void TrendPage::onCommunicationError(const QString &deviceId, const QString &)
{
    const QString id = deviceId.trimmed();
    if (id.isEmpty())
        return;
    const bool isNewDevice = !m_traffic.contains(id);
    ++m_traffic[id].errors;
    if (isNewDevice)
        refreshDeviceList();
}

void TrendPage::recordTraffic(const QString &deviceId, bool isTx)
{
    const QString id = deviceId.trimmed();
    if (id.isEmpty())
        return;

    const bool isNewDevice = !m_traffic.contains(id);
    const qint64 nowMs = monotonicNowMs();
    DeviceTraffic &traffic = m_traffic[id];
    (isTx ? traffic.txTimes : traffic.rxTimes).enqueue(nowMs);
    if (isTx) {
        ++traffic.txFrames;
    } else {
        ++traffic.rxFrames;
        traffic.lastRxMonotonicMs = nowMs;
        traffic.lastRxMs = QDateTime::currentMSecsSinceEpoch();
    }
    if (isNewDevice)
        refreshDeviceList();
}

void TrendPage::refreshDeviceList()
{
    const QString selectedId = ui->deviceCombo->currentData().toString();
    const QSignalBlocker blocker(ui->deviceCombo);
    ui->deviceCombo->clear();
    ui->deviceCombo->addItem(QStringLiteral("全部设备"), QString());

    QStringList ids = m_traffic.keys();
    if (m_diagnostics) {
        for (const CommunicationDeviceStatistics &statistics : m_diagnostics->statistics()) {
            if (!ids.contains(statistics.deviceId, Qt::CaseInsensitive))
                ids.append(statistics.deviceId);
        }
    }
    ids.sort(Qt::CaseInsensitive);
    for (const QString &id : ids) {
        const bool isDemo = m_traffic.value(id).demoData;
        const QString label = isDemo
            ? QStringLiteral("演示 | %1").arg(id)
            : QStringLiteral("真实 | %1").arg(id);
        ui->deviceCombo->addItem(label, id);
    }

    const int index = ui->deviceCombo->findData(selectedId);
    if (index >= 0)
        ui->deviceCombo->setCurrentIndex(index);
}

int TrendPage::countRecent(const QQueue<qint64> &times, qint64 nowMs) const
{
    return MonitorStatistics::countMonotonicRecent(times, nowMs);
}

 
void TrendPage::setupBusinessTrend()
{
    auto *group = new QGroupBox(QStringLiteral("真实 / 演示业务变量趋势（最近 60 秒）"), this);
    auto *layout = new QVBoxLayout(group);
    m_businessTabs = new QTabWidget(group);

    const QStringList names = {
        QStringLiteral("温度"), QStringLiteral("转速"), QStringLiteral("电流")
    };
    const QStringList ids = {
        QStringLiteral("temperature"), QStringLiteral("rpm"), QStringLiteral("current")
    };
    const QStringList units = {
        QStringLiteral("°C"), QStringLiteral("rpm"), QStringLiteral("A")
    };

    for (int i = 0; i < names.size(); ++i) {
        auto *chart = new QChart();
        chart->setTitle(names.at(i) + QStringLiteral("（") + units.at(i) + QStringLiteral("）"));
        chart->legend()->setVisible(true);
        auto *axisX = new QDateTimeAxis(chart);
        axisX->setTitleText(QStringLiteral("时间"));
        axisX->setFormat(QStringLiteral("HH:mm:ss"));
        axisX->setRange(QDateTime::currentDateTime().addSecs(-60),
                        QDateTime::currentDateTime());
        auto *axisY = new QValueAxis(chart);
        axisY->setTitleText(units.at(i));
        axisY->setRange(0.0, 100.0);
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        auto *view = new QChartView(chart, m_businessTabs);
        view->setRenderHint(QPainter::Antialiasing);
        m_businessTabs->addTab(view, names.at(i));
        m_businessCharts.append(chart);
        m_businessAxisY.append(axisY);
        m_businessSeries.append(QHash<QString, QLineSeries *>());
        m_demoBusinessSeries.append(QHash<QString, QLineSeries *>());
        auto *stats = new QLabel(QStringLiteral("真实数据：暂无有效数据"), group);
        stats->setWordWrap(true);
        m_businessStats.append(stats);
        auto *demoStats = new QLabel(QStringLiteral("演示数据：暂无有效数据"), group);
        demoStats->setWordWrap(true);
        m_demoBusinessStats.append(demoStats);
    }

    layout->addWidget(m_businessTabs);
    for (int i = 0; i < m_businessStats.size(); ++i) {
        layout->addWidget(m_businessStats.at(i));
        layout->addWidget(m_demoBusinessStats.at(i));
    }

    if (auto *mainLayout = qobject_cast<QVBoxLayout *>(this->layout()))
        mainLayout->insertWidget(4, group);
}

void TrendPage::updateBusinessTrend(qint64 nowMs)
{
    const QString selectedId = ui->deviceCombo->currentData().toString();
    const QList<BusinessDataPoint> points = m_businessDataManager
        ? m_businessDataManager->latestValues(selectedId)
        : QList<BusinessDataPoint>();
    const QStringList variableIds = {
        QStringLiteral("temperature"), QStringLiteral("rpm"), QStringLiteral("current")
    };
    const qreal leftValue = static_cast<qreal>(nowMs - 60000);

    // Real business values come only from the real-data manager.
    for (const BusinessDataPoint &point : points) {
        if (point.isDemoData() || point.quality != DataQuality::Good
            || !point.timestamp.isValid() || !qIsFinite(point.value)) {
            continue;
        }
        const int index = variableIds.indexOf(point.variableId.toCaseFolded());
        if (index < 0 || index >= m_businessCharts.size())
            continue;

        const QString seriesKey = point.deviceId + QLatin1Char('|')
            + point.variableId.toCaseFolded();
        const qint64 timestamp = point.timestamp.toMSecsSinceEpoch();
        if (m_lastBusinessTimestamp.value(seriesKey, -1) == timestamp)
            continue;

        QLineSeries *series = m_businessSeries[index].value(point.deviceId);
        if (!series) {
            series = new QLineSeries(m_businessCharts[index]);
            series->setName(QStringLiteral("真实 | %1").arg(point.deviceId));
            m_businessCharts[index]->addSeries(series);
            series->attachAxis(m_businessCharts[index]->axes(Qt::Horizontal).first());
            series->attachAxis(m_businessAxisY[index]);
            m_businessSeries[index].insert(point.deviceId, series);
        }
        TrendHistoryBuffer::appendBounded(
            series, static_cast<qreal>(timestamp), point.value);
        m_lastBusinessTimestamp.insert(seriesKey, timestamp);
    }

    for (int i = 0; i < m_businessCharts.size(); ++i) {
        qreal combinedMin = std::numeric_limits<qreal>::max();
        qreal combinedMax = std::numeric_limits<qreal>::lowest();
        bool hasVisibleSamples = false;

        auto updateSource = [&](QHash<QString, QLineSeries *> &sourceSeries,
                                QLabel *statsLabel,
                                const QString &sourceName) {
            qreal minValue = std::numeric_limits<qreal>::max();
            qreal maxValue = std::numeric_limits<qreal>::lowest();
            qreal sum = 0.0;
            qreal currentValue = 0.0;
            qreal currentTimestamp = std::numeric_limits<qreal>::lowest();
            int count = 0;

            for (auto it = sourceSeries.begin(); it != sourceSeries.end(); ++it) {
                QLineSeries *series = it.value();
                const bool visible = selectedId.isEmpty()
                    || it.key().compare(selectedId, Qt::CaseInsensitive) == 0;
                series->setVisible(visible);

                const auto oldPoints = series->points();
                QVector<QPointF> kept;
                kept.reserve(oldPoints.size());
                for (const QPointF &sample : oldPoints) {
                    if (sample.x() < leftValue)
                        continue;
                    kept.append(sample);
                    if (!visible)
                        continue;

                    minValue = qMin(minValue, sample.y());
                    maxValue = qMax(maxValue, sample.y());
                    sum += sample.y();
                    ++count;
                    if (sample.x() >= currentTimestamp) {
                        currentTimestamp = sample.x();
                        currentValue = sample.y();
                    }
                }
                if (kept.size() != oldPoints.size())
                    series->replace(kept);
            }

            if (count > 0) {
                hasVisibleSamples = true;
                combinedMin = qMin(combinedMin, minValue);
                combinedMax = qMax(combinedMax, maxValue);
                statsLabel->setText(
                    QStringLiteral("%1：当前值 %2    最大值 %3    最小值 %4    平均值 %5")
                        .arg(sourceName)
                        .arg(currentValue, 0, 'f', 2)
                        .arg(maxValue, 0, 'f', 2)
                        .arg(minValue, 0, 'f', 2)
                        .arg(sum / count, 0, 'f', 2));
            } else {
                statsLabel->setText(sourceName + QStringLiteral("：暂无有效数据"));
            }
        };

        updateSource(m_businessSeries[i], m_businessStats[i], QStringLiteral("真实数据"));
        updateSource(m_demoBusinessSeries[i], m_demoBusinessStats[i],
                     QStringLiteral("演示数据"));

        auto *axisX = qobject_cast<QDateTimeAxis *>(
            m_businessCharts[i]->axes(Qt::Horizontal).first());
        axisX->setRange(QDateTime::fromMSecsSinceEpoch(nowMs - 60000),
                        QDateTime::fromMSecsSinceEpoch(nowMs));

        if (hasVisibleSamples) {
            const qreal margin = qMax<qreal>(1.0, (combinedMax - combinedMin) * 0.15);
            m_businessAxisY[i]->setRange(combinedMin - margin, combinedMax + margin);
        } else {
            m_businessAxisY[i]->setRange(0.0, 100.0);
        }
    }
}

void TrendPage::updateTrend()
{
    const qint64 nowMonotonicMs = monotonicNowMs();
    const qint64 nowWallMs = QDateTime::currentMSecsSinceEpoch();
    updateBusinessTrend(nowWallMs);
    const QString selectedId = ui->deviceCombo->currentData().toString();
    int realTxRate = 0;
    int realRxRate = 0;
    int demoTxRate = 0;
    int demoRxRate = 0;

    for (auto it = m_traffic.begin(); it != m_traffic.end(); ++it) {
        MonitorStatistics::pruneMonotonicTimestamps(it->txTimes, nowMonotonicMs);
        MonitorStatistics::pruneMonotonicTimestamps(it->rxTimes, nowMonotonicMs);
        if (!selectedId.isEmpty()
            && it.key().compare(selectedId, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const int deviceTxRate = countRecent(it->txTimes, nowMonotonicMs);
        const int deviceRxRate = countRecent(it->rxTimes, nowMonotonicMs);
        if (it->demoData) {
            demoTxRate += deviceTxRate;
            demoRxRate += deviceRxRate;
        } else {
            realTxRate += deviceTxRate;
            realRxRate += deviceRxRate;
        }
    }

    const qreal x = static_cast<qreal>(m_sampleIndex++) * 0.25;
    m_txSeries->append(x, realTxRate);
    m_rxSeries->append(x, realRxRate);
    m_demoTxSeries->append(x, demoTxRate);
    m_demoRxSeries->append(x, demoRxRate);
    for (QLineSeries *series : {m_txSeries, m_rxSeries, m_demoTxSeries, m_demoRxSeries}) {
        while (series->count() > 61)
            series->remove(0);
    }

    m_axisX->setRange(qMax<qreal>(0.0, x - 15.0), qMax<qreal>(15.0, x));
    const qreal maxRate = qMax<qreal>(
        10.0, qMax<qreal>(qMax<qreal>(realTxRate, realRxRate),
                          qMax<qreal>(demoTxRate, demoRxRate)));
    m_axisY->setRange(0.0, qMax<qreal>(10.0, qCeil(maxRate / 5.0) * 5.0));

    ui->txRateLabel->setText(
        QStringLiteral("TX：真实 %1 / 演示 %2 帧/秒").arg(realTxRate).arg(demoTxRate));
    ui->rxRateLabel->setText(
        QStringLiteral("RX：真实 %1 / 演示 %2 帧/秒").arg(realRxRate).arg(demoRxRate));
    if (selectedId.isEmpty()) {
        ui->trendHintLabel->setText(
            QStringLiteral("全部设备：实线=真实通信，虚线=演示数据"));
    } else {
        const QString source = m_traffic.value(selectedId).demoData
            ? QStringLiteral("演示数据") : QStringLiteral("真实通信");
        ui->trendHintLabel->setText(
            QStringLiteral("当前设备：%1（%2）").arg(selectedId, source));
    }

    QStringList deviceIds = m_traffic.keys();
    deviceIds.sort(Qt::CaseInsensitive);
    QList<QString> visibleIds;
    for (const QString &id : deviceIds) {
        if (selectedId.isEmpty()
            || id.compare(selectedId, Qt::CaseInsensitive) == 0)
            visibleIds.append(id);
    }

    ui->analysisTable->setRowCount(visibleIds.size());
    quint64 realTxFrames = 0;
    quint64 realRxFrames = 0;
    quint64 realTxBytes = 0;
    quint64 realRxBytes = 0;
    quint64 realErrors = 0;
    quint64 demoTxFrames = 0;
    quint64 demoRxFrames = 0;
    quint64 demoTxBytes = 0;
    quint64 demoRxBytes = 0;
    quint64 demoErrors = 0;
    int row = 0;
    for (const QString &id : visibleIds) {
        const DeviceTraffic &traffic = m_traffic[id];
        if (traffic.demoData) {
            demoTxFrames += traffic.txFrames;
            demoRxFrames += traffic.rxFrames;
            demoTxBytes += traffic.txBytes;
            demoRxBytes += traffic.rxBytes;
            demoErrors += traffic.errors;
        } else {
            realTxFrames += traffic.txFrames;
            realRxFrames += traffic.rxFrames;
            realTxBytes += traffic.txBytes;
            realRxBytes += traffic.rxBytes;
            realErrors += traffic.errors;
        }

        const QString lastRx = traffic.lastRxMs > 0
            ? QDateTime::fromMSecsSinceEpoch(traffic.lastRxMs)
                  .toString(QStringLiteral("HH:mm:ss.zzz"))
            : QStringLiteral("--");
        const QString rxAge = traffic.lastRxMonotonicMs > 0
            ? QStringLiteral("%1 ms").arg(nowMonotonicMs - traffic.lastRxMonotonicMs)
            : QStringLiteral("--");
        const QStringList values = {
            id,
            traffic.demoData ? QStringLiteral("演示数据") : QStringLiteral("真实通信"),
            traffic.connected ? QStringLiteral("已连接") : QStringLiteral("未连接/未知"),
            QString::number(traffic.txFrames),
            QString::number(traffic.rxFrames),
            QString::number(traffic.txBytes),
            QString::number(traffic.rxBytes),
            QString::number(traffic.errors),
            lastRx,
            rxAge
        };
        for (int column = 0; column < values.size(); ++column)
            ui->analysisTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        ++row;
    }

    ui->analysisSummaryLabel->setText(
        QStringLiteral("真实通信：TX %1 帧 / %2 B，RX %3 帧 / %4 B，错误 %5"
                       "    |    演示数据：TX %6 帧 / %7 B，RX %8 帧 / %9 B，错误 %10")
            .arg(realTxFrames).arg(realTxBytes).arg(realRxFrames).arg(realRxBytes)
            .arg(realErrors).arg(demoTxFrames).arg(demoTxBytes)
            .arg(demoRxFrames).arg(demoRxBytes).arg(demoErrors));
}

void TrendPage::setupCommunicationHistoryTrend()
{
    auto *group = new QGroupBox(QStringLiteral("历史通信趋势（来自 SQLite 诊断记录）"), this);
    auto *layout = new QVBoxLayout(group);
    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("时间范围："), group));
    m_historyRangeCombo = new QComboBox(group);
    m_historyRangeCombo->addItem(QStringLiteral("最近 1 小时"), 1);
    m_historyRangeCombo->addItem(QStringLiteral("最近 24 小时"), 24);
    m_historyRangeCombo->addItem(QStringLiteral("最近 7 天"), 168);
    m_historyRangeCombo->addItem(QStringLiteral("最近 30 天"), 720);
    controls->addWidget(m_historyRangeCombo);
    auto *refreshButton = new QPushButton(QStringLiteral("查询历史趋势"), group);
    auto *exportButton = new QPushButton(QStringLiteral("导出趋势 CSV"), group);
    controls->addWidget(refreshButton);
    controls->addWidget(exportButton);
    controls->addStretch(1);
    layout->addLayout(controls);

    m_historyChart = new QChart();
    m_historyChart->setTitle(QStringLiteral("收发帧数、错误/超时与重连次数"));
    m_historyChart->legend()->setVisible(true);
    m_historyTxSeries = new QLineSeries(m_historyChart);
    m_historyTxSeries->setName(QStringLiteral("TX 帧数"));
    m_historyRxSeries = new QLineSeries(m_historyChart);
    m_historyRxSeries->setName(QStringLiteral("RX 帧数"));
    m_historyErrorSeries = new QLineSeries(m_historyChart);
    m_historyErrorSeries->setName(QStringLiteral("错误/超时"));
    m_historyReconnectSeries = new QLineSeries(m_historyChart);
    m_historyReconnectSeries->setName(QStringLiteral("重连"));
    for (QLineSeries *series : {m_historyTxSeries, m_historyRxSeries,
                                m_historyErrorSeries, m_historyReconnectSeries})
        m_historyChart->addSeries(series);

    m_historyAxisX = new QDateTimeAxis(m_historyChart);
    m_historyAxisX->setFormat(QStringLiteral("MM-dd HH:mm"));
    m_historyAxisX->setTitleText(QStringLiteral("时间"));
    m_historyChart->addAxis(m_historyAxisX, Qt::AlignBottom);
    m_historyAxisY = new QValueAxis(m_historyChart);
    m_historyAxisY->setTitleText(QStringLiteral("事件数量"));
    m_historyAxisY->setRange(0, 10);
    m_historyChart->addAxis(m_historyAxisY, Qt::AlignLeft);
    for (QLineSeries *series : {m_historyTxSeries, m_historyRxSeries,
                                m_historyErrorSeries, m_historyReconnectSeries}) {
        series->attachAxis(m_historyAxisX);
        series->attachAxis(m_historyAxisY);
    }

    auto *view = new QChartView(m_historyChart, group);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(280);
    layout->addWidget(view);
    m_historySummaryLabel = new QLabel(QStringLiteral("选择设备和时间范围后查询历史通信趋势。"), group);
    m_historySummaryLabel->setWordWrap(true);
    layout->addWidget(m_historySummaryLabel);

    if (auto *mainLayout = qobject_cast<QVBoxLayout *>(this->layout()))
        mainLayout->insertWidget(5, group);
    connect(refreshButton, &QPushButton::clicked,
            this, &TrendPage::refreshCommunicationHistoryTrend);
    connect(exportButton, &QPushButton::clicked,
            this, &TrendPage::exportCommunicationHistoryTrend);
    connect(m_historyRangeCombo, &QComboBox::currentIndexChanged,
            this, &TrendPage::refreshCommunicationHistoryTrend);
    refreshCommunicationHistoryTrend();
    m_historyRefreshTimer = new QTimer(this);
    m_historyRefreshTimer->setInterval(5000);
    connect(m_historyRefreshTimer, &QTimer::timeout,
            this, &TrendPage::refreshCommunicationHistoryTrend);
    m_historyRefreshTimer->start();
}

void TrendPage::refreshCommunicationHistoryTrend()
{
    if (!m_historySummaryLabel || !m_historyRangeCombo)
        return;
    if (!m_diagnostics || !m_diagnostics->isReady()) {
        m_historySummaryLabel->setText(QStringLiteral("通信诊断数据库不可用：%1")
            .arg(m_diagnostics ? m_diagnostics->lastError() : QStringLiteral("服务未初始化")));
        return;
    }
    const int hours = m_historyRangeCombo->currentData().toInt();
    const int bucketSeconds = hours <= 1 ? 10 : (hours <= 24 ? 60 : (hours <= 168 ? 300 : 1800));
    const QDateTime end = QDateTime::currentDateTime();
    const QDateTime start = end.addSecs(-hours * 3600);
    const QString deviceId = ui->deviceCombo->currentData().toString();
    const auto buckets = m_diagnostics->trendBuckets(deviceId, start, end, bucketSeconds);

    for (QLineSeries *series : {m_historyTxSeries, m_historyRxSeries,
                                m_historyErrorSeries, m_historyReconnectSeries})
        series->clear();

    qint64 tx = 0, rx = 0, txBytes = 0, rxBytes = 0, errors = 0, reconnects = 0;
    for (const CommunicationTrendBucket &bucket : buckets) {
        const qreal x = static_cast<qreal>(bucket.timestamp.toMSecsSinceEpoch());
        m_historyTxSeries->append(x, bucket.txFrames);
        m_historyRxSeries->append(x, bucket.rxFrames);
        m_historyErrorSeries->append(x, bucket.errors);
        m_historyReconnectSeries->append(x, bucket.reconnects);
        tx += bucket.txFrames;
        rx += bucket.rxFrames;
        txBytes += bucket.txBytes;
        rxBytes += bucket.rxBytes;
        errors += bucket.errors;
        reconnects += bucket.reconnects;
    }
    m_historyAxisX->setRange(start, end);
    const qint64 maxCount = qMax<qint64>(1, qMax<qint64>(qMax(tx, rx), qMax(errors, reconnects)));
    m_historyAxisY->setRange(0, qMax<qreal>(10.0, static_cast<qreal>(maxCount)));
    const QString scope = deviceId.isEmpty() ? QStringLiteral("全部设备") : deviceId;
    m_historySummaryLabel->setText(
        QStringLiteral("设备：%1 | 时间：%2 至 %3 | 时间桶：%4 秒 | TX：%5 帧 / %6 B | RX：%7 帧 / %8 B | 错误/超时：%9 | 重连：%10")
            .arg(scope, start.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 end.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            .arg(bucketSeconds).arg(tx).arg(txBytes).arg(rx).arg(rxBytes)
            .arg(errors).arg(reconnects));
    if (buckets.isEmpty())
        m_historySummaryLabel->setText(m_historySummaryLabel->text()
            + QStringLiteral("（当前时间范围没有诊断记录）"));
}

void TrendPage::exportCommunicationHistoryTrend()
{
    if (!m_diagnostics || !m_diagnostics->isReady()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
            m_diagnostics ? m_diagnostics->lastError() : QStringLiteral("通信诊断服务未初始化。"));
        return;
    }
    const int hours = m_historyRangeCombo->currentData().toInt();
    const int bucketSeconds = hours <= 1 ? 10 : (hours <= 24 ? 60 : (hours <= 168 ? 300 : 1800));
    const QDateTime end = QDateTime::currentDateTime();
    const QDateTime start = end.addSecs(-hours * 3600);
    const QString deviceId = ui->deviceCombo->currentData().toString();
    const auto buckets = m_diagnostics->trendBuckets(deviceId, start, end, bucketSeconds);
    if (buckets.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有数据"),
                                 QStringLiteral("当前设备和时间范围没有可导出的通信趋势数据。"));
        return;
    }
    const QString fileName = QStringLiteral("communication_trend_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出通信趋势"),
                                                       fileName, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    QByteArray output = QByteArray::fromHex("EFBBBF");
    output += QStringLiteral("时间桶,TX帧数,RX帧数,TX字节,RX字节,错误超时数,重连数\r\n").toUtf8();
    for (const CommunicationTrendBucket &bucket : buckets) {
        const QStringList columns = {
            bucket.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
            QString::number(bucket.txFrames), QString::number(bucket.rxFrames),
            QString::number(bucket.txBytes), QString::number(bucket.rxBytes),
            QString::number(bucket.errors), QString::number(bucket.reconnects)
        };
        output += columns.join(QLatin1Char(',')).toUtf8() + "\r\n";
    }
    if (file.write(output) != output.size()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("已导出 %1 个时间桶。").arg(buckets.size()));
}
