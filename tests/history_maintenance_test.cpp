#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "src/core/historymaintenance.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString path = QDir::temp().filePath(
        QStringLiteral("industrialhub_retention_%1.db")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    const QString connectionName = QStringLiteral("fixture_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) return 1;
        QSqlQuery q(db);
        if (!q.exec("CREATE TABLE communication_history (id INTEGER PRIMARY KEY, timestamp_ms INTEGER, payload_size INTEGER)")) return 2;
        if (!q.exec("CREATE TABLE business_data_history (id INTEGER PRIMARY KEY, timestamp_ms INTEGER)")) return 3;
        if (!q.exec("INSERT INTO communication_history VALUES (1, 1000, 12), (2, 2000, 20), (3, 9000, 5)")) return 4;
        if (!q.exec("INSERT INTO business_data_history VALUES (1, 1000), (2, 9000), (3, 9500)")) return 5;
        q.finish();
        q = QSqlQuery();
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
    }

    const QDateTime cutoff = QDateTime::fromMSecsSinceEpoch(5000);
    const HistoryStoragePreview preview = HistoryMaintenance::inspect(path, cutoff);
    if (!preview.ok || preview.communicationRows != 3 || preview.businessRows != 3
        || preview.communicationRowsToDelete != 2 || preview.businessRowsToDelete != 1
        || preview.communicationPayloadBytesToDelete != 32) return 6;

    const HistoryCleanupResult cleanup = HistoryMaintenance::cleanupOlderThan(path, cutoff, 1);
    if (!cleanup.ok || cleanup.communicationRowsDeleted != 2
        || cleanup.businessRowsDeleted != 1) return 7;

    const HistoryStoragePreview after = HistoryMaintenance::inspect(path, cutoff);
    if (!after.ok || after.communicationRows != 1 || after.businessRows != 2
        || after.communicationRowsToDelete != 0 || after.businessRowsToDelete != 0) return 8;

    const HistoryCleanupResult invalid = HistoryMaintenance::cleanupOlderThan(path, cutoff, 0);
    if (invalid.ok) return 9;
    QFile::remove(path);
    return 0;
}
