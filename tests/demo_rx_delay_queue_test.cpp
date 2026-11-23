#include "src/simulation/demorxdelayqueue.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <iostream>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << std::endl; \
            return 1; \
        } \
    } while (false)

namespace {
void waitFor(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    DemoRxDelayQueue queue(2);
    int queueFullCount = 0;
    QObject::connect(&queue, &DemoRxDelayQueue::queueFull,
                     [&queueFullCount](const QString &) {
        ++queueFullCount;
    });

    CHECK(queue.capacity() == 2, "configured queue capacity is retained");
    CHECK(queue.pendingCount() == 0, "queue starts empty");
    CHECK(queue.peakPendingCount() == 0, "high-water mark starts at zero");
    CHECK(queue.deliveredCount() == 0, "delivered counter starts at zero");
    CHECK(queue.cancelledCount() == 0, "cancelled counter starts at zero");
    CHECK(queue.queueFullCount() == 0, "queue-full counter starts at zero");
    CHECK(queue.schedule(QStringLiteral("DEMO-CAN-001"), QByteArray("one"), 1000),
          "first delayed RX is scheduled");
    CHECK(queue.schedule(QStringLiteral("DEMO-CANFD-002"), QByteArray("two"), 1000),
          "second delayed RX is scheduled");
    CHECK(!queue.schedule(QStringLiteral("DEMO-TCP-003"), QByteArray("three"), 1000),
          "queue rejects a frame when capacity is reached");
    CHECK(queue.pendingCount() == 2, "pending count never exceeds capacity");
    CHECK(queue.peakPendingCount() == 2, "high-water mark records queue saturation");
    CHECK(queueFullCount == 1, "queue saturation is reported exactly once");
    CHECK(queue.queueFullCount() == 1, "queue-full counter records rejected frames");
    CHECK(queue.cancelAll() == 2, "cancelAll reports every cancelled timer");
    CHECK(queue.pendingCount() == 0, "cancellation releases all queue capacity");
    CHECK(queue.cancelledCount() == 2, "cancelled counter records cleared frames");

    QVector<QString> deliveredIds;
    QVector<QByteArray> deliveredPayloads;
    QVector<int> deliveredDelays;
    QEventLoop deliveryLoop;
    QTimer deliveryDeadline;
    deliveryDeadline.setSingleShot(true);

    QObject::connect(&queue, &DemoRxDelayQueue::frameReady, &deliveryLoop,
                     [&](const QString &deviceId, const QByteArray &data, int delayMs) {
        deliveredIds.append(deviceId);
        deliveredPayloads.append(data);
        deliveredDelays.append(delayMs);
        deliveryLoop.quit();
    });
    QObject::connect(&deliveryDeadline, &QTimer::timeout,
                     &deliveryLoop, &QEventLoop::quit);

    CHECK(queue.schedule(QStringLiteral("DEMO-CAN-001"), QByteArray("stale"), 80),
          "old-configuration frame is scheduled");
    CHECK(queue.cancelAll() == 1, "old-configuration frame is cancelled");
    CHECK(queue.cancelledCount() == 3, "cancellation counter accumulates across reconfiguration");
    CHECK(queue.schedule(QStringLiteral("DEMO-TCP-003"), QByteArray("fresh"), 20),
          "new-configuration frame can be scheduled immediately");

    deliveryDeadline.start(300);
    deliveryLoop.exec();

    CHECK(deliveredIds.size() == 1, "only the new frame is delivered");
    CHECK(deliveredIds.first() == QStringLiteral("DEMO-TCP-003"),
          "delivered frame retains its device identity");
    CHECK(deliveredPayloads.first() == QByteArray("fresh"),
          "delivered frame retains its payload");
    CHECK(deliveredDelays.first() == 20,
          "delivered frame retains the configured delay");
    CHECK(queue.pendingCount() == 0, "delivered timer is removed from pending queue");
    CHECK(queue.deliveredCount() == 1, "delivered counter increments exactly once");
    CHECK(queue.peakPendingCount() == 2, "high-water mark is retained after draining");

    // Wait beyond the cancelled timer's original deadline: it must not arrive late.
    waitFor(100);
    CHECK(deliveredIds.size() == 1, "cancelled frame never arrives after its old deadline");

    int callbacksAfterOwnerDestruction = 0;
    auto *temporaryQueue = new DemoRxDelayQueue(1);
    QObject::connect(temporaryQueue, &DemoRxDelayQueue::frameReady, &app,
                     [&callbacksAfterOwnerDestruction](const QString &,
                                                       const QByteArray &, int) {
        ++callbacksAfterOwnerDestruction;
    });
    CHECK(temporaryQueue->schedule(QStringLiteral("DEMO-CAN-001"),
                                   QByteArray("owner-gone"), 30),
          "temporary queue schedules a frame before destruction");
    delete temporaryQueue;
    waitFor(60);
    CHECK(callbacksAfterOwnerDestruction == 0,
          "destroying the queue owner prevents delayed callbacks");

    std::cout << "ALL DEMO RX DELAY QUEUE TESTS PASSED." << std::endl;
    return 0;
}
