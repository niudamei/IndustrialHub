#ifndef DEMORXDELAYQUEUE_H
#define DEMORXDELAYQUEUE_H

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

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
};

#endif // DEMORXDELAYQUEUE_H
