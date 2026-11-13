#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QVector>
#include <QSqlDatabase>

#include "src/protocol/businessdatapoint.h"

class CommunicationManager;
class QTimer;

struct CommunicationHistoryRecord
{
    qint64 id = 0;
    QDateTime timestamp;
    QString deviceId;
    QString direction;
    QByteArray payload;
};

struct BusinessDataHistoryRecord
{
    qint64 id = 0;
    BusinessDataPoint point;
};

// 统一数据入口：通信原始帧与业务变量分别存储在同一 SQLite 数据库。
class DataManager : public QObject
{
    Q_OBJECT

public:
    explicit DataManager(CommunicationManager *communicationManager,
                         QObject *parent = nullptr);
    ~DataManager() override;

    bool isReady() const;
    QString databasePath() const;
    QString lastError() const;

    bool queryCommunicationHistory(const QDateTime &start,
                                   const QDateTime &end,
                                   const QString &deviceId,
                                   const QString &direction,
                                   int offset,
                                   int limit,
                                   QList<CommunicationHistoryRecord> &records,
                                   int &totalCount,
                                   QString *errorMessage = nullptr);

    bool queryBusinessDataHistory(const QDateTime &start,
                                  const QDateTime &end,
                                  const QString &deviceId,
                                  const QString &variableId,
                                  int offset,
                                  int limit,
                                  QList<BusinessDataHistoryRecord> &records,
                                  int &totalCount,
                                  QString *errorMessage = nullptr);

public slots:
    void appendBusinessDataPoints(const QString &deviceId,
                                  const QVector<BusinessDataPoint> &points);
    void flushPendingRecords();

signals:
    void recordsWritten(int count);
    void storageError(const QString &message);

private slots:
    void onDataSent(const QString &deviceId, const QByteArray &data);
    void onDataReceived(const QString &deviceId, const QByteArray &data);

private:
    void appendRecord(const QString &deviceId,
                      const QString &direction,
                      const QByteArray &payload);
    void flushPendingBusinessData();
    bool initializeDatabase();
    bool setError(const QString &message, QString *errorMessage = nullptr);

    CommunicationManager *m_communicationManager = nullptr;
    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    bool m_ready = false;
    QQueue<CommunicationHistoryRecord> m_pendingRecords;
    QQueue<BusinessDataHistoryRecord> m_pendingBusinessData;
    QTimer *m_flushTimer = nullptr;
    static constexpr int MaxPendingRecords = 10000;
    static constexpr int MaxPendingBusinessData = 20000;
    static constexpr int BatchSize = 200;
};

#endif // DATAMANAGER_H
