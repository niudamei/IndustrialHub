#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "CustomFolder/customlabel.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class DashboardPage; class DevicePage; class DeviceManager; class CommunicationManager;
class DataManager; class AlarmPage; class ConfigPage; class HistoryPage; class MonitorPage;
class TrendPage; class SettingsPage;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void onNavigationClicked(int index);
private:
    void init_class();
    Ui::MainWindow *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    DataManager *m_dataManager = nullptr;
    DashboardPage *dashboardPage = nullptr;
    DevicePage *devicePage = nullptr;
    AlarmPage *alarmPage = nullptr;
    ConfigPage *configPage = nullptr;
    HistoryPage *historyPage = nullptr;
    MonitorPage *monitorPage = nullptr;
    TrendPage *trendPage = nullptr;
    SettingsPage *settingsPage = nullptr;
    CustomLabel *m_statusLabel = nullptr;
};
#endif // MAINWINDOW_H
