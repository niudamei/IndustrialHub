#ifndef DEMOFAULTPOLICY_H
#define DEMOFAULTPOLICY_H

#include <QString>
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
