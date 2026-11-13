#include "modbusregistermap.h"

#include <QtMath>
#include <cstring>

namespace ModbusRegisterMap {
namespace {
int registerCount(DataType type)
{
    return (type == DataType::UInt16 || type == DataType::Int16) ? 1 : 2;
}

quint32 toUInt32(const QVector<quint16> &registers, WordOrder order)
{
    quint8 bytes[4] = {
        static_cast<quint8>(registers.at(0) >> 8),
        static_cast<quint8>(registers.at(0) & 0xFF),
        static_cast<quint8>(registers.at(1) >> 8),
        static_cast<quint8>(registers.at(1) & 0xFF)
    };
    quint8 normalized[4] = {};
    switch (order) {
    case WordOrder::ABCD:
        std::memcpy(normalized, bytes, 4);
        break;
    case WordOrder::CDAB:
        normalized[0] = bytes[2]; normalized[1] = bytes[3];
        normalized[2] = bytes[0]; normalized[3] = bytes[1];
        break;
    case WordOrder::BADC:
        normalized[0] = bytes[1]; normalized[1] = bytes[0];
        normalized[2] = bytes[3]; normalized[3] = bytes[2];
        break;
    case WordOrder::DCBA:
        normalized[0] = bytes[3]; normalized[1] = bytes[2];
        normalized[2] = bytes[1]; normalized[3] = bytes[0];
        break;
    }
    return (static_cast<quint32>(normalized[0]) << 24)
        | (static_cast<quint32>(normalized[1]) << 16)
        | (static_cast<quint32>(normalized[2]) << 8)
        | static_cast<quint32>(normalized[3]);
}
} // namespace

bool validate(const Mapping &mapping, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (mapping.variableId.trimmed().isEmpty())
        return fail(QStringLiteral("Modbus 变量 ID 不能为空。"));
    if (mapping.name.trimmed().isEmpty())
        return fail(QStringLiteral("Modbus 变量名称不能为空。"));
    if (mapping.function != 0x03 && mapping.function != 0x04)
        return fail(QStringLiteral("寄存器映射只支持 0x03 或 0x04 读取功能码。"));
    if (static_cast<quint32>(mapping.address) + registerCount(mapping.dataType) > 65536U)
        return fail(QStringLiteral("寄存器映射地址超出 16 位地址范围。"));
    if (!qIsFinite(mapping.scale) || !qIsFinite(mapping.offset))
        return fail(QStringLiteral("寄存器缩放系数和偏移量必须是有限数值。"));
    if (error) error->clear();
    return true;
}

bool decode(const Mapping &mapping, const QVector<quint16> &registers,
            const QString &deviceId, const QString &sourceProtocol,
            const QDateTime &timestamp, BusinessDataPoint &point,
            QString *error)
{
    if (!validate(mapping, error)) return false;
    if (deviceId.trimmed().isEmpty() || !timestamp.isValid())
    {
        if (error) *error = QStringLiteral("设备编号或采集时间无效。");
        return false;
    }
    if (registers.size() != registerCount(mapping.dataType)) {
        if (error) *error = QStringLiteral("响应寄存器数量与映射数据类型不匹配。");
        return false;
    }

    double rawValue = 0.0;
    switch (mapping.dataType) {
    case DataType::UInt16:
        rawValue = registers.at(0);
        if (mapping.wordOrder == WordOrder::BADC || mapping.wordOrder == WordOrder::DCBA)
            rawValue = ((registers.at(0) & 0x00FF) << 8)
                | ((registers.at(0) & 0xFF00) >> 8);
        break;
    case DataType::Int16: {
        quint16 raw = registers.at(0);
        if (mapping.wordOrder == WordOrder::BADC || mapping.wordOrder == WordOrder::DCBA)
            raw = static_cast<quint16>((raw << 8) | (raw >> 8));
        rawValue = static_cast<qint16>(raw);
        break;
    }
    case DataType::UInt32:
        rawValue = static_cast<double>(toUInt32(registers, mapping.wordOrder));
        break;
    case DataType::Int32:
        rawValue = static_cast<double>(static_cast<qint32>(
            toUInt32(registers, mapping.wordOrder)));
        break;
    case DataType::Float32: {
        const quint32 bits = toUInt32(registers, mapping.wordOrder);
        float value = 0.0F;
        static_assert(sizeof(float) == sizeof(quint32), "Float32 requires 32-bit float");
        std::memcpy(&value, &bits, sizeof(value));
        if (!qIsFinite(value)) {
            if (error) *error = QStringLiteral("Modbus Float32 解码结果不是有限数值。");
            return false;
        }
        rawValue = value;
        break;
    }
    }

    const double scaled = rawValue * mapping.scale + mapping.offset;
    if (!qIsFinite(scaled)) {
        if (error) *error = QStringLiteral("Modbus 缩放后的业务值不是有限数值。");
        return false;
    }
    point.deviceId = deviceId.trimmed();
    point.variableId = mapping.variableId.trimmed();
    point.name = mapping.name.trimmed();
    point.value = scaled;
    point.unit = mapping.unit;
    point.timestamp = timestamp;
    point.quality = DataQuality::Good;
    point.sourceProtocol = sourceProtocol;
    if (error) error->clear();
    return true;
}

} // namespace ModbusRegisterMap
