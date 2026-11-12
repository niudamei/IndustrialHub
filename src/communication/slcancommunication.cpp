#include "slcancommunication.h"

#include <QStringList>

SlcanCommunication::SlcanCommunication(bool canFdMode, QObject *parent)
    : ICommunication(parent),
      m_canFdMode(canFdMode)
{
    connect(&m_serialPort, &QSerialPort::readyRead,
            this, &SlcanCommunication::readSerialData);

    connect(&m_serialPort, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError)
            return;

        emit errorOccurred(QStringLiteral("SLCAN 串口错误：%1")
                               .arg(m_serialPort.errorString()));

        if (error == QSerialPort::ResourceError && m_open) {
            m_open = false;
            emit connectionStateChanged(false);
        }
    });
}

SlcanCommunication::~SlcanCommunication()
{
    close();
}

bool SlcanCommunication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (device.devId.trimmed().isEmpty())
        return setError(QStringLiteral("SLCAN 打开失败：设备编号不能为空。"), errorMessage);

    AdapterSettings settings;
    if (!parseAddress(device.devAddr, settings, errorMessage))
        return false;

    close();
    m_serialPort.setPortName(settings.portName);

    // CANable 2.0 SLCAN 使用 USB 虚拟串口；按项目现有适配器配置使用 1 Mbps。
    if (!m_serialPort.setBaudRate(1000000)
        || !m_serialPort.setDataBits(QSerialPort::Data8)
        || !m_serialPort.setParity(QSerialPort::NoParity)
        || !m_serialPort.setStopBits(QSerialPort::OneStop)
        || !m_serialPort.setFlowControl(QSerialPort::NoFlowControl)) {
        return setError(QStringLiteral("SLCAN 串口参数设置失败：%1")
                            .arg(m_serialPort.errorString()), errorMessage);
    }

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        return setError(QStringLiteral("SLCAN 打开串口 %1 失败：%2")
                            .arg(settings.portName, m_serialPort.errorString()),
                        errorMessage);
    }

    m_receiveBuffer.clear();

    QByteArray setupCommands;
    setupCommands.append('S');
    setupCommands.append(char('0' + settings.nominalBitrateIndex));
    setupCommands.append('\r');

    if (m_canFdMode) {
        setupCommands.append('Y');
        setupCommands.append(char('0' + settings.dataBitrateIndex));
        setupCommands.append('\r');
    }

    setupCommands.append("O\r");
    if (!writeCommand(setupCommands, errorMessage)) {
        m_serialPort.close();
        return false;
    }

    m_open = true;
    emit connectionStateChanged(true);
    return true;
}

void SlcanCommunication::close()
{
    const bool wasOpen = m_open;

    if (m_serialPort.isOpen()) {
        m_serialPort.write("C\r");
        m_serialPort.waitForBytesWritten(100);
        m_serialPort.close();
    }

    m_receiveBuffer.clear();
    m_open = false;
    if (wasOpen)
        emit connectionStateChanged(false);
}

bool SlcanCommunication::isOpen() const
{
    return m_open && m_serialPort.isOpen();
}

bool SlcanCommunication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("SLCAN 发送失败：适配器尚未打开。"), errorMessage);

    QByteArray command;
    if (!SlcanFrameCodec::normalizeCommand(data, m_canFdMode, command, errorMessage)) {
        const QString message = errorMessage ? *errorMessage
                                              : QStringLiteral("SLCAN 帧格式无效。");
        emit errorOccurred(message);
        return false;
    }

    return writeCommand(command, errorMessage);
}

bool SlcanCommunication::parseAddress(const QString &address,
                                      AdapterSettings &settings,
                                      QString *errorMessage)
{
    const QStringList parts = address.split(QLatin1Char(','));
    if (parts.size() < 2 || parts.size() > 3) {
        return setError(QStringLiteral(
            "SLCAN 地址格式错误，应为 COM4,1000000（CAN）或 "
            "COM4,1000000,2000000（CAN-FD）。"), errorMessage);
    }

    settings.portName = parts.at(0).trimmed();
    if (settings.portName.isEmpty())
        return setError(QStringLiteral("SLCAN 串口名不能为空。"), errorMessage);

    bool nominalOk = false;
    const int nominalBitrate = parts.at(1).trimmed().toInt(&nominalOk);
    static const int nominalRates[] = {
        10000, 20000, 50000, 100000, 125000,
        250000, 500000, 800000, 1000000
    };

    if (!nominalOk)
        return setError(QStringLiteral("SLCAN 仲裁段波特率无效。"), errorMessage);

    bool nominalFound = false;
    for (int i = 0; i < 9; ++i) {
        if (nominalRates[i] == nominalBitrate) {
            settings.nominalBitrateIndex = i;
            nominalFound = true;
            break;
        }
    }
    if (!nominalFound) {
        return setError(QStringLiteral(
            "SLCAN 仲裁段仅支持 10000、20000、50000、100000、125000、"
            "250000、500000、800000、1000000 bps。"), errorMessage);
    }

    if (!m_canFdMode && parts.size() == 3)
        return setError(QStringLiteral("经典 CAN 地址不应配置 CAN-FD 数据段波特率。"),
                        errorMessage);

    if (m_canFdMode && parts.size() == 3) {
        bool dataOk = false;
        const int dataBitrate = parts.at(2).trimmed().toInt(&dataOk);
        if (!dataOk || (dataBitrate != 2000000 && dataBitrate != 5000000)) {
            return setError(QStringLiteral("CAN-FD 数据段波特率仅支持 2000000 或 5000000 bps。"),
                            errorMessage);
        }
        settings.dataBitrateIndex = dataBitrate == 2000000 ? 2 : 5;
    }

    return true;
}

bool SlcanCommunication::writeCommand(const QByteArray &command,
                                      QString *errorMessage)
{
    const qint64 written = m_serialPort.write(command);
    if (written != command.size()) {
        return setError(QStringLiteral("SLCAN 串口写入失败：%1")
                            .arg(m_serialPort.errorString()), errorMessage);
    }
    return true;
}

bool SlcanCommunication::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;
    emit errorOccurred(message);
    return false;
}

void SlcanCommunication::readSerialData()
{
    m_receiveBuffer.append(m_serialPort.readAll());

    if (m_receiveBuffer.size() > 4096) {
        m_receiveBuffer.clear();
        emit errorOccurred(QStringLiteral("SLCAN 接收缓存超限，已清空缓存。"));
        return;
    }

    while (true) {
        const qsizetype end = m_receiveBuffer.indexOf('\r');
        if (end < 0)
            break;

        const QByteArray line = m_receiveBuffer.left(end);
        m_receiveBuffer.remove(0, end + 1);

        if (line.isEmpty() || line == QByteArray("#")
            || line.startsWith('M')) {
            continue;
        }

        if (line.contains(char(0x07))) {
            emit errorOccurred(QStringLiteral("SLCAN 适配器拒绝了命令（BEL）。"));
            continue;
        }

        SlcanFrame frame;
        QString parseError;
        if (!SlcanFrameCodec::parseFrame(line, m_canFdMode, frame, &parseError)) {
            emit errorOccurred(QStringLiteral("SLCAN 接收帧解析失败：%1")
                                   .arg(parseError));
            continue;
        }

        emit dataReceived(line);
    }
}
