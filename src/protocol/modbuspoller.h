#ifndef MODBUSPOLLER_H
#define MODBUSPOLLER_H

#include "modbusregistermap.h"
#include "modbusmaster.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

class BusinessDataManager;

class ModbusPoller : public QObject
{
    Q_OBJECT
public:
    explicit ModbusPoller(ModbusMaster *master, QString deviceId,
                          QObject *parent = nullptr);

    bool bindBusinessDataManager(BusinessDataManager *manager);
    bool setMappings(const QVector<ModbusRegisterMap::Mapping> &mappings,
                     QString *error = nullptr);
    QVector<ModbusRegisterMap::Mapping> mappings() const;
    void start(int intervalMs = 1000);
    void stop();
    bool isRunning() const;
    bool isPolling() const;

public slots:
    void pollOnce();

signals:
    void pointsReady(const QString &deviceId,
                     const QVector<BusinessDataPoint> &points);
    void pollError(const QString &deviceId, const QString &variableId,
                   const QString &message);
    void cycleFinished(int successfulPoints, int failedPoints);

private slots:
    void onRequestCompleted(quint64 requestId,
                            const ModbusCodec::Response &response);
    void onRequestFailed(quint64 requestId, const QString &reason);

private:
    void finishOne();
    void finishCycle();

    QPointer<ModbusMaster> m_master;
    QPointer<BusinessDataManager> m_businessDataManager;
    QMetaObject::Connection m_businessConnection;
    QString m_deviceId;
    QVector<ModbusRegisterMap::Mapping> m_mappings;
    QHash<quint64, int> m_requestMapping;
    QTimer m_timer;
    int m_intervalMs = 1000;
    int m_cyclePending = 0;
    int m_successfulPoints = 0;
    int m_failedPoints = 0;
    bool m_running = false;
    bool m_cycleActive = false;
};

#endif // MODBUSPOLLER_H
