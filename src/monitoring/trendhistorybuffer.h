#ifndef TRENDHISTORYBUFFER_H
#define TRENDHISTORYBUFFER_H

#include <QLineSeries>

namespace TrendHistoryBuffer {

// The demo interval can be as low as 100 ms. 600 samples therefore cover
// the complete 60-second business trend window at the fastest configured rate.
constexpr int MaxBusinessPointsPerSeries = 600;

inline bool appendBounded(QLineSeries *series,
                          qreal x,
                          qreal y,
                          int maxPoints = MaxBusinessPointsPerSeries)
{
    if (!series || maxPoints <= 0)
        return false;

    series->append(x, y);
    while (series->count() > maxPoints)
        series->remove(0);

    return true;
}

} // namespace TrendHistoryBuffer

#endif // TRENDHISTORYBUFFER_H
