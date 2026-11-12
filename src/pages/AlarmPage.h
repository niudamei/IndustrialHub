#ifndef ALARMPAGE_H
#define ALARMPAGE_H

#include <QWidget>
#include "ui_alarmpage.h"

class CustomTableWidget;

class AlarmPage : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmPage(QWidget *parent = nullptr);
    ~AlarmPage() override;

private:
    Ui::AlarmPage *ui;
};

#endif // ALARMPAGE_H
