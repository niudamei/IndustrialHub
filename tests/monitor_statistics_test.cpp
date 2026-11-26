#include "src/monitoring/monitorstatistics.h"

#include <cmath>
#include <iostream>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << std::endl; \
            return 1; \
        } \
    } while (false)

bool near(double actual, double expected, double tolerance = 0.001)
{
    return std::abs(actual - expected) <= tolerance;
}

int main()
{
    MonitorStatistics::IntervalJitter jitter;
    CHECK(jitter.sampleCount() == 0, "new jitter accumulator is empty");
    CHECK(!jitter.hasStandardDeviation(), "standard deviation needs two intervals");

    jitter.add(100);
    jitter.add(110);
    jitter.add(90);
    CHECK(jitter.sampleCount() == 3, "all valid intervals are counted");
    CHECK(near(jitter.meanMs(), 100.0), "Welford mean is correct");
    CHECK(near(jitter.standardDeviationMs(), 10.0),
          "sample standard deviation is correct for 100, 110, 90 ms");

    jitter.add(-1);
    CHECK(jitter.sampleCount() == 3, "negative intervals are ignored");

    jitter.reset();
    CHECK(jitter.sampleCount() == 0, "reset clears interval samples");
    jitter.add(1000000001LL);
    jitter.add(1000000002LL);
    jitter.add(1000000003LL);
    CHECK(near(jitter.standardDeviationMs(), 1.0),
          "online variance remains stable for large interval values");

    QQueue<qint64> recentTraffic;
    recentTraffic.enqueue(1000);
    recentTraffic.enqueue(1500);
    recentTraffic.enqueue(2000);
    recentTraffic.enqueue(2500);
    recentTraffic.enqueue(3000);
    CHECK(MonitorStatistics::countMonotonicRecent(recentTraffic, 2000) == 3,
          "one-second traffic window includes the inclusive boundary");
    CHECK(MonitorStatistics::countMonotonicRecent(recentTraffic, 1900) == 2,
          "future monotonic timestamps are not counted as recent traffic");
    CHECK(MonitorStatistics::countMonotonicRecent(recentTraffic, 3000, 500) == 2,
          "custom traffic rate windows are respected");
    MonitorStatistics::pruneMonotonicTimestamps(recentTraffic, 17001, 15000);
    CHECK(recentTraffic.size() == 2
              && recentTraffic.head() == 2500
              && recentTraffic.last() == 3000,
          "fifteen-second retention prunes only expired monotonic timestamps");

    QQueue<qint64> untouchedFutureTraffic;
    untouchedFutureTraffic.enqueue(5000);
    MonitorStatistics::pruneMonotonicTimestamps(untouchedFutureTraffic, 4000, 15000);
    CHECK(untouchedFutureTraffic.size() == 1,
          "pruning does not discard timestamps later than the supplied monotonic now");

    MonitorStatistics::OnlineAvailability availability;
    CHECK(!availability.hasObservation(), "availability is unavailable before connection");
    CHECK(availability.ratePercent(1500) < 0.0,
          "unobserved availability uses a negative sentinel");

    availability.begin(1000);
    CHECK(near(availability.ratePercent(1100), 0.0),
          "connected time before first RX is not counted online");
    availability.setOnline(true, 1100);
    CHECK(near(availability.ratePercent(1200), 50.0),
          "active receive time contributes to the current session rate");
    availability.setOnline(false, 1300);
    CHECK(near(availability.ratePercent(1500), 40.0),
          "offline silence is included in the denominator");
    availability.setOnline(true, 1400);
    CHECK(near(availability.ratePercent(1500), 60.0),
          "recovery starts a new online interval without losing prior uptime");
    availability.finish(1500);
    CHECK(near(availability.ratePercent(2000), 60.0),
          "finished session rate remains stable after disconnect");

    availability.begin(2000);
    CHECK(near(availability.ratePercent(2100), 0.0),
          "a reconnect starts a fresh connection-session window");
    availability.setOnline(true, 2050);
    availability.finish(2100);
    CHECK(near(availability.ratePercent(2200), 50.0),
          "disconnect closes an active online interval");

    MonitorStatistics::TrafficSourceTotals trafficTotals;
    trafficTotals.add(false, 10, 20, 1);
    trafficTotals.add(true, 3, 4, 2);
    trafficTotals.add(false, 2, 1, 0);
    CHECK(trafficTotals.driver.txFrames == 12
              && trafficTotals.driver.rxFrames == 21
              && trafficTotals.driver.errors == 1,
          "driver traffic totals aggregate independently");
    CHECK(trafficTotals.demo.txFrames == 3
              && trafficTotals.demo.rxFrames == 4
              && trafficTotals.demo.errors == 2,
          "demo traffic totals never mix with driver counters");

    std::cout << "ALL MONITOR STATISTICS TESTS PASSED." << std::endl;
    return 0;
}
