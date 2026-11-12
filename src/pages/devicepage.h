#ifndef DEVICEPAGE_H
#define DEVICEPAGE_H
#include <QWidget>
#include <QStandardItemModel>
#include "deviceinfo.h"
#include "ui_devicepage.h"
class DeviceManager;
class DevicePage : public QWidget
{
    Q_OBJECT
public:
    explicit DevicePage(DeviceManager *manager, QWidget *parent = nullptr);
    ~DevicePage() override;
private slots:
    void on_btnAdd_clicked();
    void on_btnEdit_clicked();
    void on_btnDel_clicked();
    void on_btnRefresh_clicked();
    void on_edtSearch_textChanged(const QString &text);
    void on_cmbStatus_currentIndexChanged(int index);
private:
    void initTable();
    void refreshTable();
    void loadTableData(const QList<DeviceInfo> &devices);
    void filterData();
    QString selectedDeviceId() const;
    Ui::DevicePage *ui = nullptr;
    QStandardItemModel *m_model = nullptr;
    DeviceManager *m_deviceManager = nullptr;
};
#endif // DEVICEPAGE_H
