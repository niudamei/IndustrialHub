#include "configpage.h"
#include "ui_configpage.h"

#include "src/pages/devicemanager.h"
#include "src/communication/communicationmanager.h"
#include "src/protocol/protocolmanager.h"
#include "src/protocol/businessdatapoint.h"

#include <QDateTime>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>

ConfigPage::ConfigPage(DeviceManager *deviceManager,
                       CommunicationManager *communicationManager,
                       ProtocolManager *protocolManager,
                       QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ConfigPage),
      m_deviceManager(deviceManager),
      m_communicationManager(communicationManager),
      m_protocolManager(protocolManager)
{
    ui->setupUi(this);

    // 协议绑定是设备级配置：默认不解析，用户明确选择后才启用解析器。
    auto *parserLayout = new QHBoxLayout;
    parserLayout->addWidget(new QLabel(QStringLiteral("业务协议："), this));
    m_parserCombo = new QComboBox(this);
    m_parserCombo->addItem(QStringLiteral("不解析原始数据"), QString());
    m_parserCombo->addItem(QStringLiteral("JSON 示例协议"), QStringLiteral("json"));
    parserLayout->addWidget(m_parserCombo, 1);
    auto *applyParserButton = new QPushButton(QStringLiteral("应用到当前设备"), this);
    parserLayout->addWidget(applyParserButton);
    ui->verticalLayout->insertLayout(1, parserLayout);
    connect(applyParserButton, &QPushButton::clicked,
            this, &ConfigPage::onApplyParserClicked);

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
            if (m_communicationManager) m_communicationManager->releaseDevice(deviceId);
        });
        connect(m_deviceManager, &DeviceManager::deviceUpdated,
                this, [this](const DeviceInfo &device) {
            const QString key = device.commType.toCaseFolded() + QChar(0x1f) + device.devAddr;
            const QString previous = m_communicationConfigByDevice.value(device.devId.toCaseFolded());
            if ((!device.enabled || (!previous.isEmpty() && previous != key)) && m_communicationManager)
                m_communicationManager->releaseDevice(device.devId);
            m_communicationConfigByDevice.insert(device.devId.toCaseFolded(), key);
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
        connect(m_communicationManager,
                &CommunicationManager::deviceReconnecting,
                this, [this](const QString &deviceId, int attempt, int delayMs) {
            appendLog(QStringLiteral("[自动重连][%1] 第 %2 次尝试，%3 秒后执行。")
                          .arg(deviceId).arg(attempt)
                          .arg(QString::number(delayMs / 1000.0, 'f', 1)));
        });
    }

    if (m_protocolManager) {
        connect(m_protocolManager, &ProtocolManager::businessDataDecoded,
                this, &ConfigPage::onBusinessDataDecoded);
        connect(m_protocolManager, &ProtocolManager::parseFailed,
                this, &ConfigPage::onParseFailed);
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

    m_communicationConfigByDevice.clear();
    if (m_deviceManager) {
        const QList<DeviceInfo> devices = m_deviceManager->getAllDevices();
        for (const DeviceInfo &device : devices) {
            m_communicationConfigByDevice.insert(device.devId.toCaseFolded(),
                device.commType.toCaseFolded() + QChar(0x1f) + device.devAddr);
            if (!device.enabled) continue;
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

    if (!device.enabled) {
        QMessageBox::warning(this, QStringLiteral("设备已禁用"), QStringLiteral("请先在设备管理中启用该设备配置。"));
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

void ConfigPage::onApplyParserClicked()
{
    if (!m_protocolManager) {
        appendLog(QStringLiteral("[协议] 协议管理器不可用。"));
        return;
    }

    const QString deviceId = selectedDeviceId();
    if (deviceId.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先选择设备。"));
        return;
    }

    const QString parserId = m_parserCombo->currentData().toString();
    if (parserId.isEmpty()) {
        m_protocolManager->unbindDevice(deviceId);
        appendLog(QStringLiteral("[协议] 设备 %1 已设置为不解析。").arg(deviceId));
        return;
    }

    QString error;
    if (!m_protocolManager->bindDevice(deviceId, parserId, &error)) {
        QMessageBox::warning(this, QStringLiteral("协议绑定失败"), error);
        appendLog(QStringLiteral("[协议错误] %1").arg(error));
        return;
    }

    appendLog(QStringLiteral("[协议] 设备 %1 已绑定 %2 解析器。")
                  .arg(deviceId, parserId.toUpper()));
}

void ConfigPage::onBusinessDataDecoded(
    const QString &deviceId,
    const QVector<BusinessDataPoint> &points)
{
    for (const BusinessDataPoint &point : points) {
        QString quality;
        switch (point.quality) {
        case DataQuality::Good: quality = QStringLiteral("Good"); break;
        case DataQuality::Uncertain: quality = QStringLiteral("Uncertain"); break;
        case DataQuality::Bad: quality = QStringLiteral("Bad"); break;
        }

        appendLog(QStringLiteral("[业务数据][%1] %2 (%3) = %4 %5 [%6]")
                      .arg(deviceId, point.name, point.variableId,
                           QString::number(point.value, 'g', 12),
                           point.unit, quality));
    }
}

void ConfigPage::onParseFailed(const QString &deviceId, const QString &message)
{
    appendLog(QStringLiteral("[协议解析失败][%1] %2").arg(deviceId, message));
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
            stateText = selectedDevice.commType.compare(QStringLiteral("CANFD"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("CAN-FD 适配器已就绪") : QStringLiteral("CAN 适配器已就绪");
        } else {
            stateText = QStringLiteral("已连接");
        }
    }
    if (m_parserCombo && m_protocolManager) {
        const QSignalBlocker blocker(m_parserCombo);
        const QString parserId = m_protocolManager->parserForDevice(id);
        const int parserIndex = m_parserCombo->findData(parserId);
        m_parserCombo->setCurrentIndex(parserIndex >= 0 ? parserIndex : 0);
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
