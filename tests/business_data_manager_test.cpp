#include <QCoreApplication>
#include <QDebug>

#include "src/core/businessdatamanager.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        qCritical() << "CHECK failed:" << #condition << "at line" << __LINE__; \
        return 1; \
    } \
} while (false)

static BusinessDataPoint makePoint(const QString &device,
                                   const QString &variable,
                                   double value,
                                   const QDateTime &timestamp)
{
    BusinessDataPoint point;
    point.deviceId = device;
    point.variableId = variable;
    point.name = variable;
    point.value = value;
    point.unit = QStringLiteral("unit");
    point.timestamp = timestamp;
    point.quality = DataQuality::Good;
    point.sourceProtocol = QStringLiteral("TEST");
    return point;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    BusinessDataManager manager;

    const QDateTime t1 = QDateTime::fromMSecsSinceEpoch(1000, QTimeZone::utc());
    const QDateTime t2 = QDateTime::fromMSecsSinceEpoch(2000, QTimeZone::utc());

    QVector<BusinessDataPoint> first {
        makePoint(QStringLiteral("DEV-01"), QStringLiteral("temperature"), 25.0, t1),
        makePoint(QStringLiteral("DEV-01"), QStringLiteral("rpm"), 1000.0, t1)
    };
    manager.updateDataPoints(QStringLiteral("dev-01"), first);

    CHECK(manager.deviceCount() == 1);
    CHECK(manager.variableCount(QStringLiteral("DEV-01")) == 2);
    CHECK(manager.variableCount() == 2);

    BusinessDataPoint latest;
    CHECK(manager.latestValue(QStringLiteral("DEV-01"),
                              QStringLiteral("TEMPERATURE"), latest));
    CHECK(qAbs(latest.value - 25.0) < 0.000001);

    QVector<BusinessDataPoint> newer {
        makePoint(QStringLiteral("DEV-01"), QStringLiteral("temperature"), 26.5, t2)
    };
    manager.updateDataPoints(QStringLiteral("DEV-01"), newer);
    CHECK(manager.latestValue(QStringLiteral("DEV-01"),
                              QStringLiteral("temperature"), latest));
    CHECK(qAbs(latest.value - 26.5) < 0.000001);

    QVector<BusinessDataPoint> stale {
        makePoint(QStringLiteral("DEV-01"), QStringLiteral("temperature"), 10.0, t1)
    };
    manager.updateDataPoints(QStringLiteral("DEV-01"), stale);
    CHECK(manager.latestValue(QStringLiteral("DEV-01"),
                              QStringLiteral("temperature"), latest));
    CHECK(qAbs(latest.value - 26.5) < 0.000001);

    CHECK(manager.latestValues(QStringLiteral("DEV-01")).size() == 2);
    CHECK(!manager.latestValue(QStringLiteral("missing"),
                               QStringLiteral("temperature"), latest));

    manager.clearDevice(QStringLiteral("dev-01"));
    CHECK(manager.deviceCount() == 0);
    CHECK(manager.variableCount() == 0);

    qInfo() << "Business data manager tests passed.";
    return 0;
}
