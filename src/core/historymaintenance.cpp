#include "historymaintenance.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
struct ScopedMaintenanceDatabase
{
    QString name = QStringLiteral("HistoryMaintenance_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db;
    ~ScopedMaintenanceDatabase()
    {
        if (db.isValid()) {
            db.close();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
        }
    }
    bool open(const QString &path, QString *error)
    {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
        if (db.open()) return true;
        if (error) *error = QStringLiteral("打开历史数据库失败：%1").arg(db.lastError().text());
        return false;
    }
};

bool scalar(QSqlDatabase db, const QString &sql, qint64 &value, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(sql) || !query.next()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    value = query.value(0).toLongLong();
    return true;
}

qint64 deleteBatches(QSqlDatabase db, const QString &table,
                     qint64 cutoffMs, int batchSize, QString *error)
{
    qint64 deleted = 0;
    const QString sql = QStringLiteral(
        "DELETE FROM %1 WHERE id IN ("
        "SELECT id FROM %1 WHERE timestamp_ms < ? "
        "ORDER BY timestamp_ms ASC, id ASC LIMIT ?)")
        .arg(table);
    while (true) {
        if (!db.transaction()) {
            if (error) *error = QStringLiteral("开启清理事务失败：%1").arg(db.lastError().text());
            return -1;
        }
        QSqlQuery query(db);
        query.prepare(sql);
        query.addBindValue(cutoffMs);
        query.addBindValue(batchSize);
        if (!query.exec()) {
            db.rollback();
            if (error) *error = QStringLiteral("清理 %1 失败：%2").arg(table, query.lastError().text());
            return -1;
        }
        const qint64 affected = query.numRowsAffected();
        query.finish();
        if (!db.commit()) {
            db.rollback();
            if (error) *error = QStringLiteral("提交 %1 清理事务失败：%2").arg(table, db.lastError().text());
            return -1;
        }
        deleted += qMax<qint64>(0, affected);
        if (affected <= 0) break;
    }
    return deleted;
}
} // namespace

HistoryStoragePreview HistoryMaintenance::inspect(const QString &databasePath,
                                                   const QDateTime &cutoff)
{
    HistoryStoragePreview result;
    result.cutoff = cutoff;
    if (databasePath.trimmed().isEmpty() || !cutoff.isValid() || cutoff > QDateTime::currentDateTime()) {
        result.error = QStringLiteral("数据库路径或清理截止时间无效。");
        return result;
    }
    ScopedMaintenanceDatabase connection;
    if (!connection.open(databasePath, &result.error)) return result;
    const qint64 cutoffMs = cutoff.toMSecsSinceEpoch();
    if (!scalar(connection.db, QStringLiteral("SELECT COUNT(*) FROM communication_history"),
                result.communicationRows, &result.error)
        || !scalar(connection.db,
                   QStringLiteral("SELECT COUNT(*) FROM business_data_history"),
                   result.businessRows, &result.error)
        || !scalar(connection.db,
                   QStringLiteral("SELECT COUNT(*) FROM communication_history WHERE timestamp_ms < %1").arg(cutoffMs),
                   result.communicationRowsToDelete, &result.error)
        || !scalar(connection.db,
                   QStringLiteral("SELECT COUNT(*) FROM business_data_history WHERE timestamp_ms < %1").arg(cutoffMs),
                   result.businessRowsToDelete, &result.error)
        || !scalar(connection.db,
                   QStringLiteral("SELECT COALESCE(SUM(payload_size), 0) FROM communication_history WHERE timestamp_ms < %1").arg(cutoffMs),
                   result.communicationPayloadBytesToDelete, &result.error))
        return result;
    result.databaseFileBytes = QFileInfo(databasePath).size();
    result.ok = true;
    return result;
}

HistoryCleanupResult HistoryMaintenance::cleanupOlderThan(
    const QString &databasePath, const QDateTime &cutoff, int batchSize)
{
    HistoryCleanupResult result;
    if (databasePath.trimmed().isEmpty() || !cutoff.isValid()
        || cutoff > QDateTime::currentDateTime() || batchSize < 1 || batchSize > 10000) {
        result.error = QStringLiteral("数据库路径、截止时间或批次大小无效。");
        return result;
    }
    ScopedMaintenanceDatabase connection;
    if (!connection.open(databasePath, &result.error)) return result;
    result.communicationRowsDeleted = deleteBatches(
        connection.db, QStringLiteral("communication_history"),
        cutoff.toMSecsSinceEpoch(), batchSize, &result.error);
    if (result.communicationRowsDeleted < 0) return result;
    result.businessRowsDeleted = deleteBatches(
        connection.db, QStringLiteral("business_data_history"),
        cutoff.toMSecsSinceEpoch(), batchSize, &result.error);
    if (result.businessRowsDeleted < 0) return result;
    result.ok = true;
    return result;
}
