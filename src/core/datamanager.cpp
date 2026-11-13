#include "datamanager.h"

#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

DataManager::DataManager(CommunicationManager *communicationManager, QObject *parent)
    : QObject(parent),
      m_communicationManager(communicationManager)
{
    m_connectionName = QStringLiteral("IndustrialHubHistory_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDirectory.isEmpty())
        dataDirectory = QDir(QCoreApplication::applicationDirPath()).filePath("data");

    QDir dir;
    if (!dir.mkpath(dataDirectory)) {
        m_lastError = QStringLiteral("无法创建历史数据目录：%1").arg(dataDirectory);
        return;
    }

    m_databasePath = QDir(dataDirectory).filePath("industrialhub.db");
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);

    if (!initializeDatabase())
        return;

    if (m_communicationManager) {
        connect(m_communicationManager, &CommunicationManager::deviceDataSent,
                this, &DataManager::onDataSent);
        connect(m_communicationManager, &CommunicationManager::deviceDataReceived,
                this, &DataManager::onDataReceived);
    }

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(250);
    connect(m_flushTimer, &QTimer::timeout,
            this, &DataManager::flushPendingRecords);
    m_flushTimer->start();
}

DataManager::~DataManager()
{
    flushPendingRecords();
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool DataManager::isReady() const
{
    return m_ready && m_database.isValid() && m_database.isOpen();
}

QString DataManager::databasePath() const
{
    return m_databasePath;
}

QString DataManager::lastError() const
{
    return m_lastError;
}

bool DataManager::initializeDatabase()
{
    if (!m_database.open())
        return setError(QStringLiteral("打开历史数据库失败：%1")
                            .arg(m_database.lastError().text()));

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS communication_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp_ms INTEGER NOT NULL,"
            "device_id TEXT NOT NULL COLLATE NOCASE,"
            "direction TEXT NOT NULL,"
            "payload BLOB NOT NULL,"
            "payload_size INTEGER NOT NULL)"))) {
        return setError(QStringLiteral("创建通信历史表失败：%1")
                            .arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_communication_history_time "
            "ON communication_history(timestamp_ms)"))) {
        return setError(QStringLiteral("创建历史时间索引失败：%1")
                            .arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_communication_history_device_time "
            "ON communication_history(device_id, timestamp_ms)"))) {
        return setError(QStringLiteral("创建历史设备索引失败：%1")
                            .arg(query.lastError().text()));
    }

    m_lastError.clear();
    m_ready = true;
    return true;
}

void DataManager::onDataSent(const QString &deviceId, const QByteArray &data)
{
    appendRecord(deviceId, QStringLiteral("TX"), data);
}

void DataManager::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    appendRecord(deviceId, QStringLiteral("RX"), data);
}

void DataManager::appendRecord(const QString &deviceId,
                               const QString &direction,
                               const QByteArray &payload)
{
    if (!isReady() || deviceId.trimmed().isEmpty())
        return;

    CommunicationHistoryRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.deviceId = deviceId.trimmed();
    record.direction = direction;
    record.payload = payload;

    if (m_pendingRecords.size() >= MaxPendingRecords) {
        m_pendingRecords.dequeue();
        emit storageError(QStringLiteral(
            "历史数据写入速度超过处理能力，已丢弃最旧的一条待写记录。"));
    }
    m_pendingRecords.enqueue(record);

    if (m_pendingRecords.size() >= BatchSize)
        flushPendingRecords();
}

