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

class DeviceManager;
class CommunicationManager;
class ProtocolManager;
class QComboBox;
class QTableWidget;
struct DeviceInfo;

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(DeviceManager *deviceManager,
                        CommunicationManager *communicationManager,
                        ProtocolManager *protocolManager = nullptr,
                        QWidget *parent = nullptr);
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

private:
    QString selectedDeviceId() const;
    void updateConnectionDisplay();
    void appendLog(const QString &message);

    Ui::ConfigPage *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    ProtocolManager *m_protocolManager = nullptr;
    QComboBox *m_parserCombo = nullptr;
    QHash<QString, QString> m_communicationConfigByDevice;
    CommunicationDiagnostics *m_diagnostics = nullptr;
    QTableWidget *m_diagnosticsTable = nullptr;
    QTableWidget *m_statisticsTable = nullptr;
};

#endif // CONFIGPAGE_H
