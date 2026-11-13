#ifndef HISTORYMAINTENANCE_H
#define HISTORYMAINTENANCE_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

struct HistoryStoragePreview
{
    bool ok = false;
    QString error;
    QDateTime cutoff;
    qint64 communicationRows = 0;
    qint64 businessRows = 0;
    qint64 communicationRowsToDelete = 0;
    qint64 businessRowsToDelete = 0;
    qint64 communicationPayloadBytesToDelete = 0;
    qint64 databaseFileBytes = 0;
};

struct HistoryCleanupResult
{
    bool ok = false;
    QString error;
    qint64 communicationRowsDeleted = 0;
    qint64 businessRowsDeleted = 0;
};

class HistoryMaintenance
{
public:
    static HistoryStoragePreview inspect(const QString &databasePath,
                                         const QDateTime &cutoff);
    static HistoryCleanupResult cleanupOlderThan(const QString &databasePath,
                                                  const QDateTime &cutoff,
                                                  int batchSize = 1000);
};

#endif // HISTORYMAINTENANCE_H
