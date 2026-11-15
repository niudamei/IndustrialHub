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

    std::cout << "ALL MONITOR STATISTICS TESTS PASSED." << std::endl;
    return 0;
}
