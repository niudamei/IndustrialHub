#include "alarmmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>
#include <QHash>
#include <QtMath>

namespace {
QString normalized(const QString &value)
{
    return value.trimmed().toCaseFolded();
}
}

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
{
    m_connectionName = QStringLiteral("IndustrialHubAlarm_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty())
        directory = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    if (!QDir().mkpath(directory)) {
        m_lastError = QStringLiteral("无法创建报警数据目录：%1").arg(directory);
        return;
    }
    m_databasePath = QDir(directory).filePath("industrialhub.db");
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    initializeDatabase();
}

AlarmManager::~AlarmManager()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool AlarmManager::isReady() const
{
    return m_database.isValid() && m_database.isOpen();
}

QString AlarmManager::lastError() const
{
    return m_lastError;
}

bool AlarmManager::initializeDatabase()
{
    if (!m_database.open())
        return writeError(QStringLiteral("打开报警数据库失败：%1").arg(m_database.lastError().text()));

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarm_events ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "rule_id TEXT NOT NULL, device_id TEXT NOT NULL COLLATE NOCASE,"
            "variable_id TEXT NOT NULL COLLATE NOCASE, variable_name TEXT NOT NULL,"
            "value REAL NOT NULL, unit TEXT NOT NULL, severity TEXT NOT NULL,"
            "state TEXT NOT NULL, message TEXT NOT NULL,"
            "triggered_at_ms INTEGER NOT NULL, acknowledged_at_ms INTEGER,"
            "recovered_at_ms INTEGER, acknowledged_by TEXT)"))) {
        return writeError(QStringLiteral("创建报警事件表失败：%1").arg(query.lastError().text()));
    }
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS alarm_rules ("
            "rule_id TEXT PRIMARY KEY COLLATE NOCASE, device_id TEXT NOT NULL COLLATE NOCASE,"
            "variable_id TEXT NOT NULL COLLATE NOCASE, name TEXT NOT NULL,"
            "low_enabled INTEGER NOT NULL, low_limit REAL NOT NULL,"
            "high_enabled INTEGER NOT NULL, high_limit REAL NOT NULL,"
            "severity TEXT NOT NULL, enabled INTEGER NOT NULL)"))) {
        return writeError(QStringLiteral("创建报警规则表失败：%1").arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_alarm_events_time "
            "ON alarm_events(triggered_at_ms DESC)"))) {
        return writeError(QStringLiteral("创建报警时间索引失败：%1").arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "SELECT rule_id, device_id, variable_id, name, low_enabled, low_limit, "
            "high_enabled, high_limit, severity, enabled FROM alarm_rules"))) {
        return writeError(QStringLiteral("读取报警规则失败：%1").arg(query.lastError().text()));
    }
    while (query.next()) {
        AlarmRule rule;
        rule.ruleId = query.value(0).toString();
        rule.deviceId = query.value(1).toString();
        rule.variableId = query.value(2).toString();
        rule.name = query.value(3).toString();
        rule.lowEnabled = query.value(4).toBool();
        rule.lowLimit = query.value(5).toDouble();
        rule.highEnabled = query.value(6).toBool();
        rule.highLimit = query.value(7).toDouble();
        rule.severity = query.value(8).toString();
        rule.enabled = query.value(9).toBool();
        m_rules.append(rule);
    }

    // 重启后活动报警仍然有效，恢复规则状态索引，等待后续数据判断恢复。
    if (!query.exec(QStringLiteral(
            "SELECT rule_id, id FROM alarm_events "
            "WHERE state IN ('ACTIVE','ACKNOWLEDGED')"))) {
        return writeError(QStringLiteral("读取活动报警失败：%1").arg(query.lastError().text()));
    }
    while (query.next())
        m_activeEventByRule.insert(query.value(0).toString(), query.value(1).toLongLong());

    m_lastError.clear();
    return true;
}

