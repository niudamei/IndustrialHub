#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QDateTime>
#include <QElapsedTimer>
#include "src/protocol/businessdatapoint.h"
#include "src/monitoring/monitorstatistics.h"
#include "src/simulation/demofaultpolicy.h"
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "ui_monitorpage.h"

class DeviceManager;
class CommunicationManager;
class DeviceHealthMonitor;
class DemoRxDelayQueue;
class QTimer;
class QCheckBox;

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(DeviceManager *deviceManager,
                         CommunicationManager *communicationManager,
                         DeviceHealthMonitor *healthMonitor = nullptr,
                         QWidget *parent = nullptr);
    ~MonitorPage() override;

private slots:
    void refreshDeviceFilter();
    void applyFilters();
    void onConnectionStateChanged(const QString &deviceId, bool connected);
    void onDataSent(const QString &deviceId, const QByteArray &data);
    void onDataReceived(const QString &deviceId, const QByteArray &data);
    void processDataReceived(const QString &deviceId, const QByteArray &data,
                             int simulatedDelayMs = 0);
    void onCommunicationError(const QString &deviceId, const QString &message);
    void onPauseClicked();
    void exportEventsToCsv();
    void toggleDemoMode();
    void onDemoIntervalChanged(int milliseconds);
    void applyDemoFaultSettings();
    void generateDemoTraffic();
    void onReceiveTimeoutChanged(int seconds);
    void updateLiveStatistics();

signals:
    void demoTrafficGenerated(const QString &deviceId, bool isTx, qint64 byteCount);
    void demoDeviceConnectionChanged(const QString &deviceId, bool connected);
    void demoBusinessDataGenerated(const QString &deviceId,
                                   const QVector<BusinessDataPoint> &points);

private:
    struct DeviceMetrics
    {
        QString deviceName;
        QString commType;
        bool connected = false;
        qint64 connectedSinceMs = 0;
        QDateTime lastReceiveTime;
        qint64 lastReceiveMs = 0;
        QQueue<qint64> receiveTimestamps;
        bool receiveTimedOut = false;
        quint64 timeoutCount = 0;
        quint64 recoveryCount = 0;
        qint64 lastIntervalMs = -1;
        MonitorStatistics::IntervalJitter intervalJitter;
        MonitorStatistics::OnlineAvailability availability;
        qint64 minIntervalMs = -1;
        qint64 maxIntervalMs = 0;
        quint64 txCount = 0;
        quint64 rxCount = 0;
        quint64 errorCount = 0;
    };

    qint64 monotonicNowMs() const;
    void appendEvent(const QString &deviceId,
                     const QString &direction,
                     const QString &content);
    QString formatData(const QString &deviceId, const QByteArray &data) const;
    void refreshMetricsTable();
    QString receiveStateText(const DeviceMetrics &metrics, qint64 nowMs) const;
    double receiveRate(const DeviceMetrics &metrics, qint64 nowMs) const;
    bool isDemoRuntimeDevice(const QString &deviceId) const;
    QString dataSourceLabel(const QString &deviceId) const;

    Ui::MonitorPage *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    DeviceHealthMonitor *m_healthMonitor = nullptr;
    QElapsedTimer m_monotonicClock;
    QTimer *m_statisticsTimer = nullptr;
    QTimer *m_demoTimer = nullptr;
    DemoRxDelayQueue *m_demoRxDelayQueue = nullptr;
    QHash<QString, DeviceMetrics> m_deviceMetrics;
    QSet<QString> m_demoDeviceIds;
    bool m_paused = false;
    bool m_demoMode = false;
    quint64 m_demoSequence = 0;
    QHash<QString, quint64> m_demoFrameCounters;
    DemoFaultPolicy::InjectedDropCounters m_demoInjectedDropCounters;
    QSet<QString> m_demoDisconnectedDevices;
    int m_demoDropEvery = 3;
    QCheckBox *m_demoOverTemperatureCheckBox = nullptr;
    static constexpr int MaxRows = 500;
    int m_receiveTimeoutMs = 3000;
};

#endif // MONITORPAGE_H
