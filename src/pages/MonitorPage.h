#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QDateTime>
#include <QHash>
#include <QQueue>
#include <QVector>
#include <QWidget>

#include "ui_monitorpage.h"

class DeviceManager;
class CommunicationManager;
class QTimer;

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(DeviceManager *deviceManager,
                         CommunicationManager *communicationManager,
                         QWidget *parent = nullptr);
    ~MonitorPage() override;

private slots:
    void refreshDeviceFilter();
    void applyFilters();
    void onConnectionStateChanged(const QString &deviceId, bool connected);
    void onDataSent(const QString &deviceId, const QByteArray &data);
    void onDataReceived(const QString &deviceId, const QByteArray &data);
    void onCommunicationError(const QString &deviceId, const QString &message);
    void onPauseClicked();
    void exportEventsToCsv();
    void toggleDemoMode();
    void generateDemoTraffic();
    void onReceiveTimeoutChanged(int seconds);
    void updateLiveStatistics();

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
        qint64 intervalSumMs = 0;
        qint64 intervalCount = 0;
        qint64 minIntervalMs = -1;
        qint64 maxIntervalMs = 0;
        quint64 txCount = 0;
        quint64 rxCount = 0;
        quint64 errorCount = 0;
    };

    void appendEvent(const QString &deviceId,
                     const QString &direction,
                     const QString &content);
    QString formatData(const QString &deviceId, const QByteArray &data) const;
    void refreshMetricsTable();
    QString receiveStateText(const DeviceMetrics &metrics, qint64 nowMs) const;
    double receiveRate(const DeviceMetrics &metrics, qint64 nowMs) const;

    Ui::MonitorPage *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    QTimer *m_statisticsTimer = nullptr;
    QTimer *m_demoTimer = nullptr;
    QHash<QString, DeviceMetrics> m_deviceMetrics;
    bool m_paused = false;
    bool m_demoMode = false;
    quint64 m_demoSequence = 0;
    int m_demoDeviceIndex = 0;
    static constexpr int MaxRows = 500;
    int m_receiveTimeoutMs = 3000;
};

#endif // MONITORPAGE_H
