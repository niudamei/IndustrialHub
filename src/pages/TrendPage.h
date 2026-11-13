#ifndef TRENDPAGE_H
#define TRENDPAGE_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QWidget>

#include "ui_trendpage.h"

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
    void onConnectionStateChanged(const QString &deviceId, bool connected);
    void onCommunicationError(const QString &deviceId, const QString &message);
    void updateTrend();

private:
    struct DeviceTraffic
    {
        QQueue<qint64> txTimes;
        QQueue<qint64> rxTimes;
        quint64 txFrames = 0;
        quint64 rxFrames = 0;
        quint64 txBytes = 0;
        quint64 rxBytes = 0;
        quint64 errors = 0;
        bool connected = false;
        qint64 lastRxMs = 0;
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
