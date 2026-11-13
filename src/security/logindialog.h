#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include "src/security/authmanager.h"

class QLineEdit;
class QLabel;

class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(AuthManager *authManager, QWidget *parent = nullptr);
    AuthUser authenticatedUser() const;

private slots:
    void tryLogin();

private:
    AuthManager *m_authManager = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLabel *m_messageLabel = nullptr;
    AuthUser m_user;
};

#endif
