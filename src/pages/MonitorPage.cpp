#include "monitorpage.h"
#include "ui_monitorpage.h"

MonitorPage::MonitorPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MonitorPage)
{
    ui->setupUi(this);
}

MonitorPage::~MonitorPage()
{
    delete ui;
}
