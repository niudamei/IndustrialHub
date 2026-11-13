#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QString>

#include <QSqlDatabase>

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

// 统一通信数据入口：接收通信管理器的 TX/RX 事件，批量写入 SQLite。
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

public slots:
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
    bool initializeDatabase();
    bool setError(const QString &message, QString *errorMessage = nullptr);

    CommunicationManager *m_communicationManager = nullptr;
    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QQueue<CommunicationHistoryRecord> m_pendingRecords;
    QTimer *m_flushTimer = nullptr;
    static constexpr int MaxPendingRecords = 10000;
    static constexpr int BatchSize = 200;
};

#endif // DATAMANAGER_H
