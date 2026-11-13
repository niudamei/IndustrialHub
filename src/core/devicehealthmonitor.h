#ifndef DEVICEHEALTHMONITOR_H
#define DEVICEHEALTHMONITOR_H

#include <QObject>
#include <QHash>
#include <QDateTime>
#include <QStringList>
#include <QTimer>

class DeviceHealthMonitor : public QObject
{
    Q_OBJECT
public:
    explicit DeviceHealthMonitor(QObject *parent = nullptr);

    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const;
    void registerDevice(const QString &deviceId);
    void removeDevice(const QString &deviceId);
    void setConnected(const QString &deviceId, bool connected);
    void markActivity(const QString &deviceId);
    bool isOnline(const QString &deviceId) const;
    QDateTime lastSeen(const QString &deviceId) const;
    QStringList deviceIds() const;

signals:
    void deviceOnlineChanged(const QString &deviceId, bool online);
    void deviceTimedOut(const QString &deviceId, qint64 silenceMs);

private slots:
    void checkTimeouts();

private:
    struct DeviceState {
        bool connected = false;
        bool online = false;
        qint64 connectedAtMs = 0;
        qint64 lastSeenMs = 0;
        QDateTime lastSeenTime;
    };

    static QString keyFor(const QString &deviceId);
    void setOnline(const QString &key, bool online);

    QHash<QString, DeviceState> m_states;
    QHash<QString, QString> m_deviceIds;
    QTimer m_timer;
    int m_timeoutMs = 3000;
};

#endif // DEVICEHEALTHMONITOR_H
