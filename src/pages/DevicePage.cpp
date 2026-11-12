#include "devicepage.h"
#include "devicedialog.h"
#include "ui_devicepage.h"
#include <QLabel>
#include <QString>

DevicePage::DevicePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DevicePage)
{
    ui->setupUi(this);
    initTable();

    // 状态下拉初始化
    ui->cmbStatus->addItem("全部");
    ui->cmbStatus->addItem("在线");
    ui->cmbStatus->addItem("离线");
    ui->cmbStatus->addItem("故障");

    // 模拟初始测试数据，你后面可以对接数据库
    m_allDevList.append(DeviceInfo(
        "DEV001", "导航控制器", "控制器",
        "TCP", "192.168.2.125:5600", DeviceStatus::Online
        ));

    m_allDevList.append(DeviceInfo(
        "DEV002", "CAN采集盒", "采集设备",
        "CAN", "0x06069001", DeviceStatus::Offline
        ));
    filterData();
}

DevicePage::~DevicePage()
{
    delete ui;
}
void DevicePage::initTable()
{
    QStringList headers = {"设备编号","设备名称","设备类型","通信类型","设备地址","当前状态","添加时间"};
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(headers);
    ui->DeviceView->setModel(m_model);

    // 样式适配，和平台统一
    ui->DeviceView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->DeviceView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->DeviceView->horizontalHeader()->setStretchLastSection(true);
    ui->DeviceView->verticalHeader()->setVisible(false);
}

void DevicePage::loadTableData(const QList<DeviceInfo> &list)
{
    m_model->removeRows(0, m_model->rowCount());

    for (const auto &dev : list)
    {
        QList<QStandardItem*> row;

        row << new QStandardItem(dev.devId);
        row << new QStandardItem(dev.devName);
        row << new QStandardItem(dev.devType);
        row << new QStandardItem(dev.commType);
        row << new QStandardItem(dev.devAddr);

        // 枚举状态转换成中文
        QString statusText;
        switch (dev.status)
        {
        case DeviceStatus::Online:
            statusText = "在线";
            break;
        case DeviceStatus::Offline:
            statusText = "离线";
            break;
        case DeviceStatus::Fault:
            statusText = "故障";
            break;
        }

        row << new QStandardItem(statusText);

        // QDateTime 转换成字符串
        row << new QStandardItem(
            dev.addTime.toString("yyyy-MM-dd HH:mm:ss"));

        m_model->appendRow(row);
    }
}

void DevicePage::filterData()
{
    QString key = ui->edtSearch->text().trimmed().toLower();
    QString statusFilter = ui->cmbStatus->currentText();

    QList<DeviceInfo> res;

    for (const auto &d : m_allDevList)
    {
        // 状态筛选
        if (statusFilter != "全部")
        {
            if (statusFilter == "在线" && d.status != DeviceStatus::Online)
                continue;

            if (statusFilter == "离线" && d.status != DeviceStatus::Offline)
                continue;

            if (statusFilter == "故障" && d.status != DeviceStatus::Fault)
                continue;
        }

        // 设备名称模糊搜索
        if (!key.isEmpty() && !d.devName.toLower().contains(key))
            continue;

        res << d;
    }

    loadTableData(res);
}
int DevicePage::findRowById(const QString &id)
{
    for(int i=0; i<m_allDevList.size();i++)
    {
        if(m_allDevList[i].devId == id)
            return i;
    }
    return -1;
}

void DevicePage::on_btnAdd_clicked()
{
    DeviceDialog dlg(this);
    if(dlg.exec() == QDialog::Accepted)
    {
        DeviceInfo info = dlg.getInfo();
        m_allDevList.append(info);
        filterData();
    }
}

void DevicePage::on_btnEdit_clicked()
{
    auto sel = ui->DeviceView->selectionModel()->selectedRows();
    if(sel.isEmpty())
    {
        QMessageBox::warning(this,"提示","请先选中一行设备！");
        return;
    }
    int row = sel.first().row();
    QString devId = m_model->index(row,0).data().toString();
    int srcIdx = findRowById(devId);
    if(srcIdx <0) return;

    DeviceDialog dlg(this);
    dlg.setInfo(m_allDevList[srcIdx]);
    if(dlg.exec() == QDialog::Accepted)
    {
        m_allDevList[srcIdx] = dlg.getInfo();
        filterData();
    }
}

void DevicePage::on_btnDel_clicked()
{
    auto sel = ui->DeviceView->selectionModel()->selectedRows();
    if(sel.isEmpty())
    {
        QMessageBox::warning(this,"提示","请先选中一行设备！");
        return;
    }
    int ret = QMessageBox::question(this,"确认删除","确定删除该设备？此操作不可恢复！",QMessageBox::Yes|QMessageBox::No);
    if(ret != QMessageBox::Yes)
        return;

    int row = sel.first().row();
    QString devId = m_model->index(row,0).data().toString();
    int srcIdx = findRowById(devId);
    if(srcIdx >=0)
    {
        m_allDevList.removeAt(srcIdx);
        filterData();
    }
}

void DevicePage::on_btnRefresh_clicked()
{
    // 这里后面对接后端/读取真实数据，现在清空搜索筛选，重新加载
    ui->edtSearch->clear();
    ui->cmbStatus->setCurrentIndex(0);
    filterData();
    QMessageBox::information(this,"刷新成功","设备列表已刷新");
}

void DevicePage::on_edtSearch_textChanged(const QString &text)
{
    filterData();
}

void DevicePage::on_cmbStatus_currentIndexChanged(int index)
{
    filterData();
}