#include "demorxdelayqueue.h"

#include <QList>
#include <QTimer>
#include <QtGlobal>

DemoRxDelayQueue::DemoRxDelayQueue(int capacity, QObject *parent)
    : QObject(parent),
      m_capacity(qMax(1, capacity))
{
}

DemoRxDelayQueue::~DemoRxDelayQueue()
{
    cancelAll();
}

bool DemoRxDelayQueue::schedule(const QString &deviceId,
                                const QByteArray &data,
                                int delayMs)
{
    if (deviceId.trimmed().isEmpty() || delayMs <= 0)
        return false;

    if (m_pending.size() >= m_capacity) {
        ++m_queueFullCount;
        emit queueFull(deviceId);
        return false;
    }

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_pending.insert(timer, PendingFrame{deviceId, data, delayMs});
    m_peakPendingCount = qMax(m_peakPendingCount, m_pending.size());

    connect(timer, &QTimer::timeout, this, [this, timer]() {
        auto it = m_pending.find(timer);
        if (it == m_pending.end()) {
            timer->deleteLater();
            return;
        }

        const PendingFrame frame = it.value();
        m_pending.erase(it);
        ++m_deliveredCount;
        timer->deleteLater();
        emit frameReady(frame.deviceId, frame.data, frame.delayMs);
    });

    timer->start(delayMs);
    return true;
}

int DemoRxDelayQueue::cancelAll()
{
    const QList<QTimer *> timers = m_pending.keys();
    m_pending.clear();
    m_cancelledCount += static_cast<quint64>(timers.size());

    for (QTimer *timer : timers) {
        if (!timer)
            continue;
        timer->stop();
        timer->deleteLater();
    }

    return timers.size();
}

int DemoRxDelayQueue::pendingCount() const noexcept
{
    return m_pending.size();
}

int DemoRxDelayQueue::capacity() const noexcept
{
    return m_capacity;
}

int DemoRxDelayQueue::peakPendingCount() const noexcept
{
    return m_peakPendingCount;
}

quint64 DemoRxDelayQueue::deliveredCount() const noexcept
{
    return m_deliveredCount;
}

quint64 DemoRxDelayQueue::cancelledCount() const noexcept
{
    return m_cancelledCount;
}

quint64 DemoRxDelayQueue::queueFullCount() const noexcept
{
    return m_queueFullCount;
}
