#ifndef JSONPROTOCOLPARSER_H
#define JSONPROTOCOLPARSER_H

#include "iprotocolparser.h"

// 通用 JSON 示例解析器，适合 JSON/TCP 类设备或模拟数据。
// 它不会自动应用到 CAN、CAN-FD 或未知私有协议设备。
class JsonProtocolParser final : public IProtocolParser
{
public:
    QString protocolName() const override;

    bool parse(const QString &deviceId,
               const QByteArray &payload,
               QVector<BusinessDataPoint> &points,
               QString *errorMessage = nullptr) const override;
};

#endif // JSONPROTOCOLPARSER_H
