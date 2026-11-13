#include "datamanager.h"

#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>
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
    while (!m_pendingRecords.isEmpty()) {
        const int pendingBeforeFlush = m_pendingRecords.size();
        flushPendingRecords();
        if (m_pendingRecords.size() == pendingBeforeFlush)
            break;
    }
    while (!m_pendingBusinessData.isEmpty()) {
        const int pendingBeforeFlush = m_pendingBusinessData.size();
        flushPendingBusinessData();
        if (m_pendingBusinessData.size() == pendingBeforeFlush)
            break;
    }
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

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS business_data_history ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp_ms INTEGER NOT NULL,"
            "device_id TEXT NOT NULL COLLATE NOCASE,"
            "variable_id TEXT NOT NULL COLLATE NOCASE,"
            "variable_name TEXT NOT NULL,"
            "value REAL NOT NULL,"
            "unit TEXT NOT NULL,"
            "quality INTEGER NOT NULL,"
            "source_protocol TEXT NOT NULL)"))) {
        return setError(QStringLiteral("创建业务变量历史表失败：%1")
                            .arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_business_history_time "
            "ON business_data_history(timestamp_ms)"))) {
        return setError(QStringLiteral("创建业务变量时间索引失败：%1")
                            .arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_business_history_device_variable_time "
            "ON business_data_history(device_id, variable_id, timestamp_ms)"))) {
        return setError(QStringLiteral("创建业务变量设备索引失败：%1")
                            .arg(query.lastError().text()));
    }

    // 复合索引覆盖历史分页的时间排序，避免数据量增长后反复临时排序。
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_communication_history_time_id "
            "ON communication_history(timestamp_ms DESC, id DESC)"))) {
        return setError(QStringLiteral("创建通信历史分页索引失败：%1")
                            .arg(query.lastError().text()));
    }
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_business_history_time_id "
            "ON business_data_history(timestamp_ms DESC, id DESC)"))) {
        return setError(QStringLiteral("创建业务变量历史分页索引失败：%1")
                            .arg(query.lastError().text()));
    }

    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_communication_history_filter_page "
            "ON communication_history(device_id, direction, timestamp_ms DESC, id DESC)"))) {
        return setError(QStringLiteral("创建通信历史筛选索引失败：%1").arg(query.lastError().text()));
    }
    if (!query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_business_history_filter_page "
            "ON business_data_history(device_id, variable_id, timestamp_ms DESC, id DESC)"))) {
        return setError(QStringLiteral("创建业务历史筛选索引失败：%1").arg(query.lastError().text()));
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


void DataManager::appendBusinessDataPoints(
    const QString &deviceId,
    const QVector<BusinessDataPoint> &points)
{
    if (!isReady() || deviceId.trimmed().isEmpty() || points.isEmpty())
        return;

    for (const BusinessDataPoint &point : points) {
        if (point.deviceId.trimmed().isEmpty()
            || point.deviceId.compare(deviceId.trimmed(), Qt::CaseInsensitive) != 0
            || point.variableId.trimmed().isEmpty()
            || !point.timestamp.isValid()
            || !qIsFinite(point.value)) {
            continue;
        }

        if (m_pendingBusinessData.size() >= MaxPendingBusinessData) {
            m_pendingBusinessData.dequeue();
            emit storageError(QStringLiteral(
                "业务变量历史写入队列已满，已丢弃最旧的一条待写记录。"));
        }
        BusinessDataHistoryRecord record;
        record.point = point;
        m_pendingBusinessData.enqueue(record);
    }

    if (m_pendingBusinessData.size() >= BatchSize)
        flushPendingBusinessData();
}

void DataManager::flushPendingBusinessData()
{
    if (!isReady() || m_pendingBusinessData.isEmpty())
        return;

    const int count = qMin(BatchSize, m_pendingBusinessData.size());
    if (!m_database.transaction()) {
        const QString message = QStringLiteral("开启业务变量历史事务失败：%1")
                                    .arg(m_database.lastError().text());
        m_lastError = message;
        emit storageError(message);
        return;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO business_data_history "
        "(timestamp_ms, device_id, variable_id, variable_name, value, unit, quality, source_protocol) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    for (int i = 0; i < count; ++i) {
        const BusinessDataPoint &point = m_pendingBusinessData.at(i).point;
        query.bindValue(0, point.timestamp.toMSecsSinceEpoch());
        query.bindValue(1, point.deviceId.trimmed());
        query.bindValue(2, point.variableId.trimmed());
        query.bindValue(3, point.name);
        query.bindValue(4, point.value);
        query.bindValue(5, point.unit);
        query.bindValue(6, static_cast<int>(point.quality));
        query.bindValue(7, point.sourceProtocol);
        if (!query.exec()) {
            m_database.rollback();
            const QString message = QStringLiteral("写入业务变量历史失败：%1")
                                        .arg(query.lastError().text());
            m_lastError = message;
            emit storageError(message);
            return;
        }
        query.finish();
    }

    if (!m_database.commit()) {
        m_database.rollback();
        const QString message = QStringLiteral("提交业务变量历史事务失败：%1")
                                    .arg(m_database.lastError().text());
        m_lastError = message;
        emit storageError(message);
        return;
    }

    for (int i = 0; i < count; ++i)
        m_pendingBusinessData.dequeue();
    m_lastError.clear();
}

bool DataManager::queryBusinessDataHistory(
    const QDateTime &start,
    const QDateTime &end,
    const QString &deviceId,
    const QString &variableId,
    int offset,
    int limit,
    QList<BusinessDataHistoryRecord> &records,
    int &totalCount,
    QString *errorMessage,
    bool refreshTotalCount)
{
    records.clear();
    if (refreshTotalCount) totalCount = 0;
    if (!isReady())
        return setError(QStringLiteral("历史数据库尚未就绪：%1").arg(m_lastError), errorMessage);
    if (!start.isValid() || !end.isValid() || start > end)
        return setError(QStringLiteral("业务变量查询时间范围无效。"), errorMessage);

    // 查询只读取已经提交的记录，不在 UI 查询调用中同步清空整个写入队列。
    // 待写记录由 250ms 定时批处理；这样翻页耗时不会随队列积压量线性增长。

    QString where = QStringLiteral(" WHERE timestamp_ms >= ? AND timestamp_ms <= ?");
    if (!deviceId.trimmed().isEmpty())
        where += QStringLiteral(" AND device_id = ?");
    if (!variableId.trimmed().isEmpty())
        where += QStringLiteral(" AND variable_id = ?");

    if (refreshTotalCount) {
        QSqlQuery countQuery(m_database);
        countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM business_data_history") + where);
        int bindIndex = 0;
        countQuery.bindValue(bindIndex++, start.toMSecsSinceEpoch());
        countQuery.bindValue(bindIndex++, end.toMSecsSinceEpoch());
        if (!deviceId.trimmed().isEmpty())
            countQuery.bindValue(bindIndex++, deviceId.trimmed());
        if (!variableId.trimmed().isEmpty())
            countQuery.bindValue(bindIndex++, variableId.trimmed());
        if (!countQuery.exec() || !countQuery.next())
            return setError(QStringLiteral("统计业务变量历史失败：%1")
                                .arg(countQuery.lastError().text()), errorMessage);
        totalCount = countQuery.value(0).toInt();
    }


    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp_ms, device_id, variable_id, variable_name, value, unit, quality, source_protocol "
        "FROM business_data_history") + where
        + QStringLiteral(" ORDER BY timestamp_ms DESC, id DESC LIMIT ? OFFSET ?"));
    bindIndex = 0;
    query.bindValue(bindIndex++, start.toMSecsSinceEpoch());
    query.bindValue(bindIndex++, end.toMSecsSinceEpoch());
    if (!deviceId.trimmed().isEmpty())
        query.bindValue(bindIndex++, deviceId.trimmed());
    if (!variableId.trimmed().isEmpty())
        query.bindValue(bindIndex++, variableId.trimmed());
    query.bindValue(bindIndex++, qMax(1, limit));
    query.bindValue(bindIndex, qMax(0, offset));
    if (!query.exec())
        return setError(QStringLiteral("查询业务变量历史失败：%1")
                            .arg(query.lastError().text()), errorMessage);

    while (query.next()) {
        BusinessDataHistoryRecord record;
        record.id = query.value(0).toLongLong();
        record.point.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong());
        record.point.deviceId = query.value(2).toString();
        record.point.variableId = query.value(3).toString();
        record.point.name = query.value(4).toString();
        record.point.value = query.value(5).toDouble();
        record.point.unit = query.value(6).toString();
        record.point.quality = static_cast<DataQuality>(query.value(7).toInt());
        record.point.sourceProtocol = query.value(8).toString();
        records.append(record);
    }
    m_lastError.clear();
    return true;
}

void DataManager::flushPendingRecords()
{
    if (!isReady())
        return;
    if (m_pendingRecords.isEmpty()) {
        flushPendingBusinessData();
        return;
    }

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
    query.finish();

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
    flushPendingBusinessData();
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
    QString *errorMessage,
    bool refreshTotalCount)
{
    records.clear();
    if (refreshTotalCount) totalCount = 0;

    if (!isReady())
        return setError(QStringLiteral("历史数据库尚未就绪：%1").arg(m_lastError),
                        errorMessage);
    if (!start.isValid() || !end.isValid() || start > end)
        return setError(QStringLiteral("查询时间范围无效。"), errorMessage);

    // 查询不再同步排空待写队列；由定时批处理负责持久化，避免翻页阻塞 UI。

    QString where = QStringLiteral(" WHERE timestamp_ms >= :start AND timestamp_ms <= :end");
    const QString normalizedDevice = deviceId.trimmed();
    if (!normalizedDevice.isEmpty())
        where += QStringLiteral(" AND device_id = :device");
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        where += QStringLiteral(" AND direction = :direction");

    if (refreshTotalCount) {
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
    }


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
