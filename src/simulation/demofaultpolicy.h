#ifndef DEMOFAULTPOLICY_H
#define DEMOFAULTPOLICY_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace DemoFaultPolicy {

enum class Mode
{
    Normal,
    Drop,
    Disconnect,
    Delay
};

inline Mode modeFromId(const QString &id)
{
    const QString normalized = id.trimmed().toCaseFolded();
    if (normalized == QStringLiteral("drop"))
        return Mode::Drop;
    if (normalized == QStringLiteral("disconnect"))
        return Mode::Disconnect;
    if (normalized == QStringLiteral("delay"))
        return Mode::Delay;
    return Mode::Normal;
}

inline bool isDemoDeviceId(const QString &deviceId)
{
    const QString normalized = deviceId.trimmed();
    return normalized.compare(QStringLiteral("DEMO-CAN-001"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("DEMO-CANFD-002"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("DEMO-TCP-003"), Qt::CaseInsensitive) == 0;
}

inline QString findReservedDemoIdCollision(const QStringList &registeredDeviceIds)
{
    for (const QString &deviceId : registeredDeviceIds) {
        const QString normalized = deviceId.trimmed();
        if (isDemoDeviceId(normalized))
            return normalized;
    }
    return {};
}

inline QString dataSourceLabel(const QString &deviceId)
{
    const QString normalized = deviceId.trimmed();
    if (normalized.isEmpty() || normalized == QStringLiteral("--"))
        return QStringLiteral("系统");
    return isDemoDeviceId(normalized)
        ? QStringLiteral("演示数据")
        : QStringLiteral("驱动通信");
}

inline bool appliesToDevice(const QString &targetDeviceId, const QString &deviceId)
{
    return targetDeviceId.trimmed().isEmpty()
        || targetDeviceId.compare(deviceId, Qt::CaseInsensitive) == 0;
}

inline bool shouldDisconnect(Mode mode,
                             const QString &targetDeviceId,
                             const QString &deviceId)
{
    return mode == Mode::Disconnect
        && appliesToDevice(targetDeviceId, deviceId);
}

inline bool shouldDropRx(Mode mode,
                         const QString &targetDeviceId,
                         const QString &deviceId,
                         quint64 frameCount,
                         int dropEvery)
{
    if (mode != Mode::Drop
        || !appliesToDevice(targetDeviceId, deviceId)
        || frameCount == 0) {
        return false;
    }

    const quint64 interval = static_cast<quint64>(dropEvery > 0 ? dropEvery : 1);
    return frameCount % interval == 0;
}

inline int rxDelayMs(Mode mode,
                     const QString &targetDeviceId,
                     const QString &deviceId,
                     int configuredDelayMs)
{
    if (mode != Mode::Delay
        || !appliesToDevice(targetDeviceId, deviceId)
        || configuredDelayMs <= 0) {
        return 0;
    }

    return configuredDelayMs;
}

} // namespace DemoFaultPolicy

#endif // DEMOFAULTPOLICY_H
