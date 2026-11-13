#include "usermanagementpage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

UserManagementPage::UserManagementPage(AuthManager *authManager, QWidget *parent)
    : QWidget(parent), m_authManager(authManager)
{
    auto *layout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("新增用户"), this);
    auto *role = new QPushButton(QStringLiteral("应用角色"), this);
    auto *toggle = new QPushButton(QStringLiteral("启用/禁用用户"), this);
    auto *refresh = new QPushButton(QStringLiteral("刷新"), this);
    toolbar->addWidget(add);
    toolbar->addWidget(role);
    toolbar->addWidget(toggle);
    toolbar->addStretch();
    toolbar->addWidget(refresh);
    layout->addLayout(toolbar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({QStringLiteral("用户编号"), QStringLiteral("用户名"),
        QStringLiteral("显示名称"), QStringLiteral("角色"), QStringLiteral("状态"),
        QStringLiteral("首次登录改密")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table);

    connect(add, &QPushButton::clicked, this, &UserManagementPage::addUser);
    connect(role, &QPushButton::clicked, this, &UserManagementPage::updateSelectedRole);
    connect(toggle, &QPushButton::clicked, this, &UserManagementPage::toggleSelectedUser);
    connect(refresh, &QPushButton::clicked, this, &UserManagementPage::refreshUsers);
    if (m_authManager)
        connect(m_authManager, &AuthManager::usersChanged, this, &UserManagementPage::refreshUsers);
    refreshUsers();
}

void UserManagementPage::refreshUsers()
{
    if (!m_authManager)
        return;
    QString error;
    const QList<AuthUser> users = m_authManager->users(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("用户列表"), error);
        return;
    }
    m_table->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row) {
        const AuthUser &user = users.at(row);
        auto *id = new QTableWidgetItem(QString::number(user.id));
        id->setData(Qt::UserRole, user.id);
        m_table->setItem(row, 0, id);
        m_table->setItem(row, 1, new QTableWidgetItem(user.username));
        m_table->setItem(row, 2, new QTableWidgetItem(user.displayName));
        m_table->setItem(row, 3, new QTableWidgetItem(user.role));
        m_table->setItem(row, 4, new QTableWidgetItem(user.enabled ? QStringLiteral("启用") : QStringLiteral("禁用")));
        m_table->setItem(row, 5, new QTableWidgetItem(user.mustChangePassword ? QStringLiteral("是") : QStringLiteral("否")));
    }
    m_table->resizeColumnsToContents();
}

void UserManagementPage::addUser()
{
    if (!m_authManager || !m_authManager->hasPermission(QStringLiteral("users.manage")))
        return;
    bool ok = false;
    const QString username = QInputDialog::getText(this, QStringLiteral("新增用户"),
        QStringLiteral("用户名（至少 3 个字符）："), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok) return;
    const QString displayName = QInputDialog::getText(this, QStringLiteral("新增用户"),
        QStringLiteral("显示名称："), QLineEdit::Normal, username, &ok).trimmed();
    if (!ok) return;
    const QString password = QInputDialog::getText(this, QStringLiteral("初始密码"),
        QStringLiteral("初始密码（至少 10 个字符）："), QLineEdit::Password, QString(), &ok);
    if (!ok) return;
    const QString role = QInputDialog::getItem(this, QStringLiteral("选择角色"),
        QStringLiteral("角色："), {QStringLiteral("viewer"), QStringLiteral("operator"), QStringLiteral("admin")},
        0, false, &ok);
    if (!ok) return;
    QString error;
    if (!m_authManager->addUser(username, displayName, password, role, &error))
        QMessageBox::warning(this, QStringLiteral("新增用户失败"), error);
}

void UserManagementPage::updateSelectedRole()
{
    if (!m_authManager || !m_authManager->hasPermission(QStringLiteral("users.manage")))
        return;
    const int row = m_table->currentRow();
    if (row < 0) return;
    const qint64 id = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    bool ok = false;
    const QString role = QInputDialog::getItem(this, QStringLiteral("修改角色"),
        QStringLiteral("新角色："), {QStringLiteral("viewer"), QStringLiteral("operator"), QStringLiteral("admin")},
        0, false, &ok);
    if (!ok) return;
    QString error;
    if (!m_authManager->setUserRole(id, role, &error))
        QMessageBox::warning(this, QStringLiteral("修改角色失败"), error);
}

void UserManagementPage::toggleSelectedUser()
{
    if (!m_authManager || !m_authManager->hasPermission(QStringLiteral("users.manage")))
        return;
    const int row = m_table->currentRow();
    if (row < 0) return;
    const qint64 id = m_table->item(row, 0)->data(Qt::UserRole).toLongLong();
    const bool currentlyEnabled = m_table->item(row, 4)->text() == QStringLiteral("启用");
    QString error;
    if (!m_authManager->setUserEnabled(id, !currentlyEnabled, &error))
        QMessageBox::warning(this, QStringLiteral("更新用户状态失败"), error);
}
