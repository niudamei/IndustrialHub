#include "src/security/authmanager.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    AuthManager auth(directory.filePath(QStringLiteral("auth.db")));
    if (!auth.isReady()) {
        qCritical() << auth.lastError();
        return 2;
    }

    AuthUser admin;
    QString error;
    if (!auth.authenticate(QStringLiteral("admin"), QStringLiteral("admin123"), &admin, &error)
        || !admin.mustChangePassword) {
        qCritical() << "Initial admin login failed:" << error;
        return 3;
    }
    if (!auth.changePassword(QStringLiteral("admin"), QStringLiteral("admin123"),
                             QStringLiteral("AdminPassword!2026"), &error)) {
        qCritical() << "Password change failed:" << error;
        return 4;
    }
    auth.logout();
    if (!auth.authenticate(QStringLiteral("admin"), QStringLiteral("AdminPassword!2026"),
                           &admin, &error) || admin.mustChangePassword
        || !auth.hasPermission(QStringLiteral("users.manage"))
        || !auth.hasPermission(QStringLiteral("plugins.manage"))) {
        qCritical() << "Admin role permissions failed:" << error;
        return 5;
    }

    if (auth.setUserRole(admin.id, QStringLiteral("viewer"), &error)) {
        qCritical() << "Self role change should be rejected.";
        return 11;
    }

    if (!auth.addUser(QStringLiteral("readuser"), QStringLiteral("只读用户"),
                      QStringLiteral("ReadOnlyPass!2026"), QStringLiteral("viewer"), &error)) {
        qCritical() << "Viewer creation failed:" << error;
        return 6;
    }
    auth.logout();
    AuthUser viewer;
    if (!auth.authenticate(QStringLiteral("readuser"), QStringLiteral("ReadOnlyPass!2026"),
                           &viewer, &error) || !auth.hasPermission(QStringLiteral("dashboard.view"))
        || auth.hasPermission(QStringLiteral("device.manage"))
        || auth.hasPermission(QStringLiteral("users.manage"))
        || auth.hasPermission(QStringLiteral("alarm.manage"))
        || auth.hasPermission(QStringLiteral("plugins.manage"))) {
        qCritical() << "Viewer permission boundary failed:" << error;
        return 7;
    }

    auth.logout();
    if (!auth.authenticate(QStringLiteral("admin"), QStringLiteral("AdminPassword!2026"),
                           &admin, &error)) return 8;
    if (!auth.setUserEnabled(viewer.id, false, &error)) {
        qCritical() << "Disable viewer failed:" << error;
        return 9;
    }
    auth.logout();
    if (auth.authenticate(QStringLiteral("readuser"), QStringLiteral("ReadOnlyPass!2026"),
                          &viewer, &error)) {
        qCritical() << "Disabled account unexpectedly authenticated.";
        return 10;
    }

    qInfo() << "PASS: auth database, forced password change, PBKDF2 password, roles, permissions and disabled-user checks.";
    return 0;
}
