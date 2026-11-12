#ifndef DEVICEINFO_H
#define DEVICEINFO_H

#include <QString>
#include <QDateTime>

enum class DeviceStatus
{
    Online,
    Offline,
    Fault
};

struct DeviceInfo
{
    QString devId;
    QString devName;
    QString devType;
    QString commType;
    QString devAddr;

    DeviceStatus status = DeviceStatus::Offline;

    QDateTime addTime;
    DeviceInfo() = default;
    DeviceInfo(QString id,
               QString name,
               QString type,
               QString comm,
               QString addr,
               DeviceStatus st)
        : devId(id),
        devName(name),
        devType(type),
        commType(comm),
        devAddr(addr),
        status(st),
        addTime(QDateTime::currentDateTime())
    {}
};

#endif // DEVICEINFO_H