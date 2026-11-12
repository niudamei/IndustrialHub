#include "rs485communication.h"

#include <QStringList>

Rs485Communication::Rs485Communication(QObject *parent)
    : ICommunication(parent)
{
    connect(&m_serialPort, &QSerialPort::readyRead, this, [this]() {
        const QByteArray data = m_serialPort.readAll();
        if (!data.isEmpty())
            emit dataReceived(data);
    });

    connect(&m_serialPort, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError)
            return;

        emit errorOccurred(QStringLiteral("RS485 串口错误：%1")
                               .arg(m_serialPort.errorString()));

        if (error == QSerialPort::ResourceError && m_open) {
            m_open = false;
            emit connectionStateChanged(false);
        }
    });
}

Rs485Communication::~Rs485Communication()
{
    close();
}

bool Rs485Communication::open(const DeviceInfo &device, QString *errorMessage)
{
    if (device.devId.trimmed().isEmpty())
        return setError(QStringLiteral("RS485 打开失败：设备编号不能为空。"), errorMessage);

    SerialSettings settings;
    if (!parseAddress(device.devAddr, settings, errorMessage))
        return false;

    if (isOpen() && m_serialPort.portName() == settings.portName)
        close();
    else if (m_serialPort.isOpen())
        close();

    m_serialPort.setPortName(settings.portName);

    if (!m_serialPort.setBaudRate(settings.baudRate)
        || !m_serialPort.setDataBits(settings.dataBits)
        || !m_serialPort.setParity(settings.parity)
        || !m_serialPort.setStopBits(settings.stopBits)
        || !m_serialPort.setFlowControl(settings.flowControl)) {
        return setError(QStringLiteral("RS485 串口参数设置失败：%1")
                            .arg(m_serialPort.errorString()), errorMessage);
    }

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        return setError(QStringLiteral("RS485 打开串口 %1 失败：%2")
                            .arg(settings.portName, m_serialPort.errorString()),
                        errorMessage);
    }

    m_open = true;
    emit connectionStateChanged(true);
    return true;
}

void Rs485Communication::close()
{
    const bool wasOpen = m_open;
    if (m_serialPort.isOpen())
        m_serialPort.close();

    m_open = false;
    if (wasOpen)
        emit connectionStateChanged(false);
}

bool Rs485Communication::isOpen() const
{
    return m_open && m_serialPort.isOpen();
}

bool Rs485Communication::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isOpen())
        return setError(QStringLiteral("RS485 发送失败：串口尚未打开。"), errorMessage);

    const qint64 written = m_serialPort.write(data);
    if (written < 0 || written != data.size()) {
        return setError(QStringLiteral("RS485 发送失败：%1")
                            .arg(m_serialPort.errorString()), errorMessage);
    }
    return true;
}

bool Rs485Communication::parseAddress(const QString &address,
                                      SerialSettings &settings,
                                      QString *errorMessage)
{
    const QStringList parts = address.split(QLatin1Char(','));
    if (parts.size() < 5 || parts.size() > 6) {
        return setError(QStringLiteral(
            "RS485 地址格式错误，应为 串口名,波特率,数据位,校验位,停止位[,流控]，"
            "例如 COM3,115200,8,N,1。"), errorMessage);
    }

    settings.portName = parts.at(0).trimmed();
    bool baudOk = false;
    const int baud = parts.at(1).trimmed().toInt(&baudOk);
    const QString dataBits = parts.at(2).trimmed();
    const QString parity = parts.at(3).trimmed().toUpper();
    const QString stopBits = parts.at(4).trimmed();

    if (settings.portName.isEmpty() || !baudOk || baud <= 0) {
        return setError(QStringLiteral("RS485 串口名或波特率无效。"), errorMessage);
    }
    settings.baudRate = baud;

    if (dataBits == QStringLiteral("5"))
        settings.dataBits = QSerialPort::Data5;
    else if (dataBits == QStringLiteral("6"))
        settings.dataBits = QSerialPort::Data6;
    else if (dataBits == QStringLiteral("7"))
        settings.dataBits = QSerialPort::Data7;
    else if (dataBits == QStringLiteral("8"))
        settings.dataBits = QSerialPort::Data8;
    else
        return setError(QStringLiteral("RS485 数据位只支持 5、6、7、8。"), errorMessage);

    if (parity == QStringLiteral("N"))
        settings.parity = QSerialPort::NoParity;
    else if (parity == QStringLiteral("E"))
        settings.parity = QSerialPort::EvenParity;
    else if (parity == QStringLiteral("O"))
        settings.parity = QSerialPort::OddParity;
    else if (parity == QStringLiteral("M"))
        settings.parity = QSerialPort::MarkParity;
    else if (parity == QStringLiteral("S"))
        settings.parity = QSerialPort::SpaceParity;
    else
        return setError(QStringLiteral("RS485 校验位只支持 N、E、O、M、S。"), errorMessage);

    if (stopBits == QStringLiteral("1"))
        settings.stopBits = QSerialPort::OneStop;
    else if (stopBits == QStringLiteral("1.5"))
        settings.stopBits = QSerialPort::OneAndHalfStop;
    else if (stopBits == QStringLiteral("2"))
        settings.stopBits = QSerialPort::TwoStop;
    else
        return setError(QStringLiteral("RS485 停止位只支持 1、1.5、2。"), errorMessage);

    if (parts.size() == 6) {
        const QString flow = parts.at(5).trimmed().toUpper();
        if (flow == QStringLiteral("N"))
            settings.flowControl = QSerialPort::NoFlowControl;
        else if (flow == QStringLiteral("H"))
            settings.flowControl = QSerialPort::HardwareControl;
        else if (flow == QStringLiteral("S"))
            settings.flowControl = QSerialPort::SoftwareControl;
        else
            return setError(QStringLiteral("RS485 流控只支持 N、H、S。"), errorMessage);
    }

    return true;
}

bool Rs485Communication::setError(const QString &message, QString *errorMessage)
{
    if (errorMessage)
        *errorMessage = message;
    emit errorOccurred(message);
    return false;
}
