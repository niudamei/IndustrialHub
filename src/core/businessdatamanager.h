#ifndef BUSINESSDATAMANAGER_H
#define BUSINESSDATAMANAGER_H

#include "src/protocol/businessdatapoint.h"

#include <QObject>
#include <QHash>
#include <QList>

// 保存各设备各变量的最新解析值。
// 原始通信帧仍由 DataManager 独立保存；本类暂不承担历史数据库职责。
class BusinessDataManager : public QObject
{
    Q_OBJECT

public:
    explicit BusinessDataManager(QObject *parent = nullptr);

    int deviceCount() const;
    int variableCount(const QString &deviceId = QString()) const;
    bool latestValue(const QString &deviceId,
                     const QString &variableId,
                     BusinessDataPoint &point) const;
    QList<BusinessDataPoint> latestValues(const QString &deviceId = QString()) const;
    void clearDevice(const QString &deviceId);
    void clear();

public slots:
    void updateDataPoints(const QString &deviceId,
                          const QVector<BusinessDataPoint> &points);

signals:
    void valuesUpdated(const QString &deviceId,
                       const QVector<BusinessDataPoint> &points);
    void valuesCleared(const QString &deviceId);

private:
    static QString key(const QString &value);

    // device key -> variable key -> latest point
    QHash<QString, QHash<QString, BusinessDataPoint>> m_latestValues;
};

#endif // BUSINESSDATAMANAGER_H
