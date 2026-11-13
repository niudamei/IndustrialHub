#include "modbuspoller.h"

#include "modbusmaster.h"

#include <QSet>

ModbusPoller::ModbusPoller(ModbusMaster *master, QString deviceId, QObject *parent)
    : QObject(parent), m_master(master), m_deviceId(std::move(deviceId))
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ModbusPoller::pollOnce);
    if (master) {
        connect(master, &ModbusMaster::requestCompleted,
                this, &ModbusPoller::onRequestCompleted);
        connect(master, &ModbusMaster::requestFailed,
                this, &ModbusPoller::onRequestFailed);
    }
}

bool ModbusPoller::setMappings(
    const QVector<ModbusRegisterMap::Mapping> &mappings, QString *error)
{
    if (m_cycleActive) {
        if (error) *error = QStringLiteral("当前轮询周期尚未完成，不能修改寄存器映射。");
        return false;
    }
    if (mappings.isEmpty()) {
        if (error) *error = QStringLiteral("至少需要配置一个 Modbus 寄存器映射。");
        return false;
    }
    QSet<QString> ids;
    for (const auto &mapping : mappings) {
        QString validationError;
        if (!ModbusRegisterMap::validate(mapping, &validationError)) {
            if (error) *error = validationError;
            return false;
        }
        const QString key = mapping.variableId.trimmed().toCaseFolded();
        if (ids.contains(key)) {
            if (error) *error = QStringLiteral("变量 ID 重复：%1").arg(mapping.variableId);
            return false;
        }
        ids.insert(key);
    }
    m_mappings = mappings;
    if (error) error->clear();
    return true;
}

QVector<ModbusRegisterMap::Mapping> ModbusPoller::mappings() const
{
    return m_mappings;
}

void ModbusPoller::start(int intervalMs)
{
    if (!m_master || m_deviceId.trimmed().isEmpty() || m_mappings.isEmpty()) {
        emit pollError(m_deviceId, QString(),
            QStringLiteral("Modbus 轮询器缺少主站、设备编号或寄存器映射。"));
        return;
    }
    m_intervalMs = qBound(100, intervalMs, 60000);
    m_running = true;
    if (!m_cycleActive) pollOnce();
}

void ModbusPoller::stop()
{
    m_running = false;
    m_timer.stop();
}

bool ModbusPoller::isRunning() const { return m_running; }
bool ModbusPoller::isPolling() const { return m_cycleActive; }

void ModbusPoller::pollOnce()
{
    if (m_cycleActive) return;
    if (!m_master || m_deviceId.trimmed().isEmpty() || m_mappings.isEmpty()) {
        emit pollError(m_deviceId, QString(),
            QStringLiteral("Modbus 轮询器尚未配置完成。"));
        return;
    }

    m_cycleActive = true;
    m_cyclePending = 0;
    m_successfulPoints = 0;
    m_failedPoints = 0;
    m_requestMapping.clear();

    for (int index = 0; index < m_mappings.size(); ++index) {
        const auto &mapping = m_mappings.at(index);
        const quint64 requestId = mapping.function == 0x03
            ? m_master->readHoldingRegisters(mapping.address,
                mapping.dataType == ModbusRegisterMap::DataType::UInt16
                    || mapping.dataType == ModbusRegisterMap::DataType::Int16 ? 1 : 2)
            : m_master->readInputRegisters(mapping.address,
                mapping.dataType == ModbusRegisterMap::DataType::UInt16
                    || mapping.dataType == ModbusRegisterMap::DataType::Int16 ? 1 : 2);
        if (requestId == 0) {
            ++m_failedPoints;
            emit pollError(m_deviceId, mapping.variableId,
                QStringLiteral("无法将寄存器读取请求加入 Modbus 主站队列。"));
            continue;
        }
        m_requestMapping.insert(requestId, index);
        ++m_cyclePending;
    }
    if (m_cyclePending == 0) finishCycle();
}

void ModbusPoller::onRequestCompleted(
    quint64 requestId, const ModbusCodec::Response &response)
{
    const auto it = m_requestMapping.find(requestId);
    if (it == m_requestMapping.end()) return;
    const int mappingIndex = it.value();
    m_requestMapping.erase(it);
    const auto mapping = m_mappings.at(mappingIndex);
    if (!response.ok) {
        ++m_failedPoints;
        emit pollError(m_deviceId, mapping.variableId,
            response.error.isEmpty() ? QStringLiteral("Modbus 设备返回异常响应。") : response.error);
        finishOne(false);
        return;
    }

    BusinessDataPoint point;
    const QString source = m_master->transport() == ModbusCodec::Transport::Rtu
        ? QStringLiteral("Modbus RTU") : QStringLiteral("Modbus TCP");
    QString error;
    if (!ModbusRegisterMap::decode(mapping, response.registers, m_deviceId,
                                   source, QDateTime::currentDateTime(), point, &error)) {
        ++m_failedPoints;
        emit pollError(m_deviceId, mapping.variableId, error);
        finishOne(false);
        return;
    }
    ++m_successfulPoints;
    emit pointsReady(m_deviceId, {point});
    finishOne(true);
}

void ModbusPoller::onRequestFailed(quint64 requestId, const QString &reason)
{
    const auto it = m_requestMapping.find(requestId);
    if (it == m_requestMapping.end()) return;
    const int mappingIndex = it.value();
    m_requestMapping.erase(it);
    ++m_failedPoints;
    emit pollError(m_deviceId, m_mappings.at(mappingIndex).variableId, reason);
    finishOne(false);
}

void ModbusPoller::finishOne(bool)
{
    if (m_cyclePending > 0) --m_cyclePending;
    if (m_cyclePending == 0) finishCycle();
}

void ModbusPoller::finishCycle()
{
    if (!m_cycleActive) return;
    m_cycleActive = false;
    emit cycleFinished(m_successfulPoints, m_failedPoints);
    if (m_running) m_timer.start(m_intervalMs);
}
