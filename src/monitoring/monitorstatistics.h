#ifndef MONITORSTATISTICS_H
#define MONITORSTATISTICS_H

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <QQueue>

namespace MonitorStatistics {

// These helpers require timestamps from one monotonic clock. The retention
// window bounds recent frame history; the shorter window supplies the live FPS.
inline void pruneMonotonicTimestamps(QQueue<qint64> &timestamps,
                                     qint64 nowMs,
                                     qint64 retentionMs = 15000) noexcept
{
    if (nowMs <= 0 || retentionMs < 0)
        return;

    while (!timestamps.isEmpty()
           && timestamps.head() <= nowMs
           && nowMs - timestamps.head() > retentionMs) {
        timestamps.dequeue();
    }
}

inline int countMonotonicRecent(const QQueue<qint64> &timestamps,
                                qint64 nowMs,
                                qint64 windowMs = 1000) noexcept
{
    if (nowMs <= 0 || windowMs < 0)
        return 0;

    int count = 0;
    for (qint64 timestamp : timestamps) {
        // A future timestamp is invalid for a monotonic elapsed-time window.
        if (timestamp > 0 && timestamp <= nowMs
            && nowMs - timestamp <= windowMs) {
            ++count;
        }
    }
    return count;
}

class IntervalJitter
{
public:
    void add(std::int64_t intervalMs) noexcept
    {
        if (intervalMs < 0)
            return;

        ++m_sampleCount;
        const double value = static_cast<double>(intervalMs);
        const double delta = value - m_meanMs;
        m_meanMs += delta / static_cast<double>(m_sampleCount);
        const double deltaAfterUpdate = value - m_meanMs;
        m_squaredDeltaSum += delta * deltaAfterUpdate;
    }

    void reset() noexcept
    {
        m_sampleCount = 0;
        m_meanMs = 0.0;
        m_squaredDeltaSum = 0.0;
    }

    std::uint64_t sampleCount() const noexcept
    {
        return m_sampleCount;
    }

    double meanMs() const noexcept
    {
        return m_meanMs;
    }

    bool hasStandardDeviation() const noexcept
    {
        return m_sampleCount >= 2;
    }

    // Sample standard deviation of inter-frame intervals, in milliseconds.
    double standardDeviationMs() const noexcept
    {
        if (!hasStandardDeviation())
            return 0.0;

        const double variance =
            m_squaredDeltaSum / static_cast<double>(m_sampleCount - 1);
        return std::sqrt(std::max(0.0, variance));
    }

private:
    std::uint64_t m_sampleCount = 0;
    double m_meanMs = 0.0;
    double m_squaredDeltaSum = 0.0;
};

class OnlineAvailability
{
public:
    // Starts a new connection-session observation window. All nowMs values must
    // come from one monotonic clock (for example QElapsedTimer), not wall time.
    void begin(std::int64_t nowMs) noexcept
    {
        m_started = true;
        m_finished = false;
        m_online = false;
        m_startMs = nowMs;
        m_endMs = nowMs;
        m_onlineSinceMs = 0;
        m_accumulatedOnlineMs = 0;
    }

    void setOnline(bool online, std::int64_t nowMs) noexcept
    {
        if (!m_started || m_finished || m_online == online)
            return;

        const std::int64_t effectiveNow = std::max(nowMs, m_startMs);
        if (online) {
            m_onlineSinceMs = effectiveNow;
            m_online = true;
            return;
        }

        m_accumulatedOnlineMs += std::max<std::int64_t>(
            0, effectiveNow - m_onlineSinceMs);
        m_onlineSinceMs = 0;
        m_online = false;
    }

    void finish(std::int64_t nowMs) noexcept
    {
        if (!m_started || m_finished)
            return;

        const std::int64_t effectiveNow = std::max(nowMs, m_startMs);
        setOnline(false, effectiveNow);
        m_endMs = effectiveNow;
        m_finished = true;
    }

    bool hasObservation() const noexcept
    {
        return m_started;
    }

    double ratePercent(std::int64_t nowMs) const noexcept
    {
        if (!m_started)
            return -1.0;

        const std::int64_t endMs =
            m_finished ? m_endMs : std::max(nowMs, m_startMs);
        const std::int64_t durationMs = endMs - m_startMs;
        if (durationMs <= 0)
            return 0.0;

        std::int64_t onlineMs = m_accumulatedOnlineMs;
        if (m_online)
            onlineMs += std::max<std::int64_t>(0, endMs - m_onlineSinceMs);
        onlineMs = std::min(onlineMs, durationMs);

        return 100.0 * static_cast<double>(onlineMs)
            / static_cast<double>(durationMs);
    }

private:
    bool m_started = false;
    bool m_finished = false;
    bool m_online = false;
    std::int64_t m_startMs = 0;
    std::int64_t m_endMs = 0;
    std::int64_t m_onlineSinceMs = 0;
    std::int64_t m_accumulatedOnlineMs = 0;
};


struct TrafficCounters
{
    std::uint64_t txFrames = 0;
    std::uint64_t rxFrames = 0;
    std::uint64_t errors = 0;

    void add(std::uint64_t tx, std::uint64_t rx, std::uint64_t errorCount) noexcept
    {
        txFrames += tx;
        rxFrames += rx;
        errors += errorCount;
    }
};

struct TrafficSourceTotals
{
    TrafficCounters driver;
    TrafficCounters demo;

    void add(bool isDemo,
             std::uint64_t tx,
             std::uint64_t rx,
             std::uint64_t errorCount) noexcept
    {
        (isDemo ? demo : driver).add(tx, rx, errorCount);
    }
};

} // namespace MonitorStatistics

#endif // MONITORSTATISTICS_H
