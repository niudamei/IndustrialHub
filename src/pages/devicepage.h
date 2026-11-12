#ifndef DEVICEPAGE_H
#define DEVICEPAGE_H

#include <QWidget>
#include <QStandardItemModel>
#include <QDialog>
#include <QDateTime>
#include <QMessageBox>
#include "deviceinfo.h"
#include "ui_devicepage.h"
#include <vector>
#include <QString>


class CustomTableWidget;
class CustomButton;
class DeviceDialog;
class DevicePage : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePage(QWidget *parent = nullptr);
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
    void loadTableData(const QList<DeviceInfo>& list);
    void filterData();
    int findRowById(const QString& id);
    Ui::DevicePage *ui;
    QStandardItemModel* m_model;
    QList<DeviceInfo> m_allDevList; // 全部原始数据，用于筛选搜索
};

#endif // DEVICEPAGE_H
