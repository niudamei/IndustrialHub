#include "alarmpage.h"
#include "ui_alarmpage.h"
#include <QLabel>

AlarmPage::AlarmPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AlarmPage)
{
    ui->setupUi(this);
}

AlarmPage::~AlarmPage()
{
    delete ui;
}
