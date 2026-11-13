#ifndef BUSINESSDATAPOINT_H
#define BUSINESSDATAPOINT_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

// 业务变量质量状态。Bad 表示值不可用于控制或报警计算。
enum class DataQuality
{
    Good,
    Uncertain,
    Bad
};

// 所有协议解析器统一输出该结构，页面和存储层不依赖具体报文格式。
struct BusinessDataPoint
{
    QString deviceId;
    QString variableId;
    QString name;
    double value = 0.0;
    QString unit;
    QDateTime timestamp;
    DataQuality quality = DataQuality::Good;
    QString sourceProtocol;
};

Q_DECLARE_METATYPE(BusinessDataPoint)
Q_DECLARE_METATYPE(QVector<BusinessDataPoint>)

#endif // BUSINESSDATAPOINT_H