bool AlarmManager::addRule(const AlarmRule &rule, QString *errorMessage)
{
    AlarmRule value = rule;
    value.ruleId = value.ruleId.trimmed();
    value.deviceId = value.deviceId.trimmed();
    value.variableId = value.variableId.trimmed();
    value.name = value.name.trimmed();
    if (value.ruleId.isEmpty() || value.deviceId.isEmpty() || value.variableId.isEmpty())
        return writeError(QStringLiteral("规则 ID、设备 ID 和变量 ID 不能为空。"), errorMessage);
    if (!value.lowEnabled && !value.highEnabled)
        return writeError(QStringLiteral("至少启用低限或高限中的一项。"), errorMessage);
    if ((value.lowEnabled && !qIsFinite(value.lowLimit))
        || (value.highEnabled && !qIsFinite(value.highLimit))
        || (value.lowEnabled && value.highEnabled && value.lowLimit >= value.highLimit))
        return writeError(QStringLiteral("报警阈值无效：低限必须小于高限。"), errorMessage);
    if (value.severity != QStringLiteral("Info")
        && value.severity != QStringLiteral("Warning")
        && value.severity != QStringLiteral("Critical"))
        return writeError(QStringLiteral("报警等级只允许 Info、Warning 或 Critical。"), errorMessage);

    QSqlQuery save(m_database);
    save.prepare(QStringLiteral(
        "INSERT INTO alarm_rules (rule_id, device_id, variable_id, name, low_enabled, "
        "low_limit, high_enabled, high_limit, severity, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(rule_id) DO UPDATE SET device_id=excluded.device_id, "
        "variable_id=excluded.variable_id, name=excluded.name, "
        "low_enabled=excluded.low_enabled, low_limit=excluded.low_limit, "
        "high_enabled=excluded.high_enabled, high_limit=excluded.high_limit, "
        "severity=excluded.severity, enabled=excluded.enabled"));
    save.addBindValue(value.ruleId);
    save.addBindValue(value.deviceId);
    save.addBindValue(value.variableId);
    save.addBindValue(value.name);
    save.addBindValue(value.lowEnabled ? 1 : 0);
    save.addBindValue(value.lowLimit);
    save.addBindValue(value.highEnabled ? 1 : 0);
    save.addBindValue(value.highLimit);
    save.addBindValue(value.severity);
    save.addBindValue(value.enabled ? 1 : 0);
    if (!save.exec())
        return writeError(QStringLiteral("保存报警规则失败：%1").arg(save.lastError().text()), errorMessage);

    bool replaced = false;
    for (int i = 0; i < m_rules.size(); ++i) {
        if (normalized(m_rules.at(i).ruleId) == normalized(value.ruleId)) {
            m_rules[i] = value;
            replaced = true;
            break;
        }
    }
    if (!replaced) m_rules.append(value);
    emit rulesChanged();
    return true;
}

bool AlarmManager::removeRule(const QString &ruleId)
{
    const QString key = normalized(ruleId);
    for (int i = m_rules.size() - 1; i >= 0; --i) {
        if (normalized(m_rules.at(i).ruleId) == key) {
            QSqlQuery remove(m_database);
            remove.prepare(QStringLiteral("DELETE FROM alarm_rules WHERE rule_id = ?"));
            remove.addBindValue(m_rules.at(i).ruleId);
            if (!remove.exec())
                return writeError(QStringLiteral("删除报警规则失败：%1").arg(remove.lastError().text()));
            m_rules.removeAt(i);
            emit rulesChanged();
            return true;
        }
    }
    return false;
}

bool AlarmManager::setRuleEnabled(const QString &ruleId, bool enabled,
                                   QString *errorMessage)
{
    const QString key = normalized(ruleId);
    for (int i = 0; i < m_rules.size(); ++i) {
        if (normalized(m_rules.at(i).ruleId) != key)
            continue;

        QSqlQuery update(m_database);
        update.prepare(QStringLiteral(
            "UPDATE alarm_rules SET enabled = ? WHERE rule_id = ?"));
        update.addBindValue(enabled ? 1 : 0);
        update.addBindValue(m_rules.at(i).ruleId);
        if (!update.exec())
            return writeError(QStringLiteral("更新报警规则状态失败：%1")
                                  .arg(update.lastError().text()), errorMessage);
        if (update.numRowsAffected() != 1)
            return writeError(QStringLiteral("报警规则不存在或状态未更新。"), errorMessage);

        m_rules[i].enabled = enabled;
        emit rulesChanged();
        return true;
    }
    return writeError(QStringLiteral("未找到指定的报警规则：%1").arg(ruleId), errorMessage);
}

