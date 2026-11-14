#include "pluginpage.h"

#include "pluginmanager.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

PluginPage::PluginPage(PluginManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    auto *root = new QVBoxLayout(this);
    auto *description = new QLabel(
        QStringLiteral("动态插件使用 Qt Plugin / QPluginLoader。只加载来自可信来源的插件；插件代码在当前进程内运行，拥有与主程序相同的权限。"),
        this);
    description->setWordWrap(true);
    root->addWidget(description);

    auto *loadRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(QStringLiteral("选择 .dll / .so / .dylib 插件文件"));
    auto *browse = new QPushButton(QStringLiteral("浏览…"), this);
    auto *load = new QPushButton(QStringLiteral("加载插件"), this);
    auto *unload = new QPushButton(QStringLiteral("卸载选中"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    loadRow->addWidget(m_pathEdit, 1);
    loadRow->addWidget(browse);
    loadRow->addWidget(load);
    loadRow->addWidget(unload);
    loadRow->addWidget(refreshButton);
    root->addLayout(loadRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({QStringLiteral("插件 ID"),
        QStringLiteral("名称"), QStringLiteral("版本"), QStringLiteral("通信类型"),
        QStringLiteral("动态库路径")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);
    root->addWidget(m_table, 1);

    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 IndustrialHub 插件"));
        if (!path.isEmpty()) m_pathEdit->setText(path);
    });
    connect(load, &QPushButton::clicked, this, &PluginPage::loadSelectedFile);
    connect(unload, &QPushButton::clicked, this, &PluginPage::unloadSelectedPlugin);
    connect(refreshButton, &QPushButton::clicked, this, &PluginPage::refresh);
    if (m_manager)
        connect(m_manager, &PluginManager::pluginsChanged, this, &PluginPage::refresh);
    refresh();
}

void PluginPage::loadSelectedFile()
{
    if (!m_manager) return;
    QString error;
    if (!m_manager->loadPlugin(m_pathEdit->text().trimmed(), &error)) {
        QMessageBox::warning(this, QStringLiteral("插件加载失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("插件加载成功"),
                             QStringLiteral("插件已加载并完成初始化。"));
}

void PluginPage::unloadSelectedPlugin()
{
    if (!m_manager || !m_table->currentItem()) return;
    const int row = m_table->currentRow();
    const QString id = m_table->item(row, 0)->text();
    if (QMessageBox::question(this, QStringLiteral("卸载插件"),
        QStringLiteral("确定卸载插件 %1？").arg(id)) != QMessageBox::Yes)
        return;
    QString error;
    if (!m_manager->unloadPlugin(id, &error))
        QMessageBox::warning(this, QStringLiteral("插件卸载失败"), error);
}

void PluginPage::refresh()
{
    if (!m_manager || !m_table) return;
    const QList<LoadedPluginInfo> plugins = m_manager->plugins();
    m_table->setRowCount(plugins.size());
    for (int row = 0; row < plugins.size(); ++row) {
        const LoadedPluginInfo &plugin = plugins.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(plugin.pluginId));
        m_table->setItem(row, 1, new QTableWidgetItem(plugin.displayName));
        m_table->setItem(row, 2, new QTableWidgetItem(plugin.version));
        m_table->setItem(row, 3, new QTableWidgetItem(
            plugin.communicationTypes.join(QStringLiteral(", "))));
        m_table->setItem(row, 4, new QTableWidgetItem(plugin.filePath));
    }
    m_table->resizeColumnsToContents();
}
