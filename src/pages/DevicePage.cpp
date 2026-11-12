#include "devicepage.h"
#include "devicedialog.h"
#include "devicemanager.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QStandardItem>

namespace {
QString statusText(DeviceStatus s) {
    switch (s) {
    case DeviceStatus::Online: return QStringLiteral("在线");
    case DeviceStatus::Offline: return QStringLiteral("离线");
    case DeviceStatus::Fault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}
}

DevicePage::DevicePage(DeviceManager *manager, QWidget *parent)
    : QWidget(parent), ui(new Ui::DevicePage), m_deviceManager(manager)
{
    ui->setupUi(this);
    initTable();
    ui->cmbStatus->addItems({QStringLiteral("全部"), QStringLiteral("在线"),
                             QStringLiteral("离线"), QStringLiteral("故障")});
    if (m_deviceManager)
        connect(m_deviceManager, &DeviceManager::devicesChanged, this, &DevicePage::refreshTable);
    refreshTable();
}
DevicePage::~DevicePage() { delete ui; }

void DevicePage::initTable()
{
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({QStringLiteral("设备编号"), QStringLiteral("设备名称"),
        QStringLiteral("设备类型"), QStringLiteral("通信类型"), QStringLiteral("设备地址"),
        QStringLiteral("当前状态"), QStringLiteral("添加时间")});
    ui->DeviceView->setModel(m_model);
    ui->DeviceView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->DeviceView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->DeviceView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->DeviceView->setAlternatingRowColors(true);
    ui->DeviceView->horizontalHeader()->setStretchLastSection(true);
    ui->DeviceView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->DeviceView->verticalHeader()->setVisible(false);
}
void DevicePage::refreshTable()
{
    if (m_deviceManager) loadTableData(m_deviceManager->getAllDevices());
    else m_model->removeRows(0, m_model->rowCount());
}
void DevicePage::loadTableData(const QList<DeviceInfo> &devices)
{
    m_model->removeRows(0, m_model->rowCount());
    for (const DeviceInfo &d : devices) {
        QList<QStandardItem*> row;
        row << new QStandardItem(d.devId) << new QStandardItem(d.devName)
            << new QStandardItem(d.devType) << new QStandardItem(d.commType)
            << new QStandardItem(d.devAddr) << new QStandardItem(statusText(d.status))
            << new QStandardItem(d.addTime.isValid() ? d.addTime.toString("yyyy-MM-dd HH:mm:ss") : QString());
        for (QStandardItem *item : row) item->setEditable(false);
        m_model->appendRow(row);
    }
    filterData();
}
void DevicePage::filterData()
{
    const QString key = ui->edtSearch->text().trimmed();
    const QString status = ui->cmbStatus->currentText();
    for (int r = 0; r < m_model->rowCount(); ++r) {
        const bool textMatch = key.isEmpty()
            || m_model->index(r,0).data().toString().contains(key, Qt::CaseInsensitive)
            || m_model->index(r,1).data().toString().contains(key, Qt::CaseInsensitive)
            || m_model->index(r,2).data().toString().contains(key, Qt::CaseInsensitive)
            || m_model->index(r,4).data().toString().contains(key, Qt::CaseInsensitive);
        const QString rowStatus = m_model->index(r,5).data().toString();
        const bool statusMatch = status.isEmpty() || status == QStringLiteral("全部") || rowStatus == status;
        ui->DeviceView->setRowHidden(r, !(textMatch && statusMatch));
    }
}
QString DevicePage::selectedDeviceId() const
{
    if (!ui->DeviceView->selectionModel()) return {};
    const auto rows = ui->DeviceView->selectionModel()->selectedRows();
    return rows.isEmpty() ? QString() : m_model->index(rows.first().row(),0).data().toString();
}
void DevicePage::on_btnAdd_clicked()
{
    if (!m_deviceManager) return;
    DeviceDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString error;
    if (!m_deviceManager->addDevice(dlg.getInfo(), &error))
        QMessageBox::warning(this, QStringLiteral("添加失败"), error);
}
void DevicePage::on_btnEdit_clicked()
{
    if (!m_deviceManager) return;
    const QString id = selectedDeviceId();
    if (id.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一行设备。")); return; }
    DeviceInfo d;
    if (!m_deviceManager->getDeviceById(id, d)) return;
    DeviceDialog dlg(this);
    dlg.setInfo(d);
    if (dlg.exec() != QDialog::Accepted) return;
    QString error;
    if (!m_deviceManager->updateDevice(dlg.getInfo(), &error))
        QMessageBox::warning(this, QStringLiteral("编辑失败"), error);
}
void DevicePage::on_btnDel_clicked()
{
    if (!m_deviceManager) return;
    const QString id = selectedDeviceId();
    if (id.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中一行设备。")); return; }
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
        QStringLiteral("确定删除设备 %1？此操作不可恢复。").arg(id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QString error;
    if (!m_deviceManager->removeDevice(id, &error))
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
}
void DevicePage::on_btnRefresh_clicked()
{
    ui->edtSearch->clear();
    ui->cmbStatus->setCurrentIndex(0);
    refreshTable();
}
void DevicePage::on_edtSearch_textChanged(const QString &text) { Q_UNUSED(text); filterData(); }
void DevicePage::on_cmbStatus_currentIndexChanged(int index) { Q_UNUSED(index); filterData(); }
