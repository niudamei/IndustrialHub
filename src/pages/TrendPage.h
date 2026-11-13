#ifndef TRENDPAGE_H
#define TRENDPAGE_H

#include <QHash>
#include <QQueue>
#include <QWidget>

#include "ui_TrendPage.h"

class CommunicationManager;
class QChartView;
class QLineSeries;
class QValueAxis;
class QTimer;

class TrendPage : public QWidget
{
    Q_OBJECT

public:
    explicit TrendPage(CommunicationManager *communicationManager,
                       QWidget *parent = nullptr);
    ~TrendPage() override;

private slots:
    void onDataSent(const QString &deviceId, const QByteArray &data);
    void onDataReceived(const QString &deviceId, const QByteArray &data);
    void updateTrend();

private:
    struct DeviceTraffic
    {
        QQueue<qint64> txTimes;
        QQueue<qint64> rxTimes;
    };

    void recordTraffic(const QString &deviceId, bool isTx);
    void refreshDeviceList();
    int countRecent(const QQueue<qint64> &times, qint64 nowMs) const;

    Ui::TrendPage *ui = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    QTimer *m_timer = nullptr;
    QChartView *m_chartView = nullptr;
    QLineSeries *m_txSeries = nullptr;
    QLineSeries *m_rxSeries = nullptr;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    QHash<QString, DeviceTraffic> m_traffic;
    quint64 m_sampleIndex = 0;
};

#endif // TRENDPAGE_H
