#include "devicehealthmonitor.h"

#include <QDateTime>
#include <QStringList>
#include <QtGlobal>

DeviceHealthMonitor::DeviceHealthMonitor(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout,
            this, &DeviceHealthMonitor::checkTimeouts);
    m_timer.start();
}

void DeviceHealthMonitor::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = qBound(250, timeoutMs, 3600000);
    checkTimeouts();
}

int DeviceHealthMonitor::timeoutMs() const
{
    return m_timeoutMs;
}

void DeviceHealthMonitor::registerDevice(const QString &deviceId)
{
    const QString id = deviceId.trimmed();
    const QString key = keyFor(id);
    if (key.isEmpty())
        return;
    if (m_states.contains(key))
        return;
    m_deviceIds.insert(key, id);
    m_states.insert(key, DeviceState{});
}

void DeviceHealthMonitor::removeDevice(const QString &deviceId)
{
    const QString key = keyFor(deviceId);
    m_states.remove(key);
    m_deviceIds.remove(key);
}

void DeviceHealthMonitor::setConnected(const QString &deviceId, bool connected)
{
    const QString id = deviceId.trimmed();
    const QString key = keyFor(id);
    if (key.isEmpty())
        return;
    if (!m_states.contains(key))
        registerDevice(id);

    DeviceState &state = m_states[key];
    if (state.connected == connected)
        return;

    state.connected = connected;
    if (connected) {
        state.connectedAtMs = QDateTime::currentMSecsSinceEpoch();
        state.lastSeenMs = 0;
        state.lastSeenTime = QDateTime();
        state.timeoutReported = false;
    } else {
        state.connectedAtMs = 0;
        state.lastSeenMs = 0;
        state.lastSeenTime = QDateTime();
        state.timeoutReported = false;
        setOnline(key, false);
    }
}

void DeviceHealthMonitor::markActivity(const QString &deviceId)
{
    const QString id = deviceId.trimmed();
    const QString key = keyFor(id);
    if (key.isEmpty())
        return;
    if (!m_states.contains(key))
        registerDevice(id);

    DeviceState &state = m_states[key];
    if (!state.connected)
        return;

    state.lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    state.lastSeenTime = QDateTime::currentDateTime();
    state.timeoutReported = false;
    setOnline(key, true);
}

bool DeviceHealthMonitor::isOnline(const QString &deviceId) const
{
    return m_states.value(keyFor(deviceId)).online;
}

QDateTime DeviceHealthMonitor::lastSeen(const QString &deviceId) const
{
    return m_states.value(keyFor(deviceId)).lastSeenTime;
}

QStringList DeviceHealthMonitor::deviceIds() const
{
    QStringList result = m_deviceIds.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

void DeviceHealthMonitor::checkTimeouts()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_states.begin(); it != m_states.end(); ++it) {
        DeviceState &state = it.value();
        if (!state.connected)
            continue;

        const qint64 referenceMs = state.lastSeenMs > 0
            ? state.lastSeenMs : state.connectedAtMs;
        if (referenceMs <= 0 || now - referenceMs <= m_timeoutMs
            || state.timeoutReported)
            continue;

        const qint64 silenceMs = now - referenceMs;
        state.timeoutReported = true;
        if (state.online)
            setOnline(it.key(), false);
        emit deviceTimedOut(m_deviceIds.value(it.key()), silenceMs);
    }
}

QString DeviceHealthMonitor::keyFor(const QString &deviceId)
{
    return deviceId.trimmed().toCaseFolded();
}

void DeviceHealthMonitor::setOnline(const QString &key, bool online)
{
    DeviceState &state = m_states[key];
    if (state.online == online)
        return;
    state.online = online;
    emit deviceOnlineChanged(m_deviceIds.value(key), online);
}
