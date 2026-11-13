#ifndef COMMUNICATIONDIAGNOSTICS_H
#define COMMUNICATIONDIAGNOSTICS_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QSqlDatabase>

class CommunicationManager;

struct CommunicationDiagnosticEvent
{
    qint64 id = 0;
    QDateTime timestamp;
    QString deviceId;
    QString category;
    QString severity;
    QString message;
};

struct CommunicationDeviceStatistics
{
    QString deviceId;
    qint64 txCount = 0;
    qint64 rxCount = 0;
    qint64 txBytes = 0;
    qint64 rxBytes = 0;
    qint64 errorCount = 0;
    qint64 reconnectCount = 0;
    QDateTime lastEvent;
    double txPerSecond = 0.0;
    double rxPerSecond = 0.0;
    double txBytesPerSecond = 0.0;
    double rxBytesPerSecond = 0.0;
    qint64 recentErrorCount = 0;
    QDateTime lastRx;
    qint64 lastRxSilenceMs = -1;
    double recentErrorRatePercent = 0.0;
    QString quality;
    QString qualityReason;
};

class CommunicationDiagnostics : public QObject
{
    Q_OBJECT
public:
    explicit CommunicationDiagnostics(CommunicationManager *manager,
                                      QObject *parent = nullptr);
    ~CommunicationDiagnostics() override;

    bool isReady() const;
    QString lastError() const;
    QList<CommunicationDiagnosticEvent> recentEvents(int limit = 100,
                                                       const QString &deviceId = {},
                                                       const QDateTime &start = {},
                                                       const QDateTime &end = {}) const;
    QList<CommunicationDeviceStatistics> statistics() const;
    CommunicationDeviceStatistics statisticsFor(const QString &deviceId) const;
    void recordHealthTimeout(const QString &deviceId, qint64 silenceMs);

signals:
    void diagnosticsChanged();
    void storageError(const QString &message);

private:
    bool initialize();
    void record(const QString &deviceId, const QString &category,
                const QString &severity, const QString &message,
                qint64 txBytes = 0, qint64 rxBytes = 0);
    static QString classifyError(const QString &message);

    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    bool m_ready = false;
};

#endif // COMMUNICATIONDIAGNOSTICS_H
