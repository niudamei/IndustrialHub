#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QWidget>
#include "ui_monitorpage.h"

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(QWidget *parent = nullptr);
    ~MonitorPage() override;

private:
    Ui::MonitorPage *ui;
};

#endif // MONITORPAGE_H
