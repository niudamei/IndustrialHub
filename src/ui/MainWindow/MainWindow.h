#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include "CustomFolder/customlabel.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class DashboardPage; class DevicePage; class DeviceManager; class CommunicationManager;
class DeviceHealthMonitor; class DataManager; class ProtocolManager; class BusinessDataManager; class AlarmManager; class AlarmPage; class ConfigPage; class HistoryPage; class MonitorPage;
class TrendPage; class SettingsPage; class AuthManager; class RemotePage; class UserManagementPage;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void onNavigationClicked(int index);\n    void applyPermissions();\n    void showLoginDialog();
private:
    void init_class();
    Ui::MainWindow *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    DeviceHealthMonitor *m_deviceHealthMonitor = nullptr;
    DataManager *m_dataManager = nullptr;
    ProtocolManager *m_protocolManager = nullptr;
    BusinessDataManager *m_businessDataManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;\n    AuthManager *m_authManager = nullptr;\n    RemotePage *m_remotePage = nullptr;\n    UserManagementPage *m_userManagementPage = nullptr;
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
