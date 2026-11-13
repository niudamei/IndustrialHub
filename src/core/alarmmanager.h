#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include "src/protocol/businessdatapoint.h"

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QHash>
#include <QVariant>
#include <QString>

struct AlarmRule
{
    QString ruleId;
    QString deviceId;
    QString variableId;
    QString name;
    bool lowEnabled = false;
    double lowLimit = 0.0;
    bool highEnabled = false;
    double highLimit = 0.0;
    QString severity = QStringLiteral("Warning");
    bool enabled = true;
};

struct AlarmEvent
{
    qint64 id = 0;
    QString ruleId;
    QString deviceId;
    QString variableId;
    QString variableName;
    double value = 0.0;
    QString unit;
    QString severity;
    QString state; // ACTIVE, ACKNOWLEDGED, RECOVERED, RECOVERED_ACK
    QString message;
    QDateTime triggeredAt;
    QDateTime acknowledgedAt;
    QDateTime recoveredAt;
    QString acknowledgedBy;
};

class AlarmManager : public QObject
{
    Q_OBJECT
public:
    explicit AlarmManager(QObject *parent = nullptr);
    ~AlarmManager() override;

    bool isReady() const;
    QString lastError() const;
    bool addRule(const AlarmRule &rule, QString *errorMessage = nullptr);
    bool removeRule(const QString &ruleId);
    bool setRuleEnabled(const QString &ruleId, bool enabled,
                        QString *errorMessage = nullptr);
    QList<AlarmRule> rules() const;
    QList<AlarmEvent> events(int limit = 500, QString *errorMessage = nullptr) const;
    bool acknowledge(qint64 eventId, const QString &user,
                     QString *errorMessage = nullptr);

public slots:
    void evaluateDataPoints(const QString &deviceId,
                            const QVector<BusinessDataPoint> &points);

signals:
    void alarmEventChanged(const AlarmEvent &event);
    void rulesChanged();
    void storageError(const QString &message);

private:
    bool initializeDatabase();
    bool writeError(const QString &message, QString *errorMessage = nullptr);
    bool insertTriggeredEvent(const AlarmRule &rule,
                              const BusinessDataPoint &point,
                              qint64 &eventId);
    bool updateEventState(qint64 eventId, const QString &state,
                          const QDateTime &time, const QString &user = QString());
    QList<AlarmEvent> queryEvents(const QString &where, int limit,
                                  const QVariantList &bindings,
                                  QString *errorMessage) const;

    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QList<AlarmRule> m_rules;
    QHash<QString, qint64> m_activeEventByRule;
};

#endif // ALARMMANAGER_H
