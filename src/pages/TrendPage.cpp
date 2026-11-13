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
    connect(ui->deviceCombo, &QComboBox::currentIndexChanged,
            this, &TrendPage::updateTrend);

    if (m_communicationManager) {
        connect(m_communicationManager, &CommunicationManager::deviceDataSent,
                this, &TrendPage::onDataSent);
        connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
                this, &TrendPage::onDataReceived);
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

void TrendPage::onDataSent(const QString &deviceId, const QByteArray &)
{
    recordTraffic(deviceId, true);
}

void TrendPage::onDataReceived(const QString &deviceId, const QByteArray &)
{
    recordTraffic(deviceId, false);
}

void TrendPage::recordTraffic(const QString &deviceId, bool isTx)
{
    const QString id = deviceId.trimmed();
    if (id.isEmpty())
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    DeviceTraffic &traffic = m_traffic[id];
    (isTx ? traffic.txTimes : traffic.rxTimes).enqueue(nowMs);
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
}
