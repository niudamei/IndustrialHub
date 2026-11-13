#ifndef IPROTOCOLPARSER_H
#define IPROTOCOLPARSER_H

#include "businessdatapoint.h"

#include <QByteArray>
#include <QString>
#include <QVector>

// 协议解析器只负责把一帧原始数据转换为统一业务变量，不负责通信和持久化。
class IProtocolParser
{
public:
    virtual ~IProtocolParser() = default;

    virtual QString protocolName() const = 0;

    // 解析失败时返回 false，并通过 errorMessage 给出原因。
    // 输出参数只会在整帧解析成功后被替换，避免产生半帧数据。
    virtual bool parse(const QString &deviceId,
                       const QByteArray &payload,
                       QVector<BusinessDataPoint> &points,
                       QString *errorMessage = nullptr) const = 0;
};

#endif // IPROTOCOLPARSER_H
