#include "virtualcancommunication.h"

#include "slcanframecodec.h"

#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>

VirtualCanCommunication::VirtualCanCommunication(bool canFd, QObject *parent)
    : ICommunication(parent), m_canFd(canFd)
{
}

VirtualCanCommunication::~VirtualCanCommunication()
{
    close();
}

bool VirtualCanCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (m_device && m_device->state() == QCanBusDevice::ConnectedState)
        return true;

    const QString interfaceName = device.devAddr.trimmed().isEmpty()
        ? QStringLiteral("can0") : device.devAddr.trimmed();
    const QStringList plugins = QCanBus::instance()->plugins();
    if (!plugins.contains(QStringLiteral("virtualcan"))) {
        const QString message = QStringLiteral(
            "Qt SerialBus 的 virtualcan 插件不可用。请安装与当前 Qt Kit 匹配的 Qt SerialBus 模块及插件。");
        if (errorMessage) *errorMessage = message;
        emit errorOccurred(message);
        return false;
    }

    QString error;
    QCanBusDevice *bus = QCanBus::instance()->createDevice(
        QStringLiteral("virtualcan"), interfaceName, &error);
    if (!bus) {
        const QString message = QStringLiteral("创建 VirtualCAN 设备失败：%1").arg(error);
        if (errorMessage) *errorMessage = message;
        emit errorOccurred(message);
        return false;
    }

    bus->setConfigurationParameter(QCanBusDevice::CanFdKey, m_canFd);
    bus->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, false);
    connect(bus, &QCanBusDevice::framesReceived, this, [this]() {
        if (!m_device) return;
        while (m_device->framesAvailable() > 0) {
            const QCanBusFrame frame = m_device->readFrame();
            if (!frame.isValid() || frame.frameType() == QCanBusFrame::ErrorFrame)
                continue;
            SlcanFrame slcan;
            slcan.id = frame.frameId();
            slcan.extended = frame.hasExtendedFrameFormat();
            slcan.fd = frame.hasFlexibleDataRateFormat();
            slcan.brs = frame.hasBitrateSwitch();
            slcan.remote = frame.frameType() == QCanBusFrame::RemoteRequestFrame;
            slcan.remoteDlc = static_cast<quint8>(frame.payload().size());
            slcan.data = frame.payload();
            QByteArray encoded;
            if (SlcanFrameCodec::encodeFrame(slcan, encoded))
                emit dataReceived(encoded + '\r');
        }
    });
    connect(bus, &QCanBusDevice::errorOccurred, this,
            [this, bus](QCanBusDevice::CanBusError) {
        if (m_device == bus)
            emit errorOccurred(bus->errorString());
    });

    if (!bus->connectDevice()) {
        const QString message = QStringLiteral("连接 VirtualCAN 失败：%1").arg(bus->errorString());
        if (errorMessage) *errorMessage = message;
        bus->deleteLater();
        return false;
    }
    m_device = bus;
    emit connectionStateChanged(true);
    return true;
}

void VirtualCanCommunication::close()
{
    if (!m_device)
        return;
    QCanBusDevice *bus = m_device;
    m_device = nullptr;
    bus->disconnectDevice();
    bus->deleteLater();
    emit connectionStateChanged(false);
}

bool VirtualCanCommunication::isOpen() const
{
    return m_device && m_device->state() == QCanBusDevice::ConnectedState;
}

bool VirtualCanCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isOpen()) {
        const QString message = QStringLiteral("VirtualCAN 尚未连接。");
        if (errorMessage) *errorMessage = message;
        emit errorOccurred(message);
        return false;
    }

    const QList<QByteArray> commands = data.split('\r');
    for (QByteArray command : commands) {
        command = command.trimmed();
        if (command.isEmpty()) continue;
        SlcanFrame parsed;
        QString error;
        if (!SlcanFrameCodec::parseFrame(command, m_canFd, parsed, &error)) {
            if (errorMessage) *errorMessage = error;
            emit errorOccurred(error);
            return false;
        }
        QCanBusFrame frame(parsed.id, parsed.data);
        if (parsed.extended) frame.setFrameId(parsed.id);
        frame.setFrameType(parsed.remote ? QCanBusFrame::RemoteRequestFrame
                                         : QCanBusFrame::DataFrame);
        if (parsed.extended) frame.setExtendedFrameFormat(true);
        if (parsed.fd) {
            frame.setFlexibleDataRateFormat(true);
            frame.setBitrateSwitch(parsed.brs);
        }
        if (!frame.isValid() || !m_device->writeFrame(frame)) {
            const QString message = QStringLiteral("VirtualCAN 写入失败：%1")
                                        .arg(m_device->errorString());
            if (errorMessage) *errorMessage = message;
            emit errorOccurred(message);
            return false;
        }
    }
    return true;
}
