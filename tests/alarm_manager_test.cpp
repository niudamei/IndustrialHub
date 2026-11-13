#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDebug>

#include "src/core/alarmmanager.h"

#define CHECK(condition) do { if (!(condition)) { qCritical() << "CHECK failed:" << #condition << "line" << __LINE__; return 1; } } while (false)

static BusinessDataPoint point(double value, DataQuality quality = DataQuality::Good)
{
    BusinessDataPoint p;
    p.deviceId = QStringLiteral("DEV-ALARM");
    p.variableId = QStringLiteral("temperature");
    p.name = QStringLiteral("温度");
    p.value = value;
    p.unit = QStringLiteral("°C");
    p.timestamp = QDateTime::currentDateTime();
    p.quality = quality;
    p.sourceProtocol = QStringLiteral("TEST");
    return p;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IndustrialHubTests"));
    QCoreApplication::setApplicationName(QStringLiteral("AlarmManagerTest"));
    QStandardPaths::setTestModeEnabled(true);
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    QFile::remove(QDir(directory).filePath(QStringLiteral("industrialhub.db")));

    qint64 triggeredId = 0;
    {
        AlarmManager manager;
        CHECK(manager.isReady());
        AlarmRule rule;
        rule.ruleId = QStringLiteral("TEMP-HIGH");
        rule.deviceId = QStringLiteral("DEV-ALARM");
        rule.variableId = QStringLiteral("temperature");
        rule.name = QStringLiteral("温度高限");
        rule.highEnabled = true;
        rule.highLimit = 80.0;
        rule.severity = QStringLiteral("Critical");
        QString error;
        CHECK(manager.addRule(rule, &error));

        // 禁用规则后不应触发报警，重新启用后才恢复阈值判断。
        CHECK(manager.setRuleEnabled(rule.ruleId, false, &error));
        CHECK(!manager.rules().first().enabled);
        manager.evaluateDataPoints(rule.deviceId, {point(90.0)});
        CHECK(manager.events().isEmpty());
        CHECK(manager.setRuleEnabled(rule.ruleId, true, &error));
        CHECK(manager.rules().first().enabled);

        manager.evaluateDataPoints(rule.deviceId, {point(85.0, DataQuality::Uncertain)});
        CHECK(manager.events().isEmpty()); // 不可信数据不能触发报警

        manager.evaluateDataPoints(rule.deviceId, {point(85.0)});
        QList<AlarmEvent> events = manager.events();
        CHECK(events.size() == 1);
        CHECK(events.first().state == QStringLiteral("ACTIVE"));
        triggeredId = events.first().id;

        CHECK(manager.acknowledge(triggeredId, QStringLiteral("operator"), &error));
        events = manager.events();
        CHECK(events.first().state == QStringLiteral("ACKNOWLEDGED"));
        CHECK(events.first().acknowledgedBy == QStringLiteral("operator"));

        manager.evaluateDataPoints(rule.deviceId, {point(70.0)});
        events = manager.events();
        CHECK(events.first().state == QStringLiteral("RECOVERED_ACK"));
        CHECK(events.first().recoveredAt.isValid());
    }

    {
        AlarmManager reopened;
        CHECK(reopened.isReady());
        CHECK(reopened.rules().size() == 1);
        const QList<AlarmEvent> persisted = reopened.events();
        CHECK(persisted.size() == 1);
        CHECK(persisted.first().id == triggeredId);
        CHECK(persisted.first().state == QStringLiteral("RECOVERED_ACK"));
    }

    qInfo() << "Alarm manager tests passed.";
    return 0;
}
