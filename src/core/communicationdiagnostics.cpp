#include "communicationdiagnostics.h"
#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

CommunicationDiagnostics::CommunicationDiagnostics(CommunicationManager *manager,
                                                     QObject *parent)
    : QObject(parent)
{
    m_connectionName = QStringLiteral("IndustrialHubDiagnostics_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty())
        directory = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    if (!QDir().mkpath(directory)) {
        m_lastError = QStringLiteral("无法创建通信诊断数据目录：%1").arg(directory);
        return;
    }
    m_databasePath = QDir(directory).filePath("industrialhub.db");
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    m_database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!initialize())
        return;

    if (manager) {
        connect(manager, &CommunicationManager::deviceConnectionStateChanged, this,
                [this](const QString &id, bool connected) {
            record(id, connected ? QStringLiteral("连接") : QStringLiteral("断开"),
                   connected ? QStringLiteral("Info") : QStringLiteral("Warning"),
                   connected ? QStringLiteral("通信驱动连接成功") : QStringLiteral("通信连接已断开"));
        });
        connect(manager, &CommunicationManager::deviceDataSent, this,
                [this](const QString &id, const QByteArray &data) {
            record(id, QStringLiteral("TX"), QStringLiteral("Info"),
                   QStringLiteral("发送成功（驱动已接受），%1 字节").arg(data.size()),
                   data.size(), 0);
        });
        connect(manager, &CommunicationManager::deviceDataReceived, this,
                [this](const QString &id, const QByteArray &data) {
            record(id, QStringLiteral("RX"), QStringLiteral("Info"),
                   QStringLiteral("接收数据，%1 字节").arg(data.size()), 0, data.size());
        });
        connect(manager, &CommunicationManager::deviceErrorOccurred, this,
                [this](const QString &id, const QString &message) {
            record(id, classifyError(message), QStringLiteral("Error"), message);
        });
        connect(manager, &CommunicationManager::deviceReconnecting, this,
                [this](const QString &id, int attempt, int delay) {
            record(id, QStringLiteral("重连"), QStringLiteral("Warning"),
                   QStringLiteral("第 %1 次重连，%2 ms 后尝试").arg(attempt).arg(delay));
        });
    }
}

CommunicationDiagnostics::~CommunicationDiagnostics()
{
    if (m_database.isValid()) {
        m_database.close();
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool CommunicationDiagnostics::isReady() const
{
    return m_ready && m_database.isValid() && m_database.isOpen();
}

QString CommunicationDiagnostics::lastError() const { return m_lastError; }

bool CommunicationDiagnostics::initialize()
{
    if (!m_database.open()) {
        m_lastError = QStringLiteral("打开通信诊断数据库失败：%1").arg(m_database.lastError().text());
        return false;
    }
    QSqlQuery q(m_database);
    if (!q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS communication_diagnostics ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp_ms INTEGER NOT NULL,"
        "device_id TEXT NOT NULL COLLATE NOCASE, category TEXT NOT NULL,"
        "severity TEXT NOT NULL, message TEXT NOT NULL,"
        "tx_bytes INTEGER NOT NULL DEFAULT 0, rx_bytes INTEGER NOT NULL DEFAULT 0)"))) {
        m_lastError = QStringLiteral("创建通信诊断表失败：%1").arg(q.lastError().text());
        return false;
    }
    if (!q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_diagnostics_device_time "
        "ON communication_diagnostics(device_id, timestamp_ms DESC, id DESC)"))) {
        m_lastError = QStringLiteral("创建通信诊断索引失败：%1").arg(q.lastError().text());
        return false;
    }
    m_ready = true;
    m_lastError.clear();
    return true;
}

