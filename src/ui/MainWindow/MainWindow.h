#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
class QCloseEvent;
class QSystemTrayIcon;
class QMenu;
#include "CustomFolder/customlabel.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class DashboardPage; class DevicePage; class DeviceManager; class CommunicationManager;
class DeviceHealthMonitor; class DataManager; class ProtocolManager; class BusinessDataManager; class AlarmManager; class AlarmPage; class ConfigPage; class HistoryPage; class MonitorPage;
class TrendPage; class SettingsPage; class AuthManager; class RemotePage; class UserManagementPage;
class PluginManager; class PluginPage; class SimulatorPage;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void onNavigationClicked(int index);
    void applyPermissions();
    void showLoginDialog();
protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void init_class();
    void initSystemTray();
    void setDarkTheme(bool darkTheme);
    Ui::MainWindow *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    DeviceHealthMonitor *m_deviceHealthMonitor = nullptr;
    DataManager *m_dataManager = nullptr;
    ProtocolManager *m_protocolManager = nullptr;
    BusinessDataManager *m_businessDataManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    AuthManager *m_authManager = nullptr;
    RemotePage *m_remotePage = nullptr;
    UserManagementPage *m_userManagementPage = nullptr;
    PluginManager *m_pluginManager = nullptr;
    PluginPage *m_pluginPage = nullptr;\n    SimulatorPage *m_simulatorPage = nullptr;
    DashboardPage *dashboardPage = nullptr;
    DevicePage *devicePage = nullptr;
    AlarmPage *alarmPage = nullptr;
    ConfigPage *configPage = nullptr;
    HistoryPage *historyPage = nullptr;
    MonitorPage *monitorPage = nullptr;
    TrendPage *trendPage = nullptr;
    SettingsPage *settingsPage = nullptr;
    CustomLabel *m_statusLabel = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    bool m_minimizeToTray = true;
    bool m_allowClose = false;
};
#endif // MAINWINDOW_H
