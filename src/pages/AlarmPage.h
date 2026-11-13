#ifndef ALARMPAGE_H
#define ALARMPAGE_H
#include <QWidget>
#include "ui_alarmpage.h"
class AlarmManager;
class AlarmPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPage(AlarmManager *alarmManager, QWidget *parent = nullptr);
    ~AlarmPage() override;
private slots:
    void refreshEvents();
    void acknowledgeSelected();
    void addThresholdRule();
private:
    Ui::AlarmPage *ui;
    AlarmManager *m_alarmManager = nullptr;
};
#endif // ALARMPAGE_H
