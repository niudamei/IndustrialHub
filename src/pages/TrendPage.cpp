#include "TrendPage.h"

#include "src/communication/communicationmanager.h"
#include "src/core/businessdatamanager.h"
#include "src/protocol/businessdatapoint.h"

#include <QChart>
#include <QChartView>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <limits>
#include <QTabWidget>
#include <QLabel>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QPainter>
#include <QSignalBlocker>
#include <QTimer>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QValueAxis>
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
    ui->setupUi(this);
    setupBusinessTrend();

    auto *chart = new QChart();
    chart->setTitle(QStringLiteral("发送 / 接收帧率（最近 15 秒）"));
    chart->legend()->setVisible(true);

    m_txSeries = new QLineSeries(chart);
    m_txSeries->setName(QStringLiteral("TX 发送帧率"));
    chart->addSeries(m_txSeries);

    m_rxSeries = new QLineSeries(chart);
    m_rxSeries->setName(QStringLiteral("RX 接收帧率"));
    chart->addSeries(m_rxSeries);

    m_axisX = new QValueAxis(chart);
    m_axisX->setTitleText(QStringLiteral("时间（秒）"));
    m_axisX->setRange(0.0, 15.0);
    m_axisX->setLabelFormat("%.1f");
    chart->addAxis(m_axisX, Qt::AlignBottom);
    m_txSeries->attachAxis(m_axisX);
    m_rxSeries->attachAxis(m_axisX);

    m_axisY = new QValueAxis(chart);
    m_axisY->setTitleText(QStringLiteral("帧 / 秒（FPS）"));
    m_axisY->setRange(0.0, 10.0);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_txSeries->attachAxis(m_axisY);
    m_rxSeries->attachAxis(m_axisY);

    m_chartView = new QChartView(chart, ui->chartHost);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    auto *chartLayout = new QVBoxLayout(ui->chartHost);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_chartView);

    ui->analysisTable->setColumnCount(9);
    ui->analysisTable->setHorizontalHeaderLabels({
        QStringLiteral("设备编号"), QStringLiteral("连接状态"),
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
        m_sampleIndex = 0;
        updateTrend();
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

    if (m_businessDataManager) {
        connect(m_businessDataManager, &BusinessDataManager::valuesUpdated,
                this, &TrendPage::onBusinessValuesUpdated);
    }

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

void TrendPage::onBusinessValuesUpdated(const QString &, const QVector<BusinessDataPoint> &)
{
    // 数据缓存由 BusinessDataManager 维护；定时器按最新时间戳采样，避免重复点。
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
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    DeviceTraffic &traffic = m_traffic[id];
    (isTx ? traffic.txTimes : traffic.rxTimes).enqueue(nowMs);
    if (isTx) {
        ++traffic.txFrames;
    } else {
        ++traffic.rxFrames;
        traffic.lastRxMs = nowMs;
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
    ids.sort(Qt::CaseInsensitive);
    for (const QString &id : ids)
        ui->deviceCombo->addItem(id, id);

    const int index = ui->deviceCombo->findData(selectedId);
    if (index >= 0)
        ui->deviceCombo->setCurrentIndex(index);
}

int TrendPage::countRecent(const QQueue<qint64> &times, qint64 nowMs) const
{
    int count = 0;
    for (qint64 timestamp : times) {
        if (nowMs - timestamp <= 1000)
            ++count;
    }
    return count;
}

 
void TrendPage::setupBusinessTrend()
{
    auto *group = new QGroupBox(QStringLiteral("业务变量实时趋势（最近 60 秒）"), this);
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
        m_businessSeries.append({});
        auto *stats = new QLabel(QStringLiteral("当前值：--    最大值：--    最小值：--    平均值：--"), group);
        stats->setWordWrap(true);
        m_businessStats.append(stats);
    }

    layout->addWidget(m_businessTabs);
    for (QLabel *stats : m_businessStats)
        layout->addWidget(stats);

    if (auto *mainLayout = qobject_cast<QVBoxLayout *>(this->layout()))
        mainLayout->insertWidget(4, group);
}

void TrendPage::updateBusinessTrend(qint64 nowMs)
{
    if (!m_businessDataManager)
        return;

    const QString selectedId = ui->deviceCombo->currentData().toString();
    const QList<BusinessDataPoint> points =
        m_businessDataManager->latestValues(selectedId);
    const QStringList variableIds = {
        QStringLiteral("temperature"), QStringLiteral("rpm"), QStringLiteral("current")
    };
    const qreal nowValue = static_cast<qreal>(nowMs);
    const qreal leftValue = nowValue - 60000.0;

    for (const BusinessDataPoint &point : points) {
        if (point.quality != DataQuality::Good || !point.timestamp.isValid())
            continue;
        const int index = variableIds.indexOf(point.variableId.toCaseFolded());
        if (index < 0)
            continue;

        const QString seriesKey = point.deviceId + QLatin1Char('|') + point.variableId;
        const qint64 timestamp = point.timestamp.toMSecsSinceEpoch();
        if (m_lastBusinessTimestamp.value(seriesKey, -1) == timestamp)
            continue;

        QLineSeries *series = m_businessSeries[index].value(point.deviceId);
        if (!series) {
            series = new QLineSeries(m_businessCharts[index]);
            series->setName(point.deviceId);
            m_businessCharts[index]->addSeries(series);
            series->attachAxis(m_businessCharts[index]->axes(Qt::Horizontal).first());
            series->attachAxis(m_businessAxisY[index]);
            m_businessSeries[index].insert(point.deviceId, series);
        }
        series->append(static_cast<qreal>(timestamp), point.value);
        m_lastBusinessTimestamp.insert(seriesKey, timestamp);
    }

    for (int i = 0; i < m_businessCharts.size(); ++i) {
        qreal minValue = std::numeric_limits<qreal>::max();
        qreal maxValue = std::numeric_limits<qreal>::lowest();
        qreal sum = 0.0;
        int count = 0;
        qreal currentValue = 0.0;
        bool hasCurrent = false;

        for (auto it = m_businessSeries[i].begin(); it != m_businessSeries[i].end(); ++it) {
            QLineSeries *series = it.value();
            QVector<QPointF> kept;
            const auto oldPoints = series->points();
            for (const QPointF &sample : oldPoints) {
                if (sample.x() >= leftValue) {
                    kept.append(sample);
                    minValue = qMin(minValue, sample.y());
                    maxValue = qMax(maxValue, sample.y());
                    sum += sample.y();
                    ++count;
                    currentValue = sample.y();
                    hasCurrent = true;
                }
            }
            if (kept.size() != oldPoints.size())
                series->replace(kept);
        }

        auto *axisX = qobject_cast<QDateTimeAxis *>(m_businessCharts[i]->axes(Qt::Horizontal).first());
        axisX->setRange(QDateTime::fromMSecsSinceEpoch(nowMs - 60000),
                        QDateTime::fromMSecsSinceEpoch(nowMs));
        if (count > 0) {
            const qreal margin = qMax<qreal>(1.0, (maxValue - minValue) * 0.15);
            m_businessAxisY[i]->setRange(minValue - margin, maxValue + margin);
            m_businessStats[i]->setText(
                QStringLiteral("当前值：%1    最大值：%2    最小值：%3    平均值：%4")
                    .arg(currentValue, 0, 'f', 2)
                    .arg(maxValue, 0, 'f', 2)
                    .arg(minValue, 0, 'f', 2)
                    .arg(sum / count, 0, 'f', 2));
        } else {
            m_businessAxisY[i]->setRange(0.0, 100.0);
            m_businessStats[i]->setText(QStringLiteral("暂无有效数据（请在实时监控中启动模拟设备）"));
        }
    }
}

void TrendPage::updateTrend()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    updateBusinessTrend(nowMs);
    const QString selectedId = ui->deviceCombo->currentData().toString();
    int txRate = 0;
    int rxRate = 0;

    for (auto it = m_traffic.begin(); it != m_traffic.end(); ++it) {
        auto prune = [nowMs](QQueue<qint64> &queue) {
            while (!queue.isEmpty() && nowMs - queue.head() > 15000)
                queue.dequeue();
        };
        prune(it->txTimes);
        prune(it->rxTimes);
        if (selectedId.isEmpty() || it.key().compare(selectedId, Qt::CaseInsensitive) == 0) {
            txRate += countRecent(it->txTimes, nowMs);
            rxRate += countRecent(it->rxTimes, nowMs);
        }
    }

    const qreal x = static_cast<qreal>(m_sampleIndex++) * 0.25;
    m_txSeries->append(x, txRate);
    m_rxSeries->append(x, rxRate);
    while (m_txSeries->count() > 61)
        m_txSeries->remove(0);
    while (m_rxSeries->count() > 61)
        m_rxSeries->remove(0);

    m_axisX->setRange(qMax<qreal>(0.0, x - 15.0), qMax<qreal>(15.0, x));
    const qreal maxRate = qMax<qreal>(10.0, qMax<qreal>(txRate, rxRate));
    m_axisY->setRange(0.0, qMax<qreal>(10.0, qCeil(maxRate / 5.0) * 5.0));

    ui->txRateLabel->setText(QStringLiteral("当前 TX：%1 帧/秒").arg(txRate));
    ui->rxRateLabel->setText(QStringLiteral("当前 RX：%1 帧/秒").arg(rxRate));
    ui->trendHintLabel->setText(selectedId.isEmpty()
        ? QStringLiteral("当前统计范围：全部设备")
        : QStringLiteral("当前统计设备：%1").arg(selectedId));

    QStringList deviceIds = m_traffic.keys();
    deviceIds.sort(Qt::CaseInsensitive);
    QList<QString> visibleIds;
    for (const QString &id : deviceIds) {
        if (selectedId.isEmpty()
            || id.compare(selectedId, Qt::CaseInsensitive) == 0)
            visibleIds.append(id);
    }

    ui->analysisTable->setRowCount(visibleIds.size());
    quint64 totalTxFrames = 0;
    quint64 totalRxFrames = 0;
    quint64 totalErrors = 0;
    quint64 totalTxBytes = 0;
    quint64 totalRxBytes = 0;
    int row = 0;
    for (const QString &id : visibleIds) {
        const DeviceTraffic &traffic = m_traffic[id];
        totalTxFrames += traffic.txFrames;
        totalRxFrames += traffic.rxFrames;
        totalErrors += traffic.errors;
        totalTxBytes += traffic.txBytes;
        totalRxBytes += traffic.rxBytes;
        const QString lastRx = traffic.lastRxMs > 0
            ? QDateTime::fromMSecsSinceEpoch(traffic.lastRxMs)
                  .toString(QStringLiteral("HH:mm:ss.zzz"))
            : QStringLiteral("--");
        const QString rxAge = traffic.lastRxMs > 0
            ? QStringLiteral("%1 ms").arg(nowMs - traffic.lastRxMs)
            : QStringLiteral("--");
        const QStringList values = {
            id,
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
        QStringLiteral("累计 TX %1 帧 / %2 B    RX %3 帧 / %4 B    错误 %5")
            .arg(totalTxFrames).arg(totalTxBytes)
            .arg(totalRxFrames).arg(totalRxBytes).arg(totalErrors));
}
