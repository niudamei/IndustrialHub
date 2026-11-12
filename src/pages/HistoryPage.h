#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QWidget>
#include "ui_historypage.h"

class CustomTableWidget;

class HistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPage(QWidget *parent = nullptr);
    ~HistoryPage() override;

private:
    Ui::HistoryPage *ui;
};

#endif // HISTORYPAGE_H
