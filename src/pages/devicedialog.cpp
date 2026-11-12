#include "devicedialog.h"
#include "ui_devicedialog.h"
#include <QDateTime>
#include <QMessageBox>
#include <QComboBox>

DeviceDialog::DeviceDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::DeviceDialog)
{
    ui->setupUi(this);
    // 通信类型下拉
    ui->cmbCommType_C->addItems({"TCP","UDP","CAN","RS485","Modbus"});
    // 状态下拉
    ui->cmbStatus_C->addItems({"在线","离线","故障"});
}

DeviceDialog::~DeviceDialog()
{
    delete ui;
}

// DeviceStatus 转中文
static QString statusToString(DeviceStatus status)
{
    switch (status)
    {
    case DeviceStatus::Online:
        return "在线";
    case DeviceStatus::Offline:
        return "离线";
    case DeviceStatus::Fault:
        return "故障";
    }

    return "未知";
}

// 中文转 DeviceStatus
static DeviceStatus stringToStatus(const QString &status)
{
    if (status == "在线")
        return DeviceStatus::Online;

    if (status == "故障")
        return DeviceStatus::Fault;

    return DeviceStatus::Offline;
}
void DeviceDialog::setInfo(const DeviceInfo &info)
{
    ui->edtDevId->setText(info.devId);
    ui->edtDevName->setText(info.devName);
    ui->edtDevType->setText(info.devType);

    ui->cmbCommType_C->setCurrentText(info.commType);

    ui->edtAddr->setText(info.devAddr);

    // 枚举转换成中文
    ui->cmbStatus_C->setCurrentText(statusToString(info.status));

    // QDateTime 转换成字符串
    ui->edtAddTime_L->setText(
        info.addTime.toString("yyyy-MM-dd HH:mm:ss"));

    // 编辑模式下，设备编号不允许修改
    ui->edtDevId_L->setReadOnly(true);
}
DeviceInfo DeviceDialog::getInfo() const
{
    DeviceInfo d;

    d.devId = ui->edtDevId->text().trimmed();
    d.devName = ui->edtDevName->text().trimmed();
    d.devType = ui->edtDevType->text().trimmed();

    d.commType = ui->cmbCommType_C->currentText();

    d.devAddr = ui->edtAddr->text().trimmed();

    // 中文转换成枚举
    d.status = stringToStatus(ui->cmbStatus_C->currentText());

    // 时间处理：新增时自动生成，编辑时保留原时间
    if (ui->edtAddTime_L->text().trimmed().isEmpty())
    {
        d.addTime = QDateTime::currentDateTime();
    }
    else
    {
        d.addTime = QDateTime::fromString(
            ui->edtAddTime_L->text(),
            "yyyy-MM-dd HH:mm:ss");
    }

    return d;
}
void DeviceDialog::on_btnOk_clicked()
{
    // 简单校验
    if(ui->edtDevId->text().isEmpty() || ui->edtDevName->text().isEmpty())
    {
        QMessageBox::warning(this,"校验失败","设备编号、设备名称不能为空！");
        return;
    }
    accept();
}

void DeviceDialog::on_btnCancel_clicked()
{
    reject();
}
