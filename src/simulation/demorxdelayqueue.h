#ifndef DEMORXDELAYQUEUE_H
#define DEMORXDELAYQUEUE_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QTimer;

// Owns delayed demo RX timers so they can be cancelled before delivery.
class DemoRxDelayQueue final : public QObject
{
    Q_OBJECT

public:
    explicit DemoRxDelayQueue(int capacity = 256, QObject *parent = nullptr);
    ~DemoRxDelayQueue() override;

    bool schedule(const QString &deviceId, const QByteArray &data, int delayMs);
    int cancelAll();
    int pendingCount() const noexcept;
    int capacity() const noexcept;
    int peakPendingCount() const noexcept;
    quint64 deliveredCount() const noexcept;
    quint64 cancelledCount() const noexcept;
    quint64 queueFullCount() const noexcept;

signals:
    void frameReady(const QString &deviceId, const QByteArray &data, int delayMs);
    void queueFull(const QString &deviceId);

private:
    struct PendingFrame
    {
        QString deviceId;
        QByteArray data;
        int delayMs = 0;
    };

    QHash<QTimer *, PendingFrame> m_pending;
    int m_capacity = 256;
    int m_peakPendingCount = 0;
    quint64 m_deliveredCount = 0;
    quint64 m_cancelledCount = 0;
    quint64 m_queueFullCount = 0;
};

#endif // DEMORXDELAYQUEUE_H
