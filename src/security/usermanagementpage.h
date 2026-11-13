#ifndef USERMANAGEMENTPAGE_H
#define USERMANAGEMENTPAGE_H

#include <QWidget>
#include "src/security/authmanager.h"

class QTableWidget;

class UserManagementPage : public QWidget
{
    Q_OBJECT
public:
    explicit UserManagementPage(AuthManager *authManager, QWidget *parent = nullptr);

private slots:
    void refreshUsers();
    void addUser();
    void updateSelectedRole();
    void toggleSelectedUser();

private:
    AuthManager *m_authManager = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif
