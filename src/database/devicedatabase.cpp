#include "devicedatabase.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString dateTimeToString(const QDateTime &dateTime)
{
    return dateTime.toString(Qt::ISODateWithMs);
}

QDateTime dateTimeFromString(const QString &value)
{
    QDateTime result = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!result.isValid())
        result = QDateTime::fromString(value, Qt::ISODate);
    return result;
}
}

DeviceDatabase::DeviceDatabase()
{
    m_connectionName = QStringLiteral("IndustrialHubSQLite_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDirectory.isEmpty())
        dataDirectory = QDir(QCoreApplication::applicationDirPath()).filePath("data");

    QDir dir;
    if (!dir.mkpath(dataDirectory))
        return;

    m_databasePath = QDir(dataDirectory).filePath("industrialhub.db");
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
}

DeviceDatabase::~DeviceDatabase()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DeviceDatabase::initialize(QString *errorMessage)
{
    if (!m_database.isValid())
        return setError(QStringLiteral("无法创建 SQLite 数据库连接或创建数据目录失败。"),
                        errorMessage);

    if (!m_database.open())
        return setError(QStringLiteral("打开 SQLite 数据库失败：%1")
                            .arg(m_database.lastError().text()), errorMessage);

    QSqlQuery query(m_database);
    const QString createTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS devices ("
        "dev_id TEXT PRIMARY KEY COLLATE NOCASE,"
        "dev_name TEXT NOT NULL,"
        "dev_type TEXT,"
        "comm_type TEXT,"
        "dev_addr TEXT,"
        "status INTEGER NOT NULL DEFAULT 1,"
        "add_time TEXT NOT NULL,"
        "group_name TEXT NOT NULL DEFAULT '',"
        "enabled INTEGER NOT NULL DEFAULT 1"
        ")");

    if (!query.exec(createTableSql))
        return setError(QStringLiteral("创建 devices 数据表失败：%1")
                            .arg(query.lastError().text()), errorMessage);

    // Upgrade existing databases safely; repeated startup is idempotent.
    auto ensureColumn = [this](const QString &name, const QString &definition, QString *error) -> bool {
        QSqlQuery info(m_database);
        if (!info.exec(QStringLiteral("PRAGMA table_info(devices)")))
            return setError(QStringLiteral("检查设备表结构失败：%1").arg(info.lastError().text()), error);
        while (info.next()) {
            if (info.value(1).toString().compare(name, Qt::CaseInsensitive) == 0) return true;
        }
        QSqlQuery alter(m_database);
        if (!alter.exec(QStringLiteral("ALTER TABLE devices ADD COLUMN ") + definition))
            return setError(QStringLiteral("升级设备表字段 %1 失败：%2").arg(name, alter.lastError().text()), error);
        return true;
    };
    if (!ensureColumn(QStringLiteral("group_name"), QStringLiteral("group_name TEXT NOT NULL DEFAULT ''"), errorMessage)
        || !ensureColumn(QStringLiteral("enabled"), QStringLiteral("enabled INTEGER NOT NULL DEFAULT 1"), errorMessage)) return false;

    if (!query.exec(QStringLiteral("PRAGMA user_version = 1")))
        return setError(QStringLiteral("设置数据库版本失败：%1")
                            .arg(query.lastError().text()), errorMessage);

    m_initialized = true;
    return true;
}

bool DeviceDatabase::isOpen() const
{
    return m_initialized && m_database.isOpen();
}

QString DeviceDatabase::databasePath() const
{
    return m_databasePath;
}

bool DeviceDatabase::loadAll(QList<DeviceInfo> &devices, QString *errorMessage) const
{
    if (!isOpen())
        return setError(QStringLiteral("数据库尚未初始化。"), errorMessage);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT dev_id, dev_name, dev_type, comm_type, dev_addr, status, add_time, group_name, enabled "
        "FROM devices ORDER BY group_name COLLATE NOCASE, dev_name COLLATE NOCASE"));

    if (!query.exec())
        return setError(QStringLiteral("读取设备列表失败：%1")
                            .arg(query.lastError().text()), errorMessage);

    QList<DeviceInfo> loadedDevices;
    while (query.next()) {
        DeviceInfo device;
        device.devId = query.value(0).toString();
        device.devName = query.value(1).toString();
        device.devType = query.value(2).toString();
        device.commType = query.value(3).toString();
        device.devAddr = query.value(4).toString();

        const int statusValue = query.value(5).toInt();
        if (statusValue >= static_cast<int>(DeviceStatus::Online)
            && statusValue <= static_cast<int>(DeviceStatus::Fault)) {
            device.status = static_cast<DeviceStatus>(statusValue);
        } else {
            device.status = DeviceStatus::Offline;
        }

        device.addTime = dateTimeFromString(query.value(6).toString());
        device.groupName = query.value(7).toString();
        device.enabled = query.value(8).toInt() != 0;
        loadedDevices.append(device);
    }

    devices = loadedDevices;
    return true;
}

