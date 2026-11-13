#include "devicepage.h"
#include "devicedialog.h"
#include "devicemanager.h"

#include <QHeaderView>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QStandardItem>
#include <QFileDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDateTime>
#include <functional>

namespace {
QString statusText(DeviceStatus s) {
    switch (s) {
    case DeviceStatus::Online: return QStringLiteral("在线");
    case DeviceStatus::Offline: return QStringLiteral("离线");
    case DeviceStatus::Fault: return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}
QJsonObject deviceToJson(const DeviceInfo &d) {
    QJsonObject o;
    o.insert(QStringLiteral("deviceId"), d.devId);
    o.insert(QStringLiteral("name"), d.devName);
    o.insert(QStringLiteral("type"), d.devType);
    o.insert(QStringLiteral("communicationType"), d.commType);
    o.insert(QStringLiteral("address"), d.devAddr);
    o.insert(QStringLiteral("group"), d.groupName);
    o.insert(QStringLiteral("enabled"), d.enabled);
    o.insert(QStringLiteral("status"), static_cast<int>(d.status));
    o.insert(QStringLiteral("addedAt"), d.addTime.toString(Qt::ISODateWithMs));
    return o;
}
bool deviceFromJson(const QJsonObject &o, DeviceInfo &d, QString *error) {
    d.devId = o.value(QStringLiteral("deviceId")).toString().trimmed();
    d.devName = o.value(QStringLiteral("name")).toString().trimmed();
    d.devType = o.value(QStringLiteral("type")).toString().trimmed();
    d.commType = o.value(QStringLiteral("communicationType")).toString().trimmed();
    d.devAddr = o.value(QStringLiteral("address")).toString().trimmed();
    d.groupName = o.value(QStringLiteral("group")).toString().trimmed();
    d.enabled = o.value(QStringLiteral("enabled")).toBool(true);
    const int status = o.value(QStringLiteral("status")).toInt(static_cast<int>(DeviceStatus::Offline));
    if (status < static_cast<int>(DeviceStatus::Online) || status > static_cast<int>(DeviceStatus::Fault)) {
        if (error) *error = QStringLiteral("设备 %1 的状态值无效。").arg(d.devId);
        return false;
    }
    d.status = static_cast<DeviceStatus>(status);
    d.addTime = QDateTime::fromString(o.value(QStringLiteral("addedAt")).toString(), Qt::ISODateWithMs);
    if (!d.addTime.isValid()) d.addTime = QDateTime::currentDateTime();
    if (d.devId.isEmpty() || d.devName.isEmpty()) {
        if (error) *error = QStringLiteral("设备编号和名称不能为空。");
        return false;
    }
    return true;
}
}

DevicePage::DevicePage(DeviceManager *manager, QWidget *parent)
    : QWidget(parent), ui(new Ui::DevicePage), m_deviceManager(manager)
{
    ui->setupUi(this);
    initTable();
    ui->cmbStatus->addItems({QStringLiteral("全部"), QStringLiteral("在线"),
                             QStringLiteral("离线"), QStringLiteral("故障")});

    auto *actions = qobject_cast<QHBoxLayout *>(ui->verticalLayout->itemAt(0)->layout());
    auto addAction = [this, actions](const QString &label, const std::function<void()> &fn) {
        auto *button = new QPushButton(label, this);
        actions->addWidget(button);
        connect(button, &QPushButton::clicked, this, fn);
    };
    addAction(QStringLiteral("批量启用"), [this]() {
        if (!m_deviceManager) return;
        const QStringList ids = selectedDeviceIds();
        if (ids.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先多选设备。")); return; }
        int changed = 0; QStringList errors;
        for (const QString &id : ids) {
            DeviceInfo d;
            if (!m_deviceManager->getDeviceById(id, d)) continue;
            d.enabled = true; QString error;
            if (m_deviceManager->updateDevice(d, &error)) ++changed; else errors << error;
        }
        refreshTable();
        QMessageBox::information(this, QStringLiteral("批量启用"), QStringLiteral("成功更新 %1 台设备。%2").arg(changed).arg(errors.isEmpty() ? QString() : QStringLiteral("\n失败：") + errors.join(QStringLiteral("\n"))));
    });
    addAction(QStringLiteral("批量禁用"), [this]() {
        if (!m_deviceManager) return;
        const QStringList ids = selectedDeviceIds();
        if (ids.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先多选设备。")); return; }
        int changed = 0; QStringList errors;
        for (const QString &id : ids) {
            DeviceInfo d;
            if (!m_deviceManager->getDeviceById(id, d)) continue;
            d.enabled = false; QString error;
            if (m_deviceManager->updateDevice(d, &error)) ++changed; else errors << error;
        }
        refreshTable();
        QMessageBox::information(this, QStringLiteral("批量禁用"), QStringLiteral("成功更新 %1 台设备。%2").arg(changed).arg(errors.isEmpty() ? QString() : QStringLiteral("\n失败：") + errors.join(QStringLiteral("\n"))));
    });
    addAction(QStringLiteral("导入配置"), [this]() { importConfiguration(); });
    addAction(QStringLiteral("导出配置"), [this]() { exportConfiguration(); });
    addAction(QStringLiteral("保存通信模板"), [this]() { saveCommunicationTemplate(); });
    addAction(QStringLiteral("应用通信模板"), [this]() { applyCommunicationTemplate(); });

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
        QStringLiteral("设备分组"), QStringLiteral("启用状态"), QStringLiteral("当前状态"), QStringLiteral("添加时间")});
    ui->DeviceView->setModel(m_model);
    ui->DeviceView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->DeviceView->setSelectionMode(QAbstractItemView::ExtendedSelection);
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
            << new QStandardItem(d.devAddr) << new QStandardItem(d.groupName)
            << new QStandardItem(d.enabled ? QStringLiteral("已启用") : QStringLiteral("已禁用"))
            << new QStandardItem(statusText(d.status))
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
            || m_model->index(r,4).data().toString().contains(key, Qt::CaseInsensitive)
            || m_model->index(r,5).data().toString().contains(key, Qt::CaseInsensitive);
        const QString rowStatus = m_model->index(r,7).data().toString();
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
QStringList DevicePage::selectedDeviceIds() const
{
    QStringList ids;
    if (!ui->DeviceView->selectionModel()) return ids;
    for (const QModelIndex &index : ui->DeviceView->selectionModel()->selectedRows())
        ids << m_model->index(index.row(), 0).data().toString();
    return ids;
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
    const QStringList ids = selectedDeviceIds();
    if (ids.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中设备。")); return; }
    if (QMessageBox::question(this, QStringLiteral("确认删除"),
        QStringLiteral("确定删除选中的 %1 台设备？此操作不可恢复。").arg(ids.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QStringList errors;
    for (const QString &id : ids) { QString error; if (!m_deviceManager->removeDevice(id, &error)) errors << error; }
    if (!errors.isEmpty()) QMessageBox::warning(this, QStringLiteral("部分删除失败"), errors.join(QStringLiteral("\n")));
}
void DevicePage::on_btnRefresh_clicked()
{
    ui->edtSearch->clear();
    ui->cmbStatus->setCurrentIndex(0);
    refreshTable();
}
void DevicePage::on_edtSearch_textChanged(const QString &text) { Q_UNUSED(text); filterData(); }
void DevicePage::on_cmbStatus_currentIndexChanged(int index) { Q_UNUSED(index); filterData(); }

void DevicePage::exportConfiguration()
{
    if (!m_deviceManager) return;
    QList<DeviceInfo> devices;
    const QStringList selected = selectedDeviceIds();
    for (const DeviceInfo &d : m_deviceManager->getAllDevices())
        if (selected.isEmpty() || selected.contains(d.devId, Qt::CaseInsensitive)) devices << d;
    QJsonArray array;
    for (const DeviceInfo &d : devices) array.append(deviceToJson(d));
    QJsonObject root; root.insert(QStringLiteral("format"), QStringLiteral("IndustrialHubDeviceConfig"));
    root.insert(QStringLiteral("version"), 1); root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("devices"), array);
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出设备配置"), QStringLiteral("industrialhub_devices.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString()); return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("已导出 %1 台设备。").arg(devices.size()));
}
void DevicePage::importConfiguration()
{
    if (!m_deviceManager) return;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入设备配置"), QString(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, QStringLiteral("导入失败"), file.errorString()); return; }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("JSON 格式错误：%1").arg(parseError.errorString())); return;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("IndustrialHubDeviceConfig")
        || !root.value(QStringLiteral("devices")).isArray()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("文件不是有效的 IndustrialHub 设备配置。")); return;
    }
    int added = 0, updated = 0;
    QStringList errors;
    for (const QJsonValue &value : root.value(QStringLiteral("devices")).toArray()) {
        if (!value.isObject()) { errors << QStringLiteral("发现非对象设备项"); continue; }
        DeviceInfo d; QString error;
        if (!deviceFromJson(value.toObject(), d, &error)) { errors << error; continue; }
        DeviceInfo existing;
        const bool exists = m_deviceManager->getDeviceById(d.devId, existing);
        const bool ok = exists ? m_deviceManager->updateDevice(d, &error) : m_deviceManager->addDevice(d, &error);
        if (ok) { if (exists) ++updated; else ++added; } else errors << error;
    }
    refreshTable();
    QMessageBox::information(this, QStringLiteral("导入完成"),
        QStringLiteral("新增 %1 台，更新 %2 台。%3").arg(added).arg(updated)
        .arg(errors.isEmpty() ? QString() : QStringLiteral("\n错误：\n") + errors.join(QStringLiteral("\n"))));
}
void DevicePage::saveCommunicationTemplate()
{
    if (!m_deviceManager) return;
    const QString id = selectedDeviceId();
    DeviceInfo d;
    if (id.isEmpty() || !m_deviceManager->getDeviceById(id, d)) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请选择一台设备作为通信模板。")); return;
    }
    QJsonObject root; root.insert(QStringLiteral("format"), QStringLiteral("IndustrialHubCommunicationTemplate"));
    root.insert(QStringLiteral("version"), 1); root.insert(QStringLiteral("communicationType"), d.commType);
    root.insert(QStringLiteral("deviceType"), d.devType); root.insert(QStringLiteral("address"), d.devAddr);
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存通信参数模板"),
        d.commType + QStringLiteral("_template.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), file.errorString()); return;
    }
    QMessageBox::information(this, QStringLiteral("保存完成"), QStringLiteral("通信参数模板已保存。"));
}
void DevicePage::applyCommunicationTemplate()
{
    if (!m_deviceManager) return;
    const QStringList ids = selectedDeviceIds();
    if (ids.isEmpty()) { QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要应用模板的设备。")); return; }
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择通信参数模板"), QString(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, QStringLiteral("读取失败"), file.errorString()); return; }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || doc.object().value(QStringLiteral("format")).toString() != QStringLiteral("IndustrialHubCommunicationTemplate")) {
        QMessageBox::warning(this, QStringLiteral("模板无效"), QStringLiteral("无法识别该通信参数模板。")); return;
    }
    const QJsonObject root = doc.object();
    int applied = 0; QStringList errors;
    for (const QString &id : ids) {
        DeviceInfo d;
        if (!m_deviceManager->getDeviceById(id, d)) continue;
        d.commType = root.value(QStringLiteral("communicationType")).toString(d.commType);
        d.devType = root.value(QStringLiteral("deviceType")).toString(d.devType);
        d.devAddr = root.value(QStringLiteral("address")).toString(d.devAddr);
        QString error;
        if (m_deviceManager->updateDevice(d, &error)) ++applied; else errors << error;
    }
    refreshTable();
    QMessageBox::information(this, QStringLiteral("模板应用完成"),
        QStringLiteral("成功应用到 %1 台设备。%2").arg(applied)
        .arg(errors.isEmpty() ? QString() : QStringLiteral("\n失败：") + errors.join(QStringLiteral("\n"))));
}
