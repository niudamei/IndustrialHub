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
        emit queueFull(deviceId);
        return false;
    }

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    m_pending.insert(timer, PendingFrame{deviceId, data, delayMs});

    connect(timer, &QTimer::timeout, this, [this, timer]() {
        auto it = m_pending.find(timer);
        if (it == m_pending.end()) {
            timer->deleteLater();
            return;
        }

        const PendingFrame frame = it.value();
        m_pending.erase(it);
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