void CommunicationDiagnostics::record(const QString &deviceId, const QString &category,
                                       const QString &severity, const QString &message,
                                       qint64 txBytes, qint64 rxBytes)
{
    if (!isReady()) return;
    QSqlQuery q(m_database);
    q.prepare(QStringLiteral(
        "INSERT INTO communication_diagnostics "
        "(timestamp_ms,device_id,category,severity,message,tx_bytes,rx_bytes) "
        "VALUES(?,?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toMSecsSinceEpoch());
    q.addBindValue(deviceId.trimmed().isEmpty() ? QStringLiteral("(系统)") : deviceId.trimmed());
    q.addBindValue(category);
    q.addBindValue(severity);
    q.addBindValue(message.left(2048));
    q.addBindValue(txBytes);
    q.addBindValue(rxBytes);
    if (!q.exec()) {
        m_lastError = QStringLiteral("写入通信诊断事件失败：%1").arg(q.lastError().text());
        emit storageError(m_lastError);
        return;
    }
    emit diagnosticsChanged();
}

QString CommunicationDiagnostics::classifyError(const QString &message)
{
    const QString text = message.toCaseFolded();
    if (text.contains(QStringLiteral("timeout")) || text.contains(QStringLiteral("超时")))
        return QStringLiteral("超时");
    if (text.contains(QStringLiteral("serial")) || text.contains(QStringLiteral("串口"))
        || text.contains(QStringLiteral("port")))
        return QStringLiteral("端口错误");
    if (text.contains(QStringLiteral("socket")) || text.contains(QStringLiteral("tcp"))
        || text.contains(QStringLiteral("network")) || text.contains(QStringLiteral("连接")))
        return QStringLiteral("网络错误");
    if (text.contains(QStringLiteral("queue")) || text.contains(QStringLiteral("队列"))
        || text.contains(QStringLiteral("发送")))
        return QStringLiteral("发送错误");
    return QStringLiteral("通信错误");
}

QList<CommunicationDiagnosticEvent> CommunicationDiagnostics::recentEvents(
    int limit, const QString &deviceId, const QDateTime &start, const QDateTime &end) const
{
    QList<CommunicationDiagnosticEvent> result;
    if (!isReady()) return result;
    QSqlQuery q(m_database);
    QString where;
    if (!deviceId.trimmed().isEmpty()) where += QStringLiteral("device_id=?");
    if (start.isValid()) where += (where.isEmpty() ? QString() : QStringLiteral(" AND ")) + QStringLiteral("timestamp_ms>=?");
    if (end.isValid()) where += (where.isEmpty() ? QString() : QStringLiteral(" AND ")) + QStringLiteral("timestamp_ms<=?");
    q.prepare(QStringLiteral(
        "SELECT id,timestamp_ms,device_id,category,severity,message "
        "FROM communication_diagnostics %1 ORDER BY timestamp_ms DESC,id DESC LIMIT ?")
        .arg(where.isEmpty() ? QString() : QStringLiteral("WHERE ") + where));
    int index = 0;
    if (!deviceId.trimmed().isEmpty()) q.bindValue(index++, deviceId.trimmed());
    if (start.isValid()) q.bindValue(index++, start.toMSecsSinceEpoch());
    if (end.isValid()) q.bindValue(index++, end.toMSecsSinceEpoch());
    q.bindValue(index, qBound(1, limit, 1000));
    if (!q.exec()) return result;
    while (q.next()) {
        CommunicationDiagnosticEvent e;
        e.id=q.value(0).toLongLong();
        e.timestamp=QDateTime::fromMSecsSinceEpoch(q.value(1).toLongLong());
        e.deviceId=q.value(2).toString();
        e.category=q.value(3).toString();
        e.severity=q.value(4).toString();
        e.message=q.value(5).toString();
        result.append(e);
    }
    return result;
}

QList<CommunicationDeviceStatistics> CommunicationDiagnostics::statistics() const
{
    QList<CommunicationDeviceStatistics> result;
    if (!isReady()) return result;
    QSqlQuery q(m_database);
    if (!q.exec(QStringLiteral(
        "SELECT device_id,"
        "SUM(CASE WHEN category='TX' THEN 1 ELSE 0 END),"
        "SUM(CASE WHEN category='RX' THEN 1 ELSE 0 END),"
        "SUM(tx_bytes),SUM(rx_bytes),"
        "SUM(CASE WHEN severity='Error' THEN 1 ELSE 0 END),"
        "SUM(CASE WHEN category='重连' THEN 1 ELSE 0 END),MAX(timestamp_ms) "
        "FROM communication_diagnostics GROUP BY device_id ORDER BY device_id COLLATE NOCASE")))
        return result;
    while(q.next()) {
        CommunicationDeviceStatistics s;
        s.deviceId=q.value(0).toString(); s.txCount=q.value(1).toLongLong();
        s.rxCount=q.value(2).toLongLong(); s.txBytes=q.value(3).toLongLong();
        s.rxBytes=q.value(4).toLongLong(); s.errorCount=q.value(5).toLongLong();
        s.reconnectCount=q.value(6).toLongLong();
        s.lastEvent=QDateTime::fromMSecsSinceEpoch(q.value(7).toLongLong());
        result.append(s);
    }
    return result;
}

CommunicationDeviceStatistics CommunicationDiagnostics::statisticsFor(const QString &deviceId) const
{
    for (const auto &s : statistics())
        if (s.deviceId.compare(deviceId, Qt::CaseInsensitive)==0) return s;
    CommunicationDeviceStatistics s; s.deviceId=deviceId; return s;
}

void CommunicationDiagnostics::recordHealthTimeout(const QString &deviceId, qint64 silenceMs)
{
    record(deviceId, QStringLiteral("超时"), QStringLiteral("Warning"),
           QStringLiteral("设备接收超时，连续静默 %1 ms").arg(silenceMs));
}