bool DeviceDatabase::insert(const DeviceInfo &device, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("数据库尚未初始化。"), errorMessage);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO devices (dev_id, dev_name, dev_type, comm_type, dev_addr, status, add_time, group_name, enabled) "
        "VALUES (:id, :name, :type, :comm, :addr, :status, :time, :group_name, :enabled)"));
    query.bindValue(QStringLiteral(":id"), device.devId);
    query.bindValue(QStringLiteral(":name"), device.devName);
    query.bindValue(QStringLiteral(":type"), device.devType);
    query.bindValue(QStringLiteral(":comm"), device.commType);
    query.bindValue(QStringLiteral(":addr"), device.devAddr);
    query.bindValue(QStringLiteral(":status"), static_cast<int>(device.status));
    query.bindValue(QStringLiteral(":time"), dateTimeToString(device.addTime));
    query.bindValue(QStringLiteral(":group_name"), device.groupName.trimmed());
    query.bindValue(QStringLiteral(":enabled"), device.enabled ? 1 : 0);

    if (!query.exec())
        return setError(QStringLiteral("写入设备失败：%1").arg(query.lastError().text()),
                        errorMessage);
    return true;
}

bool DeviceDatabase::update(const DeviceInfo &device, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("数据库尚未初始化。"), errorMessage);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE devices SET dev_name = :name, dev_type = :type, comm_type = :comm, "
        "dev_addr = :addr, status = :status, add_time = :time, group_name = :group_name, "
        "enabled = :enabled WHERE dev_id = :id"));
    query.bindValue(QStringLiteral(":id"), device.devId);
    query.bindValue(QStringLiteral(":name"), device.devName);
    query.bindValue(QStringLiteral(":type"), device.devType);
    query.bindValue(QStringLiteral(":comm"), device.commType);
    query.bindValue(QStringLiteral(":addr"), device.devAddr);
    query.bindValue(QStringLiteral(":status"), static_cast<int>(device.status));
    query.bindValue(QStringLiteral(":time"), dateTimeToString(device.addTime));
    query.bindValue(QStringLiteral(":group_name"), device.groupName.trimmed());
    query.bindValue(QStringLiteral(":enabled"), device.enabled ? 1 : 0);

    if (!query.exec())
        return setError(QStringLiteral("更新设备失败：%1").arg(query.lastError().text()),
                        errorMessage);
    if (query.numRowsAffected() == 0)
        return setError(QStringLiteral("数据库中不存在设备：%1").arg(device.devId),
                        errorMessage);
    return true;
}

bool DeviceDatabase::remove(const QString &devId, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("数据库尚未初始化。"), errorMessage);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM devices WHERE dev_id = :id"));
    query.bindValue(QStringLiteral(":id"), devId);

    if (!query.exec())
        return setError(QStringLiteral("删除设备失败：%1").arg(query.lastError().text()),
                        errorMessage);
    if (query.numRowsAffected() == 0)
        return setError(QStringLiteral("数据库中不存在设备：%1").arg(devId), errorMessage);
    return true;
}

bool DeviceDatabase::updateStatus(const QString &devId, DeviceStatus status,
                                  QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("数据库尚未初始化。"), errorMessage);

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE devices SET status = :status WHERE dev_id = :id"));
    query.bindValue(QStringLiteral(":id"), devId);
    query.bindValue(QStringLiteral(":status"), static_cast<int>(status));

    if (!query.exec())
        return setError(QStringLiteral("更新设备状态失败：%1")
                            .arg(query.lastError().text()), errorMessage);
    if (query.numRowsAffected() == 0)
        return setError(QStringLiteral("数据库中不存在设备：%1").arg(devId), errorMessage);
    return true;
}

bool DeviceDatabase::setError(const QString &message, QString *errorMessage) const
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}
