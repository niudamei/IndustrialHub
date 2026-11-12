#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>
#include "ui_configpage.h"

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    ~ConfigPage() override;

private:
    Ui::ConfigPage *ui;
};

#endif // CONFIGPAGE_H