void DataManager::flushPendingRecords()
{
    if (!isReady() || m_pendingRecords.isEmpty())
        return;

    const int count = qMin(BatchSize, m_pendingRecords.size());
    if (!m_database.transaction()) {
        const QString message = QStringLiteral("开启历史数据事务失败：%1")
                                     .arg(m_database.lastError().text());
        m_lastError = message;
        emit storageError(message);
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO communication_history "
        "(timestamp_ms, device_id, direction, payload, payload_size) "
        "VALUES (:timestamp, :device, :direction, :payload, :size)"));

    for (int i = 0; i < count; ++i) {
        const CommunicationHistoryRecord &record = m_pendingRecords.at(i);
        query.bindValue(QStringLiteral(":timestamp"),
                        record.timestamp.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":device"), record.deviceId);
        query.bindValue(QStringLiteral(":direction"), record.direction);
        query.bindValue(QStringLiteral(":payload"), record.payload);
        query.bindValue(QStringLiteral(":size"), record.payload.size());
        if (!query.exec()) {
            m_database.rollback();
            const QString message = QStringLiteral("写入通信历史失败：%1")
                                        .arg(query.lastError().text());
            m_lastError = message;
            emit storageError(message);
            return;
        }
    }

    if (!m_database.commit()) {
        m_database.rollback();
        const QString message = QStringLiteral("提交历史数据事务失败：%1")
                                    .arg(m_database.lastError().text());
        m_lastError = message;
        emit storageError(message);
        return;
    }

    for (int i = 0; i < count; ++i)
        m_pendingRecords.dequeue();

    m_lastError.clear();
    emit recordsWritten(count);
}

bool DataManager::queryCommunicationHistory(
    const QDateTime &start,
    const QDateTime &end,
    const QString &deviceId,
    const QString &direction,
    int offset,
    int limit,
    QList<CommunicationHistoryRecord> &records,
    int &totalCount,
    QString *errorMessage)
{
    records.clear();
    totalCount = 0;

    if (!isReady())
        return setError(QStringLiteral("历史数据库尚未就绪：%1").arg(m_lastError),
                        errorMessage);
    if (!start.isValid() || !end.isValid() || start > end)
        return setError(QStringLiteral("查询时间范围无效。"), errorMessage);

    flushPendingRecords();

    QString where = QStringLiteral(" WHERE timestamp_ms >= :start AND timestamp_ms <= :end");
    const QString normalizedDevice = deviceId.trimmed();
    if (!normalizedDevice.isEmpty())
        where += QStringLiteral(" AND device_id = :device");
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        where += QStringLiteral(" AND direction = :direction");

    QSqlQuery countQuery(m_database);
    countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM communication_history") + where);
    countQuery.bindValue(QStringLiteral(":start"), start.toMSecsSinceEpoch());
    countQuery.bindValue(QStringLiteral(":end"), end.toMSecsSinceEpoch());
    if (!normalizedDevice.isEmpty())
        countQuery.bindValue(QStringLiteral(":device"), normalizedDevice);
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        countQuery.bindValue(QStringLiteral(":direction"), direction);

    if (!countQuery.exec() || !countQuery.next())
        return setError(QStringLiteral("统计历史数据失败：%1")
                            .arg(countQuery.lastError().text()), errorMessage);
    totalCount = countQuery.value(0).toInt();

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp_ms, device_id, direction, payload "
        "FROM communication_history") + where
        + QStringLiteral(" ORDER BY timestamp_ms DESC, id DESC LIMIT :limit OFFSET :offset"));
    query.bindValue(QStringLiteral(":start"), start.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":end"), end.toMSecsSinceEpoch());
    if (!normalizedDevice.isEmpty())
        query.bindValue(QStringLiteral(":device"), normalizedDevice);
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        query.bindValue(QStringLiteral(":direction"), direction);
    query.bindValue(QStringLiteral(":limit"), qMax(1, limit));
    query.bindValue(QStringLiteral(":offset"), qMax(0, offset));

    if (!query.exec())
        return setError(QStringLiteral("查询历史数据失败：%1")
                            .arg(query.lastError().text()), errorMessage);

    while (query.next()) {
        CommunicationHistoryRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong());
        record.deviceId = query.value(2).toString();
        record.direction = query.value(3).toString();
        record.payload = query.value(4).toByteArray();
        records.append(record);
    }

    m_lastError.clear();
    return true;
}

bool DataManager::setError(const QString &message, QString *errorMessage)
{
    m_lastError = message;
    if (errorMessage)
        *errorMessage = message;
    return false;
}
