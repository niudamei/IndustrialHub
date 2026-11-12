#ifndef DEVICEDIALOG_H
#define DEVICEDIALOG_H

#include <QDialog>
#include "deviceinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DeviceDialog; }
QT_END_NAMESPACE

class DeviceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeviceDialog(QWidget *parent = nullptr);
    ~DeviceDialog();

    void setInfo(const DeviceInfo& info);
    DeviceInfo getInfo() const;

private slots:
    void on_btnOk_clicked();
    void on_btnCancel_clicked();

private:
    Ui::DeviceDialog *ui;
};
#endif // DEVICEDIALOG_H
