#ifndef DEVICEDATABASE_H
#define DEVICEDATABASE_H

#include "src/pages/deviceinfo.h"
#include <QSqlDatabase>
#include <QString>
#include <QList>

// SQLite 持久化层：只负责设备数据的读写，不处理页面逻辑。
class DeviceDatabase
{
public:
    DeviceDatabase();
    ~DeviceDatabase();

    DeviceDatabase(const DeviceDatabase &) = delete;
    DeviceDatabase &operator=(const DeviceDatabase &) = delete;

    bool initialize(QString *errorMessage = nullptr);
    bool isOpen() const;
    QString databasePath() const;

    bool loadAll(QList<DeviceInfo> &devices, QString *errorMessage = nullptr) const;
    bool insert(const DeviceInfo &device, QString *errorMessage = nullptr);
    bool update(const DeviceInfo &device, QString *errorMessage = nullptr);
    bool remove(const QString &devId, QString *errorMessage = nullptr);
    bool updateStatus(const QString &devId, DeviceStatus status,
                      QString *errorMessage = nullptr);

private:
    bool setError(const QString &message, QString *errorMessage) const;

    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_databasePath;
    bool m_initialized = false;
};

#endif // DEVICEDATABASE_H
