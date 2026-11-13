#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QHash>
#include "ui_configpage.h"
#include "src/protocol/businessdatapoint.h"
#include "src/core/communicationdiagnostics.h"
#include "src/protocol/modbusregistermap.h"

class DeviceManager;
class DeviceHealthMonitor;
class CommunicationManager;
class ProtocolManager;
class BusinessDataManager;
class AlarmManager;
class ModbusMaster;
class ModbusPoller;
class QComboBox;
class QTableWidget;
class QTimer;
class QDateTimeEdit;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
struct DeviceInfo;

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(DeviceManager *deviceManager,
                        CommunicationManager *communicationManager,
                        ProtocolManager *protocolManager = nullptr,
                        QWidget *parent = nullptr,
                        DeviceHealthMonitor *healthMonitor = nullptr,
                        BusinessDataManager *businessDataManager = nullptr,
                        AlarmManager *alarmManager = nullptr);
    ~ConfigPage() override;

private slots:
    void refreshDeviceList();
    void onConnectClicked();
    void onDisconnectClicked();
    void onSendClicked();
    void onConnectionStateChanged(const QString &deviceId, bool connected);
    void onDataReceived(const QString &deviceId, const QByteArray &data);
    void onCommunicationError(const QString &deviceId, const QString &message);
    void onApplyParserClicked();
    void onBusinessDataDecoded(const QString &deviceId,
                               const QVector<BusinessDataPoint> &points);
    void onParseFailed(const QString &deviceId, const QString &message);
    void refreshDiagnostics();
    void refreshModbusMappings();
    void addModbusMapping();
    void removeModbusMapping();
    void loadSimulatedModbusMappings();
    void startModbusPolling();
    void stopModbusPolling();

private:
    QString selectedDeviceId() const;
    void updateConnectionDisplay();
    void appendLog(const QString &message);
    void updateModbusControls();
    void saveModbusMappings(const QString &deviceId);
    void destroyModbusSession(const QString &deviceId);

    Ui::ConfigPage *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    ProtocolManager *m_protocolManager = nullptr;
    BusinessDataManager *m_businessDataManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    QComboBox *m_parserCombo = nullptr;
    QHash<QString, QString> m_communicationConfigByDevice;
    CommunicationDiagnostics *m_diagnostics = nullptr;
    QTableWidget *m_diagnosticsTable = nullptr;
    QTableWidget *m_statisticsTable = nullptr;
    QTimer *m_diagnosticsRefreshTimer = nullptr;
    QTimer *m_qualityRefreshTimer = nullptr;
    QLineEdit *m_diagnosticsDeviceFilter = nullptr;
    QDateTimeEdit *m_diagnosticsStart = nullptr;
    QDateTimeEdit *m_diagnosticsEnd = nullptr;

    QLineEdit *m_modbusVariableIdEdit = nullptr;
    QLineEdit *m_modbusNameEdit = nullptr;
    QLineEdit *m_modbusUnitEdit = nullptr;
    QSpinBox *m_modbusAddressSpin = nullptr;
    QSpinBox *m_modbusUnitIdSpin = nullptr;
    QSpinBox *m_modbusIntervalSpin = nullptr;
    QSpinBox *m_modbusTimeoutSpin = nullptr;
    QDoubleSpinBox *m_modbusScaleSpin = nullptr;
    QDoubleSpinBox *m_modbusOffsetSpin = nullptr;
    QComboBox *m_modbusFunctionCombo = nullptr;
    QComboBox *m_modbusDataTypeCombo = nullptr;
    QComboBox *m_modbusWordOrderCombo = nullptr;
    QTableWidget *m_modbusMappingTable = nullptr;
    QPushButton *m_modbusAddButton = nullptr;
    QPushButton *m_modbusRemoveButton = nullptr;
    QPushButton *m_modbusLoadSampleButton = nullptr;
    QPushButton *m_modbusStartButton = nullptr;
    QPushButton *m_modbusStopButton = nullptr;
    QLabel *m_modbusStatusLabel = nullptr;
    QHash<QString, QVector<ModbusRegisterMap::Mapping>> m_modbusMappingsByDevice;
    QHash<QString, ModbusMaster *> m_modbusMastersByDevice;
    QHash<QString, ModbusPoller *> m_modbusPollersByDevice;
};

#endif // CONFIGPAGE_H
