#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "CustomFolder/customlabel.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class DashboardPage;
class DevicePage;
class AlarmPage;
class ConfigPage;
class HistoryPage;
class MonitorPage;
class SettingsPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void init_class();
    ~MainWindow() override;

private slots:
    void onNavigationClicked(int index);

private:
    Ui::MainWindow *ui;
    DashboardPage *dashboardPage;
    DevicePage    *devicePage;
    AlarmPage     *alarmPage;
    ConfigPage    *configPage;
    HistoryPage   *historyPage;
    MonitorPage   *monitorPage;
    SettingsPage  *settingsPage;
    CustomLabel *m_statusLabel = nullptr;
};

#endif // MAINWINDOW_H