QList<AlarmRule> AlarmManager::rules() const
{
    return m_rules;
}

QList<AlarmEvent> AlarmManager::events(int limit, QString *errorMessage) const
{
    return queryEvents(QString(), limit, {}, errorMessage);
}

QList<AlarmEvent> AlarmManager::queryEvents(const QString &where, int limit,
                                             const QVariantList &bindings,
                                             QString *errorMessage) const
{
    QList<AlarmEvent> result;
    if (!isReady()) {
        if (errorMessage) *errorMessage = QStringLiteral("报警数据库尚未就绪：%1").arg(m_lastError);
        return result;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, rule_id, device_id, variable_id, variable_name, value, unit, "
        "severity, state, message, triggered_at_ms, acknowledged_at_ms, "
        "recovered_at_ms, acknowledged_by FROM alarm_events ")
        + where + QStringLiteral(" ORDER BY triggered_at_ms DESC, id DESC LIMIT ?"));
    for (int i = 0; i < bindings.size(); ++i)
        query.bindValue(i, bindings.at(i));
    query.bindValue(bindings.size(), qMax(1, limit));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return result;
    }
    while (query.next()) {
        AlarmEvent event;
        event.id = query.value(0).toLongLong();
        event.ruleId = query.value(1).toString();
        event.deviceId = query.value(2).toString();
        event.variableId = query.value(3).toString();
        event.variableName = query.value(4).toString();
        event.value = query.value(5).toDouble();
        event.unit = query.value(6).toString();
        event.severity = query.value(7).toString();
        event.state = query.value(8).toString();
        event.message = query.value(9).toString();
        event.triggeredAt = QDateTime::fromMSecsSinceEpoch(query.value(10).toLongLong());
        if (!query.value(11).isNull())
            event.acknowledgedAt = QDateTime::fromMSecsSinceEpoch(query.value(11).toLongLong());
        if (!query.value(12).isNull())
            event.recoveredAt = QDateTime::fromMSecsSinceEpoch(query.value(12).toLongLong());
        event.acknowledgedBy = query.value(13).toString();
        result.append(event);
    }
    return result;
}

bool AlarmManager::acknowledge(qint64 eventId, const QString &user, QString *errorMessage)
{
    if (user.trimmed().isEmpty())
        return writeError(QStringLiteral("确认人不能为空。"), errorMessage);
    QSqlQuery find(m_database);
    find.prepare(QStringLiteral("SELECT state FROM alarm_events WHERE id = ?"));
    find.addBindValue(eventId);
    if (!find.exec() || !find.next())
        return writeError(QStringLiteral("报警事件不存在或查询失败。"), errorMessage);
    const QString state = find.value(0).toString();
    if (state != QStringLiteral("ACTIVE") && state != QStringLiteral("RECOVERED"))
        return writeError(QStringLiteral("该报警已确认，无需重复确认。"), errorMessage);

    const QString nextState = state == QStringLiteral("ACTIVE")
        ? QStringLiteral("ACKNOWLEDGED") : QStringLiteral("RECOVERED_ACK");
    if (!updateEventState(eventId, nextState, QDateTime::currentDateTime(), user))
        return writeError(m_lastError, errorMessage);
    const QList<AlarmEvent> changed = queryEvents(QStringLiteral("WHERE id = ?"), 1, {eventId}, errorMessage);
    if (!changed.isEmpty())
        emit alarmEventChanged(changed.first());
    return true;
}

