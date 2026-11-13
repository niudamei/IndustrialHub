#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QSqlDatabase>
#include <QStringList>

struct AuthUser
{
    qint64 id = 0;
    QString username;
    QString displayName;
    QString role;
    bool enabled = true;
    bool mustChangePassword = false;
    QString createdAt;
};

class AuthManager : public QObject
{
    Q_OBJECT
public:
    explicit AuthManager(const QString &databasePath, QObject *parent = nullptr);
    ~AuthManager() override;
    bool isReady() const;
    QString lastError() const;
    bool authenticate(const QString &username, const QString &password,
                      AuthUser *user, QString *error = nullptr);
    bool hasPermission(const QString &permission) const;
    QString currentUsername() const;
    QString currentRole() const;
    AuthUser currentUser() const;
    void logout();
    bool changePassword(const QString &username, const QString &oldPassword,
                        const QString &newPassword, QString *error = nullptr);
    bool addUser(const QString &username, const QString &displayName,
                 const QString &password, const QString &role,
                 QString *error = nullptr);
    bool setUserRole(qint64 userId, const QString &role, QString *error = nullptr);
    bool setUserEnabled(qint64 userId, bool enabled, QString *error = nullptr);
    QList<AuthUser> users(QString *error = nullptr) const;
    void audit(const QString &action, const QString &target = QString(),
               const QString &detail = QString());

signals:
    void authenticated(const AuthUser &user);
    void loggedOut();
    void usersChanged();
    void auditWritten();

private:
    bool initialize(QString *error);
    bool setError(const QString &message, QString *error = nullptr);
    static QByteArray passwordHash(const QByteArray &password, const QByteArray &salt);
    static QByteArray hmacSha256(const QByteArray &key, const QByteArray &message);
    bool roleExists(const QString &role) const;
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_lastError;
    AuthUser m_currentUser;
    bool m_ready = false;
};

#endif
