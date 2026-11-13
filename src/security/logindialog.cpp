#include "logindialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QInputDialog>

LoginDialog::LoginDialog(AuthManager *authManager, QWidget *parent)
    : QDialog(parent), m_authManager(authManager)
{
    setWindowTitle(QStringLiteral("IndustrialHub 用户登录"));
    setModal(true);
    setMinimumWidth(380);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(QStringLiteral("工业设备监控与智能运维平台"), this);
    QFont font = title->font();
    font.setPointSize(14);
    font.setBold(true);
    title->setFont(font);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *form = new QFormLayout;
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(QStringLiteral("请输入用户名"));
    m_usernameEdit->setText(QStringLiteral("admin"));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    form->addRow(QStringLiteral("用户名："), m_usernameEdit);
    form->addRow(QStringLiteral("密码："), m_passwordEdit);
    layout->addLayout(form);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setStyleSheet(QStringLiteral("color:#c0392b;"));
    layout->addWidget(m_messageLabel);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("退出"), this);
    auto *login = new QPushButton(QStringLiteral("登录"), this);
    login->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(login);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(login, &QPushButton::clicked, this, &LoginDialog::tryLogin);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);
}

AuthUser LoginDialog::authenticatedUser() const
{
    return m_user;
}

void LoginDialog::tryLogin()
{
    if (!m_authManager) {
        m_messageLabel->setText(QStringLiteral("权限服务不可用。"));
        return;
    }
    AuthUser user;
    QString error;
    const QString password = m_passwordEdit->text();
    if (!m_authManager->authenticate(m_usernameEdit->text(), password, &user, &error)) {
        m_messageLabel->setText(error);
        m_passwordEdit->selectAll();
        m_passwordEdit->setFocus();
        return;
    }

    if (user.mustChangePassword) {
        bool accepted = false;
        const QString newPassword = QInputDialog::getText(
            this, QStringLiteral("首次登录必须修改密码"),
            QStringLiteral("初始管理员密码为 admin123。请输入至少 10 个字符的新密码："),
            QLineEdit::Password, QString(), &accepted);
        if (!accepted) {
            m_authManager->logout();
            return;
        }
        const QString confirmation = QInputDialog::getText(
            this, QStringLiteral("确认新密码"), QStringLiteral("请再次输入新密码："),
            QLineEdit::Password, QString(), &accepted);
        if (!accepted) {
            m_authManager->logout();
            return;
        }
        if (newPassword != confirmation) {
            m_authManager->logout();
            m_messageLabel->setText(QStringLiteral("两次输入的新密码不一致，请重新登录。"));
            return;
        }
        if (!m_authManager->changePassword(user.username, password, newPassword, &error)) {
            m_authManager->logout();
            m_messageLabel->setText(error);
            return;
        }
        user.mustChangePassword = false;
    }
    m_user = user;
    accept();
}
