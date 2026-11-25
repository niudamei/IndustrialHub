#include "src/monitoring/trendhistorybuffer.h"

#include <QCoreApplication>
#include <QDebug>

#define CHECK(condition) do { \
    if (!(condition)) { \
        qCritical() << "CHECK failed:" << #condition << "at line" << __LINE__; \
        return 1; \
    } \
} while (false)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QLineSeries businessSeries;
    for (int i = 0; i < TrendHistoryBuffer::MaxBusinessPointsPerSeries + 50; ++i) {
        CHECK(TrendHistoryBuffer::appendBounded(
                  &businessSeries, static_cast<qreal>(i), static_cast<qreal>(i)),
              "default bounded append accepts valid series and capacity");
    }

    CHECK(businessSeries.count() == TrendHistoryBuffer::MaxBusinessPointsPerSeries,
          "business trend history never exceeds the default per-series limit");

    const auto points = businessSeries.points();
    CHECK(points.first().x() == 50.0,
          "bounded history removes the oldest points first");
    CHECK(points.last().x() == 649.0,
          "bounded history retains the newest point");

    const int countBeforeInvalidLimit = businessSeries.count();
    CHECK(!TrendHistoryBuffer::appendBounded(&businessSeries, 650.0, 650.0, 0),
          "zero capacity is rejected");
    CHECK(businessSeries.count() == countBeforeInvalidLimit,
          "invalid capacity does not modify the series");
    CHECK(!TrendHistoryBuffer::appendBounded(nullptr, 650.0, 650.0),
          "null series is rejected");

    QLineSeries smallSeries;
    for (int i = 0; i < 10; ++i) {
        CHECK(TrendHistoryBuffer::appendBounded(
                  &smallSeries, static_cast<qreal>(i), static_cast<qreal>(i), 4),
              "custom bounded append accepts valid samples");
    }
    CHECK(smallSeries.count() == 4,
          "custom capacity is enforced");
    CHECK(smallSeries.points().first().x() == 6.0
              && smallSeries.points().last().x() == 9.0,
          "custom capacity preserves the newest samples in order");

    qInfo() << "Trend history buffer tests passed.";
    return 0;
}
