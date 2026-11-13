#include "protocolmanager.h"

QString ProtocolManager::normalizedKey(const QString &value)
{
    return value.trimmed().toCaseFolded();
}

ProtocolManager::ProtocolManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<BusinessDataPoint>();
    qRegisterMetaType<QVector<BusinessDataPoint>>();
}

bool ProtocolManager::registerParser(
    const QString &parserId,
    std::shared_ptr<const IProtocolParser> parser,
    QString *errorMessage)
{
    const QString key = normalizedKey(parserId);
    if (key.isEmpty() || !parser)
        return setError(QStringLiteral("解析器编号和解析器实例不能为空。"), errorMessage);
    if (m_parsers.contains(key))
        return setError(QStringLiteral("解析器编号已注册：%1").arg(parserId), errorMessage);

    m_parsers.insert(key, std::move(parser));
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool ProtocolManager::bindDevice(const QString &deviceId,
                                 const QString &parserId,
                                 QString *errorMessage)
{
    const QString deviceKey = normalizedKey(deviceId);
    const QString parserKey = normalizedKey(parserId);
    if (deviceKey.isEmpty())
        return setError(QStringLiteral("设备编号不能为空。"), errorMessage);
    if (!m_parsers.contains(parserKey))
        return setError(QStringLiteral("解析器尚未注册：%1").arg(parserId), errorMessage);

    m_deviceBindings.insert(deviceKey, parserKey);
    if (errorMessage)
        errorMessage->clear();
    return true;
}

void ProtocolManager::unbindDevice(const QString &deviceId)
{
    m_deviceBindings.remove(normalizedKey(deviceId));
}

QString ProtocolManager::parserForDevice(const QString &deviceId) const
{
    const QString parserKey = m_deviceBindings.value(normalizedKey(deviceId));
    return m_parsers.contains(parserKey) ? parserKey : QString();
}

bool ProtocolManager::parseFrame(const QString &deviceId,
                                 const QByteArray &payload,
                                 QString *errorMessage)
{
    const QString normalizedDeviceId = deviceId.trimmed();
    if (normalizedDeviceId.isEmpty())
        return setError(QStringLiteral("设备编号不能为空。"), errorMessage);

    const QString parserKey = parserForDevice(normalizedDeviceId);
    if (parserKey.isEmpty())
        return setError(QStringLiteral("设备 %1 尚未绑定协议解析器。")
                            .arg(normalizedDeviceId), errorMessage);

    QVector<BusinessDataPoint> points;
    QString parseError;
    const auto parser = m_parsers.value(parserKey);
    if (!parser->parse(normalizedDeviceId, payload, points, &parseError)) {
        if (errorMessage)
            *errorMessage = parseError;
        emit parseFailed(normalizedDeviceId, parseError);
        return false;
    }

    emit businessDataDecoded(normalizedDeviceId, points);
    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool ProtocolManager::setError(const QString &message,
                               QString *errorMessage) const
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}
