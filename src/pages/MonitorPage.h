#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QWidget>

#include "ui_monitorpage.h"

class DeviceManager;
class CommunicationManager;

class MonitorPage : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorPage(DeviceManager *deviceManager,
                         CommunicationManager *communicationManager,
                         QWidget *parent = nullptr);
    ~MonitorPage() override;

private slots:
    void refreshDeviceFilter();
    void applyFilters();
    void onConnectionStateChanged(const QString &deviceId, bool connected);
    void onDataSent(const QString &deviceId, const QByteArray &data);
    void onDataReceived(const QString &deviceId, const QByteArray &data);
    void onCommunicationError(const QString &deviceId, const QString &message);
    void onPauseClicked();

private:
    void appendEvent(const QString &deviceId,
                     const QString &direction,
                     const QString &content);
    QString formatData(const QString &deviceId, const QByteArray &data) const;

    Ui::MonitorPage *ui = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    CommunicationManager *m_communicationManager = nullptr;
    bool m_paused = false;
    static constexpr int MaxRows = 500;
};

#endif // MONITORPAGE_H