bool AlarmManager::insertTriggeredEvent(const AlarmRule &rule,
                                        const BusinessDataPoint &point,
                                        qint64 &eventId)
{
    const bool low = rule.lowEnabled && point.value < rule.lowLimit;
    const bool high = rule.highEnabled && point.value > rule.highLimit;
    const QString condition = low
        ? QStringLiteral("低于下限 %1").arg(rule.lowLimit)
        : QStringLiteral("高于上限 %1").arg(rule.highLimit);
    const QString message = QStringLiteral("%1：%2，当前值 %3 %4")
        .arg(point.name.isEmpty() ? rule.variableId : point.name,
             condition, QString::number(point.value, 'g', 12), point.unit);
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO alarm_events (rule_id, device_id, variable_id, variable_name, "
        "value, unit, severity, state, message, triggered_at_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 'ACTIVE', ?, ?)"));
    query.addBindValue(rule.ruleId);
    query.addBindValue(point.deviceId);
    query.addBindValue(point.variableId);
    query.addBindValue(point.name);
    query.addBindValue(point.value);
    query.addBindValue(point.unit);
    query.addBindValue(rule.severity);
    query.addBindValue(message);
    query.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch());
    if (!query.exec())
        return writeError(QStringLiteral("写入报警事件失败：%1").arg(query.lastError().text()));
    eventId = query.lastInsertId().toLongLong();
    return true;
}

bool AlarmManager::updateEventState(qint64 eventId, const QString &state,
                                    const QDateTime &time, const QString &user)
{
    QSqlQuery query(m_database);
    if (state == QStringLiteral("ACKNOWLEDGED")
        || state == QStringLiteral("RECOVERED_ACK")) {
        query.prepare(QStringLiteral(
            "UPDATE alarm_events SET state = ?, acknowledged_at_ms = ?, acknowledged_by = ? WHERE id = ?"));
        query.addBindValue(state);
        query.addBindValue(time.toMSecsSinceEpoch());
        query.addBindValue(user);
        query.addBindValue(eventId);
    } else {
        query.prepare(QStringLiteral(
            "UPDATE alarm_events SET state = ?, recovered_at_ms = ? WHERE id = ?"));
        query.addBindValue(state);
        query.addBindValue(time.toMSecsSinceEpoch());
        query.addBindValue(eventId);
    }
    if (!query.exec() || query.numRowsAffected() != 1)
        return writeError(QStringLiteral("更新报警状态失败：%1").arg(query.lastError().text()));
    return true;
}

void AlarmManager::evaluateDataPoints(const QString &deviceId,
                                      const QVector<BusinessDataPoint> &points)
{
    if (!isReady())
        return;
    for (const BusinessDataPoint &point : points) {
        if (point.quality != DataQuality::Good || !qIsFinite(point.value))
            continue;
        for (const AlarmRule &rule : m_rules) {
            if (!rule.enabled || normalized(rule.deviceId) != normalized(deviceId)
                || normalized(rule.variableId) != normalized(point.variableId))
                continue;
            const bool outOfRange = (rule.lowEnabled && point.value < rule.lowLimit)
                                    || (rule.highEnabled && point.value > rule.highLimit);
            const QString activeKey = normalized(rule.ruleId);
            const qint64 activeId = m_activeEventByRule.value(activeKey, 0);
            if (outOfRange && activeId == 0) {
                qint64 eventId = 0;
                if (insertTriggeredEvent(rule, point, eventId)) {
                    m_activeEventByRule.insert(activeKey, eventId);
                    const QList<AlarmEvent> latest = events(1);
                    if (!latest.isEmpty()) emit alarmEventChanged(latest.first());
                }
            } else if (!outOfRange && activeId != 0) {
                QSqlQuery stateQuery(m_database);
                stateQuery.prepare(QStringLiteral("SELECT state FROM alarm_events WHERE id = ?"));
                stateQuery.addBindValue(activeId);
                if (stateQuery.exec() && stateQuery.next()) {
                    const QString state = stateQuery.value(0).toString();
                    const QString recoveredState = state == QStringLiteral("ACKNOWLEDGED")
                        ? QStringLiteral("RECOVERED_ACK") : QStringLiteral("RECOVERED");
                    if (updateEventState(activeId, recoveredState, QDateTime::currentDateTime())) {
                        m_activeEventByRule.remove(activeKey);
                        const QList<AlarmEvent> latest = events(1);
                        if (!latest.isEmpty()) emit alarmEventChanged(latest.first());
                    }
                } else {
                    m_activeEventByRule.remove(activeKey);
                }
            }
        }
    }
}

bool AlarmManager::writeError(const QString &message, QString *errorMessage)
{
    m_lastError = message;
    if (errorMessage) *errorMessage = message;
    emit storageError(message);
    return false;
}
