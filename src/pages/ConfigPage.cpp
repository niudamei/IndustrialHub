#include "configpage.h"
#include "ui_configpage.h"

#include "src/pages/devicemanager.h"
#include "src/communication/communicationmanager.h"

#include <QDateTime>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>

ConfigPage::ConfigPage(DeviceManager *deviceManager,
                       CommunicationManager *communicationManager,
                       QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ConfigPage),
      m_deviceManager(deviceManager),
      m_communicationManager(communicationManager)
{
    ui->setupUi(this);

    ui->disconnectButton->setEnabled(false);
    ui->sendButton->setEnabled(false);

    connect(ui->connectButton, &QPushButton::clicked,
            this, &ConfigPage::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked,
            this, &ConfigPage::onDisconnectClicked);
    connect(ui->sendButton, &QPushButton::clicked,
            this, &ConfigPage::onSendClicked);
    connect(ui->deviceCombo, &QComboBox::currentIndexChanged,
            this, &ConfigPage::updateConnectionDisplay);

    if (m_deviceManager) {
        connect(m_deviceManager, &DeviceManager::devicesChanged,
                this, &ConfigPage::refreshDeviceList);
        connect(m_deviceManager, &DeviceManager::deviceRemoved,
                this, [this](const QString &deviceId) {
            if (m_communicationManager)
                m_communicationManager->releaseDevice(deviceId);
        });
    }

    if (m_communicationManager) {
        connect(m_communicationManager,
                &CommunicationManager::deviceConnectionStateChanged,
                this, &ConfigPage::onConnectionStateChanged);
        connect(m_communicationManager,
                &CommunicationManager::deviceDataReceived,
                this, &ConfigPage::onDataReceived);
        connect(m_communicationManager,
                &CommunicationManager::deviceErrorOccurred,
                this, &ConfigPage::onCommunicationError);
    }

    refreshDeviceList();
}

ConfigPage::~ConfigPage()
{
    delete ui;
}

QString ConfigPage::selectedDeviceId() const
{
    return ui->deviceCombo->currentData().toString();
}

void ConfigPage::refreshDeviceList()
{
    const QString previousId = selectedDeviceId();
    const QSignalBlocker blocker(ui->deviceCombo);
    ui->deviceCombo->clear();

    if (m_deviceManager) {
        const QList<DeviceInfo> devices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : devices) {
            const QString label = QStringLiteral("%1 | %2 | %3")
                                      .arg(device.devId, device.devName, device.commType);
            ui->deviceCombo->addItem(label, device.devId);
        }
    }

    const int previousIndex = ui->deviceCombo->findData(previousId);
    if (previousIndex >= 0)
        ui->deviceCombo->setCurrentIndex(previousIndex);

    updateConnectionDisplay();
}

