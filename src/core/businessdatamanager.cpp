#include "businessdatamanager.h"

#include <algorithm>

BusinessDataManager::BusinessDataManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<BusinessDataPoint>();
    qRegisterMetaType<QVector<BusinessDataPoint>>();
}

QString BusinessDataManager::key(const QString &value)
{
    return value.trimmed().toCaseFolded();
}

int BusinessDataManager::deviceCount() const
{
    return m_latestValues.size();
}

int BusinessDataManager::variableCount(const QString &deviceId) const
{
    if (deviceId.trimmed().isEmpty()) {
        int total = 0;
        for (auto it = m_latestValues.cbegin(); it != m_latestValues.cend(); ++it)
            total += it.value().size();
        return total;
    }
    return m_latestValues.value(key(deviceId)).size();
}

bool BusinessDataManager::latestValue(const QString &deviceId,
                                      const QString &variableId,
                                      BusinessDataPoint &point) const
{
    const auto deviceIt = m_latestValues.constFind(key(deviceId));
    if (deviceIt == m_latestValues.cend())
        return false;

    const auto valueIt = deviceIt.value().constFind(key(variableId));
    if (valueIt == deviceIt.value().cend())
        return false;

    point = valueIt.value();
    return true;
}

QList<BusinessDataPoint> BusinessDataManager::latestValues(const QString &deviceId) const
{
    QList<BusinessDataPoint> result;
    if (!deviceId.trimmed().isEmpty()) {
        const auto values = m_latestValues.value(key(deviceId));
        result.reserve(values.size());
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            result.append(it.value());
    } else {
        for (auto deviceIt = m_latestValues.cbegin();
             deviceIt != m_latestValues.cend(); ++deviceIt) {
            for (auto valueIt = deviceIt.value().cbegin();
                 valueIt != deviceIt.value().cend(); ++valueIt) {
                result.append(valueIt.value());
            }
        }
    }

    std::sort(result.begin(), result.end(),
              [](const BusinessDataPoint &a, const BusinessDataPoint &b) {
        const int deviceCompare = QString::compare(a.deviceId, b.deviceId,
                                                   Qt::CaseInsensitive);
        if (deviceCompare != 0)
            return deviceCompare < 0;
        return QString::compare(a.variableId, b.variableId,
                                Qt::CaseInsensitive) < 0;
    });
    return result;
}

void BusinessDataManager::updateDataPoints(
    const QString &deviceId,
    const QVector<BusinessDataPoint> &points)
{
    const QString deviceKey = key(deviceId);
    if (deviceKey.isEmpty() || points.isEmpty())
        return;

    QVector<BusinessDataPoint> accepted;
    accepted.reserve(points.size());

    auto deviceIt = m_latestValues.find(deviceKey);
    const bool hadDevice = deviceIt != m_latestValues.end();
    QHash<QString, BusinessDataPoint> pendingValues =
        hadDevice ? deviceIt.value() : QHash<QString, BusinessDataPoint>();

    for (const BusinessDataPoint &point : points) {
        if (point.deviceId.trimmed().isEmpty()
            || key(point.deviceId) != deviceKey
            || point.variableId.trimmed().isEmpty()
            || !point.timestamp.isValid()) {
            continue;
        }

        const QString variableKey = key(point.variableId);
        if (variableKey.isEmpty())
            continue;

        const auto oldIt = pendingValues.constFind(variableKey);
        if (oldIt != pendingValues.cend()
            && oldIt.value().timestamp > point.timestamp) {
            continue; // 防止较旧数据覆盖更新的数据
        }

        pendingValues.insert(variableKey, point);
        accepted.append(point);
    }

    if (!accepted.isEmpty()) {
        m_latestValues.insert(deviceKey, pendingValues);
        emit valuesUpdated(deviceId.trimmed(), accepted);
    }
}

void BusinessDataManager::clearDevice(const QString &deviceId)
{
    const QString deviceKey = key(deviceId);
    if (m_latestValues.remove(deviceKey) > 0)
        emit valuesCleared(deviceId.trimmed());
}

void BusinessDataManager::clear()
{
    if (m_latestValues.isEmpty())
        return;
    m_latestValues.clear();
    emit valuesCleared(QString());
}
