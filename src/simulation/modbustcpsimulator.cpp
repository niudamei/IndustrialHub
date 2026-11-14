#include "modbustcpsimulator.h"

#include <QModbusTcpServer>
#include <QModbusDevice>
#include <QModbusDataUnit>

ModbusTcpSimulator::ModbusTcpSimulator(QObject *parent)
    : QObject(parent), m_server(new QModbusTcpServer(this))
{
    connect(m_server, &QModbusServer::dataWritten, this,
            &ModbusTcpSimulator::registerWritten);
    connect(m_server, &QModbusDevice::errorOccurred, this,
            [this](QModbusDevice::Error) {
        if (m_server && m_server->error() != QModbusDevice::NoError)
            emit errorOccurred(m_server->errorString());
    });
    connect(m_server, &QModbusDevice::stateChanged, this,
            [this](QModbusDevice::State state) {
        if (state == QModbusDevice::UnconnectedState) {
            const bool wasRunning = m_port != 0;
            m_port = 0;
            if (wasRunning)
                emit runningChanged(false);
        }
    });
}

ModbusTcpSimulator::~ModbusTcpSimulator()
{
    stop();
}

bool ModbusTcpSimulator::start(const QString &address, quint16 port, int unitId,
                               QString *errorMessage)
{
    const QString host = address.trimmed();
    if (host.isEmpty() || port == 0 || unitId < 1 || unitId > 247) {
        if (errorMessage)
            *errorMessage = QStringLiteral("监听地址、端口或 Unit ID 无效（Unit ID 范围为 1–247）。");
        return false;
    }
    if (isRunning())
        stop();

    QModbusDataUnitMap map;
    map.insert(QModbusDataUnit::Coils,
               QModbusDataUnit(QModbusDataUnit::Coils, 0, 128));
    map.insert(QModbusDataUnit::DiscreteInputs,
               QModbusDataUnit(QModbusDataUnit::DiscreteInputs, 0, 128));
    map.insert(QModbusDataUnit::HoldingRegisters,
               QModbusDataUnit(QModbusDataUnit::HoldingRegisters, 0, 128));
    map.insert(QModbusDataUnit::InputRegisters,
               QModbusDataUnit(QModbusDataUnit::InputRegisters, 0, 128));
    if (!m_server->setMap(map)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("初始化 Modbus 寄存器映射失败。");
        return false;
    }

    m_server->setServerAddress(unitId);
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter, host);
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    if (!m_server->connectDevice()) {
        if (errorMessage)
            *errorMessage = m_server->errorString();
        return false;
    }
    m_port = port;
    m_unitId = unitId;

    // Demo values are explicitly illustrative, not a real device register map.
    m_server->setData(QModbusDataUnit::InputRegisters, 0, 253); // 25.3 C, x0.1
    m_server->setData(QModbusDataUnit::InputRegisters, 1, 125); // 1.25 A, x0.01
    m_server->setData(QModbusDataUnit::InputRegisters, 2, 1200);
    m_server->setData(QModbusDataUnit::InputRegisters, 3, 1);
    m_server->setData(QModbusDataUnit::HoldingRegisters, 0, 100);
    if (errorMessage)
        errorMessage->clear();
    emit runningChanged(true);
    return true;
}

void ModbusTcpSimulator::stop()
{
    if (!m_server || m_server->state() == QModbusDevice::UnconnectedState) {
        m_port = 0;
        return;
    }
    m_server->disconnectDevice();
    m_port = 0;
    emit runningChanged(false);
}

bool ModbusTcpSimulator::isRunning() const
{
    return m_server && m_server->state() == QModbusDevice::ConnectedState;
}

quint16 ModbusTcpSimulator::port() const { return m_port; }
int ModbusTcpSimulator::unitId() const { return m_unitId; }

bool ModbusTcpSimulator::setRegister(QModbusDataUnit::RegisterType type,
                                     quint16 address, quint16 value)
{
    if (!isRunning() || address >= 128)
        return false;
    return m_server->setData(type, address, value);
}

bool ModbusTcpSimulator::registerValue(QModbusDataUnit::RegisterType type,
                                       quint16 address, quint16 *value) const
{
    if (!isRunning() || !value || address >= 128)
        return false;
    return m_server->data(type, address, value);
}

QModbusTcpServer *ModbusTcpSimulator::server() const { return m_server; }
