#include "TrendPage.h"

#include "src/communication/communicationmanager.h"

#include <QChart>
#include <QChartView>
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
#include <QtMath>
#include <QVBoxLayout>

TrendPage::TrendPage(CommunicationManager *communicationManager, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::TrendPage),
      m_communicationManager(communicationManager)
{
    ui->setupUi(this);

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

void TrendPage::updateTrend()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
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
