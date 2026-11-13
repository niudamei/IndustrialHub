#include "authmanager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString nowText()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}
}

AuthManager::AuthManager(const QString &databasePath, QObject *parent)
    : QObject(parent)
{
    m_connectionName = QStringLiteral("IndustrialHubAuth_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(databasePath);
    if (!initialize(&m_lastError))
        return;
    m_ready = true;
}

AuthManager::~AuthManager()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool AuthManager::initialize(QString *error)
{
    if (!m_db.open())
        return setError(QStringLiteral("打开权限数据库失败：%1").arg(m_db.lastError().text()), error);
    QSqlQuery q(m_db);
    const QStringList schema = {
        QStringLiteral("PRAGMA busy_timeout=10000"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS auth_roles (role TEXT PRIMARY KEY COLLATE NOCASE, description TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS auth_users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT NOT NULL UNIQUE COLLATE NOCASE, display_name TEXT NOT NULL, salt BLOB NOT NULL, password_hash BLOB NOT NULL, role TEXT NOT NULL REFERENCES auth_roles(role), enabled INTEGER NOT NULL DEFAULT 1, must_change_password INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS auth_permissions (permission TEXT PRIMARY KEY, description TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS auth_role_permissions (role TEXT NOT NULL REFERENCES auth_roles(role) ON DELETE CASCADE, permission TEXT NOT NULL REFERENCES auth_permissions(permission) ON DELETE CASCADE, PRIMARY KEY(role, permission))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS auth_audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp TEXT NOT NULL, username TEXT NOT NULL, action TEXT NOT NULL, target TEXT NOT NULL DEFAULT '', detail TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_auth_audit_timestamp ON auth_audit_log(timestamp)")
    };
    for (const QString &sql : schema) {
        if (!q.exec(sql))
            return setError(QStringLiteral("初始化权限表失败：%1").arg(q.lastError().text()), error);
    }

    const QList<QPair<QString, QString>> roles = {
        {QStringLiteral("admin"), QStringLiteral("系统管理员")},
        {QStringLiteral("operator"), QStringLiteral("操作员")},
        {QStringLiteral("viewer"), QStringLiteral("只读用户")}
    };
    for (const auto &role : roles) {
        q.prepare(QStringLiteral("INSERT OR IGNORE INTO auth_roles(role,description) VALUES(?,?)"));
        q.addBindValue(role.first);
        q.addBindValue(role.second);
        if (!q.exec())
            return setError(q.lastError().text(), error);
    }

    const QList<QPair<QString, QString>> permissions = {
        {QStringLiteral("dashboard.view"), QStringLiteral("查看运行总览")},
        {QStringLiteral("device.view"), QStringLiteral("查看设备")},
        {QStringLiteral("device.manage"), QStringLiteral("管理设备")},
        {QStringLiteral("device.operate"), QStringLiteral("操作设备与模拟设备")},
        {QStringLiteral("monitor.view"), QStringLiteral("查看实时监控")},
        {QStringLiteral("trend.view"), QStringLiteral("查看实时趋势")},
        {QStringLiteral("alarm.view"), QStringLiteral("查看报警")},
        {QStringLiteral("alarm.manage"), QStringLiteral("管理报警规则")},
        {QStringLiteral("alarm.acknowledge"), QStringLiteral("确认报警")},
        {QStringLiteral("history.view"), QStringLiteral("查看历史数据")},
        {QStringLiteral("history.maintenance"), QStringLiteral("维护/清理历史数据")},
        {QStringLiteral("communication.view"), QStringLiteral("查看通信状态")},
        {QStringLiteral("communication.manage"), QStringLiteral("管理通信配置")},
        {QStringLiteral("remote.manage"), QStringLiteral("管理 MQTT/WebSocket")},
        {QStringLiteral("system.manage"), QStringLiteral("管理系统设置")},
        {QStringLiteral("users.manage"), QStringLiteral("管理用户和角色")}
    };
    for (const auto &permission : permissions) {
        q.prepare(QStringLiteral("INSERT OR IGNORE INTO auth_permissions(permission,description) VALUES(?,?)"));
        q.addBindValue(permission.first);
        q.addBindValue(permission.second);
        if (!q.exec())
            return setError(q.lastError().text(), error);
    }

    const QStringList allPermissions = {
        QStringLiteral("dashboard.view"), QStringLiteral("device.view"),
        QStringLiteral("device.manage"), QStringLiteral("device.operate"), QStringLiteral("monitor.view"),
        QStringLiteral("trend.view"), QStringLiteral("alarm.view"),
        QStringLiteral("alarm.manage"), QStringLiteral("alarm.acknowledge"), QStringLiteral("history.view"),
        QStringLiteral("history.maintenance"), QStringLiteral("communication.view"),
        QStringLiteral("communication.manage"), QStringLiteral("remote.manage"),
        QStringLiteral("system.manage"), QStringLiteral("users.manage"),
        QStringLiteral("plugins.manage")
    };
    const QStringList operatorPermissions = {
        QStringLiteral("dashboard.view"), QStringLiteral("device.view"),
        QStringLiteral("device.operate"), QStringLiteral("monitor.view"), QStringLiteral("trend.view"),
        QStringLiteral("alarm.view"), QStringLiteral("alarm.acknowledge"),
        QStringLiteral("history.view"), QStringLiteral("communication.view"),
        QStringLiteral("communication.manage"), QStringLiteral("remote.manage")
    };
    const QStringList viewerPermissions = {
        QStringLiteral("dashboard.view"), QStringLiteral("device.view"),
        QStringLiteral("monitor.view"), QStringLiteral("trend.view"),
        QStringLiteral("alarm.view"), QStringLiteral("history.view"),
        QStringLiteral("communication.view")
    };
    const QList<QPair<QString, QStringList>> roleGrants = {
        {QStringLiteral("admin"), allPermissions},
        {QStringLiteral("operator"), operatorPermissions},
        {QStringLiteral("viewer"), viewerPermissions}
    };
    for (const auto &grant : roleGrants) {
        for (const QString &permission : grant.second) {
            q.prepare(QStringLiteral("INSERT OR IGNORE INTO auth_role_permissions(role,permission) VALUES(?,?)"));
            q.addBindValue(grant.first);
            q.addBindValue(permission);
            if (!q.exec())
                return setError(q.lastError().text(), error);
        }
    }

    q.prepare(QStringLiteral("SELECT id FROM auth_users WHERE username='admin' COLLATE NOCASE"));
    if (!q.exec())
        return setError(q.lastError().text(), error);
    if (!q.next()) {
        const QByteArray salt = QUuid::createUuid().toRfc4122();
        q.prepare(QStringLiteral("INSERT INTO auth_users(username,display_name,salt,password_hash,role,enabled,must_change_password,created_at,updated_at) VALUES(?,?,?,?, 'admin',1,1,?,?)"));
        q.addBindValue(QStringLiteral("admin"));
        q.addBindValue(QStringLiteral("系统管理员"));
        q.addBindValue(salt);
        q.addBindValue(passwordHash(QByteArrayLiteral("admin123"), salt));
        q.addBindValue(nowText());
        q.addBindValue(nowText());
        if (!q.exec())
            return setError(QStringLiteral("创建初始管理员失败：%1").arg(q.lastError().text()), error);
    }
    return true;
}

bool AuthManager::isReady() const { return m_ready; }
QString AuthManager::lastError() const { return m_lastError; }
QString AuthManager::currentUsername() const { return m_currentUser.username; }
QString AuthManager::currentRole() const { return m_currentUser.role; }
AuthUser AuthManager::currentUser() const { return m_currentUser; }

bool AuthManager::authenticate(const QString &username, const QString &password,
                               AuthUser *user, QString *error)
{
    if (!m_ready)
        return setError(QStringLiteral("权限服务尚未就绪。"), error);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id,username,display_name,salt,password_hash,role,enabled,must_change_password,created_at FROM auth_users WHERE username=? COLLATE NOCASE"));
    q.addBindValue(username.trimmed());
    if (!q.exec() || !q.next())
        return setError(QStringLiteral("用户名或密码错误。"), error);
    if (q.value(6).toInt() == 0)
        return setError(QStringLiteral("该用户已被禁用，请联系管理员。"), error);
    const QByteArray salt = q.value(3).toByteArray();
    const QByteArray expected = q.value(4).toByteArray();
    const QByteArray actual = passwordHash(password.toUtf8(), salt);
    if (actual != expected)
        return setError(QStringLiteral("用户名或密码错误。"), error);

    AuthUser authenticatedUser;
    authenticatedUser.id = q.value(0).toLongLong();
    authenticatedUser.username = q.value(1).toString();
    authenticatedUser.displayName = q.value(2).toString();
    authenticatedUser.role = q.value(5).toString();
    authenticatedUser.enabled = true;
    authenticatedUser.mustChangePassword = q.value(7).toInt() != 0;
    authenticatedUser.createdAt = q.value(8).toString();
    m_currentUser = authenticatedUser;
    if (user)
        *user = authenticatedUser;
    audit(QStringLiteral("login"), authenticatedUser.username, QStringLiteral("登录成功"));
    emit authenticated(authenticatedUser);
    return true;
}

bool AuthManager::hasPermission(const QString &permission) const
{
    if (m_currentUser.id <= 0)
        return false;
    if (m_currentUser.role.compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0)
        return true;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM auth_role_permissions WHERE role=? COLLATE NOCASE AND permission=?"));
    q.addBindValue(m_currentUser.role);
    q.addBindValue(permission);
    return q.exec() && q.next();
}

void AuthManager::logout()
{
    if (m_currentUser.id <= 0)
        return;
    audit(QStringLiteral("logout"), m_currentUser.username);
    m_currentUser = AuthUser();
    emit loggedOut();
}

bool AuthManager::changePassword(const QString &username, const QString &oldPassword,
                                 const QString &newPassword, QString *error)
{
    if (newPassword.size() < 10)
        return setError(QStringLiteral("新密码至少需要 10 个字符。"), error);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id,salt,password_hash FROM auth_users WHERE username=? COLLATE NOCASE"));
    q.addBindValue(username.trimmed());
    if (!q.exec() || !q.next())
        return setError(QStringLiteral("用户不存在。"), error);
    const QByteArray salt = q.value(1).toByteArray();
    const QByteArray expected = q.value(2).toByteArray();
    const QByteArray actual = passwordHash(oldPassword.toUtf8(), salt);
    if (actual != expected)
        return setError(QStringLiteral("原密码不正确。"), error);
    const qint64 userId = q.value(0).toLongLong();
    const QByteArray newSalt = QUuid::createUuid().toRfc4122();
    q.prepare(QStringLiteral("UPDATE auth_users SET salt=?,password_hash=?,must_change_password=0,updated_at=? WHERE id=?"));
    q.addBindValue(newSalt);
    q.addBindValue(passwordHash(newPassword.toUtf8(), newSalt));
    q.addBindValue(nowText());
    q.addBindValue(userId);
    if (!q.exec())
        return setError(QStringLiteral("更新密码失败：%1").arg(q.lastError().text()), error);
    m_currentUser.mustChangePassword = false;
    audit(QStringLiteral("password.change"), username);
    return true;
}

bool AuthManager::addUser(const QString &username, const QString &displayName,
                          const QString &password, const QString &role, QString *error)
{
    const QString name = username.trimmed();
    if (name.size() < 3 || name.size() > 64 || password.size() < 10
        || displayName.trimmed().isEmpty())
        return setError(QStringLiteral("用户名至少 3 个字符，显示名称不能为空，初始密码至少 10 个字符。"), error);
    if (!roleExists(role))
        return setError(QStringLiteral("角色不存在。"), error);
    const QByteArray salt = QUuid::createUuid().toRfc4122();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO auth_users(username,display_name,salt,password_hash,role,enabled,must_change_password,created_at,updated_at) VALUES(?,?,?,?,?,1,1,?,?)"));
    q.addBindValue(name);
    q.addBindValue(displayName.trimmed());
    q.addBindValue(salt);
    q.addBindValue(passwordHash(password.toUtf8(), salt));
    q.addBindValue(role);
    q.addBindValue(nowText());
    q.addBindValue(nowText());
    if (!q.exec())
        return setError(QStringLiteral("创建用户失败：%1").arg(q.lastError().text()), error);
    audit(QStringLiteral("user.create"), name, role);
    emit usersChanged();
    return true;
}

bool AuthManager::setUserRole(qint64 userId, const QString &role, QString *error)
{
    if (!roleExists(role))
        return setError(QStringLiteral("角色不存在。"), error);
    if (userId == m_currentUser.id)
        return setError(QStringLiteral("不能在当前会话中修改自己的角色，请由另一名管理员操作。"), error);
    QSqlQuery current(m_db);
    current.prepare(QStringLiteral("SELECT role FROM auth_users WHERE id=?"));
    current.addBindValue(userId);
    if (!current.exec() || !current.next())
        return setError(QStringLiteral("用户不存在。"), error);
    if (current.value(0).toString().compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0
        && role.compare(QStringLiteral("admin"), Qt::CaseInsensitive) != 0) {
        QSqlQuery count(m_db);
        if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM auth_users WHERE role='admin' COLLATE NOCASE AND enabled=1"))
            || !count.next() || count.value(0).toInt() <= 1)
            return setError(QStringLiteral("系统至少需要保留一个启用状态的管理员账户。"), error);
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE auth_users SET role=?,updated_at=? WHERE id=?"));
    q.addBindValue(role);
    q.addBindValue(nowText());
    q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected() != 1)
        return setError(QStringLiteral("修改角色失败：%1").arg(q.lastError().text()), error);
    audit(QStringLiteral("user.role"), QString::number(userId), role);
    emit usersChanged();
    return true;
}

bool AuthManager::setUserEnabled(qint64 userId, bool enabled, QString *error)
{
    if (userId == m_currentUser.id && !enabled)
        return setError(QStringLiteral("不能禁用当前登录账户。"), error);
    if (!enabled) {
        QSqlQuery current(m_db);
        current.prepare(QStringLiteral("SELECT role FROM auth_users WHERE id=?"));
        current.addBindValue(userId);
        if (current.exec() && current.next()
            && current.value(0).toString().compare(QStringLiteral("admin"), Qt::CaseInsensitive) == 0) {
            QSqlQuery count(m_db);
            if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM auth_users WHERE role='admin' COLLATE NOCASE AND enabled=1"))
                || !count.next() || count.value(0).toInt() <= 1)
                return setError(QStringLiteral("不能禁用最后一个启用状态的管理员账户。"), error);
        }
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE auth_users SET enabled=?,updated_at=? WHERE id=?"));
    q.addBindValue(enabled ? 1 : 0);
    q.addBindValue(nowText());
    q.addBindValue(userId);
    if (!q.exec() || q.numRowsAffected() != 1)
        return setError(QStringLiteral("更新用户状态失败：%1").arg(q.lastError().text()), error);
    audit(QStringLiteral("user.enabled"), QString::number(userId), enabled ? QStringLiteral("enabled") : QStringLiteral("disabled"));
    emit usersChanged();
    return true;
}

QList<AuthUser> AuthManager::users(QString *error) const
{
    QList<AuthUser> result;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id,username,display_name,role,enabled,must_change_password,created_at FROM auth_users ORDER BY username COLLATE NOCASE"))) {
        if (error) *error = q.lastError().text();
        return result;
    }
    while (q.next()) {
        AuthUser u;
        u.id = q.value(0).toLongLong();
        u.username = q.value(1).toString();
        u.displayName = q.value(2).toString();
        u.role = q.value(3).toString();
        u.enabled = q.value(4).toInt() != 0;
        u.mustChangePassword = q.value(5).toInt() != 0;
        u.createdAt = q.value(6).toString();
        result.append(u);
    }
    return result;
}

QList<AuthAuditRecord> AuthManager::auditEntries(int limit, QString *error) const
{
    QList<AuthAuditRecord> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id,timestamp,username,action,target,detail FROM auth_audit_log ORDER BY id DESC LIMIT ?"));
    q.addBindValue(qBound(1, limit, 5000));
    if (!q.exec()) {
        if (error) *error = q.lastError().text();
        return result;
    }
    while (q.next()) {
        AuthAuditRecord record;
        record.id = q.value(0).toLongLong();
        record.timestamp = q.value(1).toString();
        record.username = q.value(2).toString();
        record.action = q.value(3).toString();
        record.target = q.value(4).toString();
        record.detail = q.value(5).toString();
        result.append(record);
    }
    return result;
}

void AuthManager::audit(const QString &action, const QString &target, const QString &detail)
{
    if (!m_db.isOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO auth_audit_log(timestamp,username,action,target,detail) VALUES(?,?,?,?,?)"));
    q.addBindValue(nowText());
    q.addBindValue(m_currentUser.username.isEmpty() ? QStringLiteral("system") : m_currentUser.username);
    q.addBindValue(action);
    q.addBindValue(target);
    q.addBindValue(detail);
    if (q.exec())
        emit auditWritten();
}

bool AuthManager::roleExists(const QString &role) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM auth_roles WHERE role=? COLLATE NOCASE"));
    q.addBindValue(role.trimmed());
    return q.exec() && q.next();
}

bool AuthManager::setError(const QString &message, QString *error)
{
    m_lastError = message;
    if (error) *error = message;
    return false;
}

QByteArray AuthManager::hmacSha256(const QByteArray &key, const QByteArray &message)
{
    QByteArray k = key;
    if (k.size() > 64)
        k = QCryptographicHash::hash(k, QCryptographicHash::Sha256);
    k = k.leftJustified(64, char(0), true);
    QByteArray inner(64, char(0x36)), outer(64, char(0x5c));
    for (int i = 0; i < 64; ++i) {
        inner[i] = char(static_cast<unsigned char>(inner.at(i)) ^ static_cast<unsigned char>(k.at(i)));
        outer[i] = char(static_cast<unsigned char>(outer.at(i)) ^ static_cast<unsigned char>(k.at(i)));
    }
    return QCryptographicHash::hash(outer + QCryptographicHash::hash(inner + message, QCryptographicHash::Sha256),
                                    QCryptographicHash::Sha256);
}

QByteArray AuthManager::passwordHash(const QByteArray &password, const QByteArray &salt)
{
    // PBKDF2-HMAC-SHA256, one 32-byte output block, 120,000 iterations.
    const QByteArray block = salt + QByteArray::fromHex("00000001");
    QByteArray u = hmacSha256(password, block);
    QByteArray result = u;
    for (int i = 1; i < 120000; ++i) {
        u = hmacSha256(password, u);
        for (int j = 0; j < result.size(); ++j)
            result[j] = char(static_cast<unsigned char>(result.at(j)) ^ static_cast<unsigned char>(u.at(j)));
    }
    return result;
}