void ConfigPage::onConnectClicked()
{
    if (!m_deviceManager || !m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    DeviceInfo device;
    if (id.isEmpty() || !m_deviceManager->getDeviceById(id, device)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个有效设备。"));
        return;
    }

    QString error;
    if (!m_communicationManager->connectDevice(device, &error)) {
        appendLog(QStringLiteral("[错误] %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("连接失败"), error);
        return;
    }

    appendLog(QStringLiteral("[连接请求] %1 -> %2").arg(device.devId, device.devAddr));
    updateConnectionDisplay();
}

void ConfigPage::onDisconnectClicked()
{
    if (!m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    if (id.isEmpty())
        return;

    m_communicationManager->disconnectDevice(id);
    appendLog(QStringLiteral("[断开请求] %1").arg(id));
    updateConnectionDisplay();
}

void ConfigPage::onSendClicked()
{
    if (!m_communicationManager)
        return;

    const QString id = selectedDeviceId();
    DeviceInfo device;
    if (id.isEmpty() || !m_deviceManager
        || !m_deviceManager->getDeviceById(id, device)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择一个有效设备。"));
        return;
    }

    QByteArray data;
    const bool slcanMode =
        device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
        || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0;

    if (slcanMode) {
        data = ui->sendHexEdit->text().trimmed().toLatin1();
        if (data.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("数据格式错误"),
                                 QStringLiteral("请输入 SLCAN 帧命令，例如 T18AABBCC03112233。"));
            return;
        }
    } else {
        QString hex = ui->sendHexEdit->text();
        hex.remove(QRegularExpression(QStringLiteral("\\s+")));

        if (hex.isEmpty() || (hex.size() % 2) != 0
            || !QRegularExpression(QStringLiteral("^[0-9A-Fa-f]+$")).match(hex).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("数据格式错误"),
                                 QStringLiteral("请输入偶数个十六进制字符，可使用空格分隔。"));
            return;
        }
        data = QByteArray::fromHex(hex.toLatin1());
    }

    QString error;
    if (!m_communicationManager->sendData(id, data, &error)) {
        appendLog(QStringLiteral("[发送失败] %1").arg(error));
        QMessageBox::warning(this, QStringLiteral("发送失败"), error);
        return;
    }

    const QString displayData = slcanMode
        ? QString::fromLatin1(data)
        : QString::fromLatin1(data.toHex(' ').toUpper());
    appendLog(QStringLiteral("[TX][%1] %2").arg(id, displayData));
}

void ConfigPage::onConnectionStateChanged(const QString &deviceId, bool connected)
{
    appendLog(QStringLiteral("[%1] 设备 %2")
                  .arg(connected ? QStringLiteral("已连接") : QStringLiteral("已断开"),
                       deviceId));

    if (m_deviceManager) {
        DeviceInfo device;
        if (m_deviceManager->getDeviceById(deviceId, device)) {
            // TCP 有连接握手；UDP 绑定本地端口、RS485 打开串口都不能证明远端设备在线。
            const bool connectionProvesOnline =
                device.commType.compare(QStringLiteral("TCP"), Qt::CaseInsensitive) == 0
                || device.commType.compare(QStringLiteral("SIM"), Qt::CaseInsensitive) == 0
                || device.commType.compare(QStringLiteral("SIMULATED"), Qt::CaseInsensitive) == 0;

            if (!connected || connectionProvesOnline) {
                m_deviceManager->updateDeviceStatus(deviceId,
                    connected ? DeviceStatus::Online : DeviceStatus::Offline);
            }
        }
    }

    if (deviceId.compare(selectedDeviceId(), Qt::CaseInsensitive) == 0)
        updateConnectionDisplay();
}

void ConfigPage::onDataReceived(const QString &deviceId, const QByteArray &data)
{
    QString displayData = QString::fromLatin1(data.toHex(' ').toUpper());
    if (m_deviceManager) {
        DeviceInfo device;
        if (m_deviceManager->getDeviceById(deviceId, device)
            && (device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
                || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0)) {
            displayData = QString::fromLatin1(data);
        }
        // 收到实际数据后才将 UDP、RS485、CAN/CAN-FD 等无连接链路标记为在线。
        m_deviceManager->updateDeviceStatus(deviceId, DeviceStatus::Online);
    }
    appendLog(QStringLiteral("[RX][%1] %2").arg(deviceId, displayData));
}

void ConfigPage::onCommunicationError(const QString &deviceId, const QString &message)
{
    const QString prefix = deviceId.isEmpty()
        ? QStringLiteral("[通信管理错误]")
        : QStringLiteral("[通信错误][%1]").arg(deviceId);
    appendLog(prefix + QStringLiteral(" ") + message);
}

void ConfigPage::updateConnectionDisplay()
{
    const QString id = selectedDeviceId();
    bool connected = m_communicationManager && !id.isEmpty()
                     && m_communicationManager->isConnected(id);

    QString stateText = QStringLiteral("未连接");
    if (connected) {
        DeviceInfo selectedDevice;
        if (m_deviceManager && m_deviceManager->getDeviceById(id, selectedDevice)
            && selectedDevice.commType.compare(QStringLiteral("UDP"), Qt::CaseInsensitive) == 0) {
            stateText = QStringLiteral("UDP 本地已就绪");
        } else if (m_deviceManager
                   && m_deviceManager->getDeviceById(id, selectedDevice)
                   && (selectedDevice.commType.compare(QStringLiteral("RS485"), Qt::CaseInsensitive) == 0
                       || selectedDevice.commType.compare(QStringLiteral("SERIAL"), Qt::CaseInsensitive) == 0)) {
            stateText = QStringLiteral("串口已打开");
        } else if (m_deviceManager
                   && m_deviceManager->getDeviceById(id, selectedDevice)
                   && (selectedDevice.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
                       || selectedDevice.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0)) {
            stateText = QStringLiteral("CAN 适配器已就绪");
        } else {
            stateText = QStringLiteral("已连接");
        }
    }
    ui->connectionStateLabel->setText(stateText);
    ui->connectButton->setEnabled(!id.isEmpty() && !connected);
    ui->disconnectButton->setEnabled(connected);
    ui->sendButton->setEnabled(connected);

    DeviceInfo device;
    if (m_deviceManager && !id.isEmpty()
        && m_deviceManager->getDeviceById(id, device)) {
        ui->deviceAddressLabel->setText(device.devAddr);
        const bool slcanMode =
            device.commType.compare(QStringLiteral("CAN"), Qt::CaseInsensitive) == 0
            || device.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0;
        ui->sendHexEdit->setPlaceholderText(slcanMode
            ? QStringLiteral("SLCAN 帧：T18AABBCC03112233 / D000000018...")
            : QStringLiteral("HEX 数据：01 03 00 00 00 02"));
    } else {
        ui->deviceAddressLabel->setText(QStringLiteral("--"));
        ui->sendHexEdit->setPlaceholderText(QStringLiteral("请选择设备"));
    }
}

void ConfigPage::appendLog(const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    ui->communicationLog->append(QStringLiteral("[%1] %2").arg(time, message));
}
