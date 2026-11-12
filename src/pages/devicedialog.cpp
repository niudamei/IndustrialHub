#include "devicedialog.h"
#include "ui_devicedialog.h"
#include <QDateTime>
#include <QMessageBox>

namespace {
QString statusToString(DeviceStatus s) {
    switch (s) {
    case DeviceStatus::Online: return QStringLiteral("在线");
    case DeviceStatus::Offline: return QStringLiteral("离线");
    case DeviceStatus::Fault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}
DeviceStatus stringToStatus(const QString &s) {
    if (s == QStringLiteral("在线")) return DeviceStatus::Online;
    if (s == QStringLiteral("故障")) return DeviceStatus::Fault;
    return DeviceStatus::Offline;
}
}
DeviceDialog::DeviceDialog(QWidget *parent) : QDialog(parent), ui(new Ui::DeviceDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("设备信息"));
    ui->cmbCommType_C->addItems({"TCP", "UDP", "CAN", "CANFD", "RS485", "SIMULATED", "Modbus"});
    connect(ui->cmbCommType_C, &QComboBox::currentTextChanged,
            this, [this](const QString &type) {
        QString hint;
        if (type == QStringLiteral("TCP")) {
            hint = QStringLiteral("192.168.1.100:5000 或 [IPv6]:5000");
        } else if (type == QStringLiteral("UDP")) {
            hint = QStringLiteral("192.168.1.100:5000（数字 IP）");
        } else if (type == QStringLiteral("RS485")) {
            hint = QStringLiteral("COM3,115200,8,N,1");
        } else if (type == QStringLiteral("CAN")) {
            hint = QStringLiteral("COM4,1000000（CANable SLCAN）");
        } else if (type == QStringLiteral("CANFD")) {
            hint = QStringLiteral("COM4,1000000,2000000（仲裁段/数据段）");
        } else if (type == QStringLiteral("SIMULATED")) {
            hint = QStringLiteral("模拟通信无需填写");
        } else {
            hint = QStringLiteral("当前版本暂未实现该通信驱动");
        }
        ui->edtAddr_L->setPlaceholderText(hint);
        ui->edtAddr_L->setToolTip(hint);
    });
    ui->cmbCommType_C->setCurrentText(QStringLiteral("TCP"));
    ui->edtAddr_L->setPlaceholderText(QStringLiteral("192.168.1.100:5000 或 [IPv6]:5000"));
    ui->cmbStatus_C->addItems({"在线", "离线", "故障"});
    ui->edtAddTime_L->setReadOnly(true);
    ui->edtAddTime_L->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}
DeviceDialog::~DeviceDialog() { delete ui; }
void DeviceDialog::setInfo(const DeviceInfo &d)
{
    ui->edtDevId_L->setText(d.devId);
    ui->edtDevName_L->setText(d.devName);
    ui->edtDevType_L->setText(d.devType);
    ui->cmbCommType_C->setCurrentText(d.commType);
    ui->edtAddr_L->setText(d.devAddr);
    ui->cmbStatus_C->setCurrentText(statusToString(d.status));
    ui->edtAddTime_L->setText(d.addTime.isValid() ? d.addTime.toString("yyyy-MM-dd HH:mm:ss")
                                                   : QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    ui->edtDevId_L->setReadOnly(true);
}
DeviceInfo DeviceDialog::getInfo() const
{
    DeviceInfo d;
    d.devId = ui->edtDevId_L->text().trimmed();
    d.devName = ui->edtDevName_L->text().trimmed();
    d.devType = ui->edtDevType_L->text().trimmed();
    d.commType = ui->cmbCommType_C->currentText().trimmed();
    d.devAddr = ui->edtAddr_L->text().trimmed();
    d.status = stringToStatus(ui->cmbStatus_C->currentText());
    d.addTime = QDateTime::fromString(ui->edtAddTime_L->text().trimmed(), "yyyy-MM-dd HH:mm:ss");
    if (!d.addTime.isValid()) d.addTime = QDateTime::currentDateTime();
    return d;
}
void DeviceDialog::on_btnOk_clicked()
{
    if (ui->edtDevId_L->text().trimmed().isEmpty() || ui->edtDevName_L->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("校验失败"), QStringLiteral("设备编号、设备名称不能为空！"));
        return;
    }
    accept();
}
void DeviceDialog::on_btnCancel_clicked() { reject(); }
