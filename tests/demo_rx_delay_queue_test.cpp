#include "src/simulation/demorxdelayqueue.h"

#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QTimer>
#include <QStringList>
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


    // Exercise the production queue capacity with all three demo device identities.
    DemoRxDelayQueue burstQueue(256);
    const QStringList burstDeviceIds = {
        QStringLiteral("DEMO-CAN-001"),
        QStringLiteral("DEMO-CANFD-002"),
        QStringLiteral("DEMO-TCP-003")
    };
    CHECK(!burstQueue.schedule(QString(), QByteArray("invalid"), 25),
          "empty device IDs are rejected");
    CHECK(!burstQueue.schedule(QStringLiteral("DEMO-CAN-001"), QByteArray("invalid"), 0),
          "non-positive delays are rejected");
    CHECK(burstQueue.pendingCount() == 0 && burstQueue.queueFullCount() == 0,
          "invalid requests do not consume capacity or count as queue overflow");

    constexpr int BurstFrameCount = 256;
    QVector<int> burstPerDevice(3, 0);
    int burstDelivered = 0;
    bool unexpectedBurstFrame = false;
    bool burstTimedOut = false;
    QEventLoop burstLoop;
    QTimer burstDeadline;
    burstDeadline.setSingleShot(true);

    QObject::connect(&burstQueue, &DemoRxDelayQueue::frameReady, &burstLoop,
                     [&](const QString &deviceId, const QByteArray &data, int) {
        if (deviceId == burstDeviceIds.at(0))
            ++burstPerDevice[0];
        else if (deviceId == burstDeviceIds.at(1))
            ++burstPerDevice[1];
        else if (deviceId == burstDeviceIds.at(2))
            ++burstPerDevice[2];
        else
            unexpectedBurstFrame = true;

        if (data.isEmpty())
            unexpectedBurstFrame = true;

        ++burstDelivered;
        if (burstDelivered == BurstFrameCount)
            burstLoop.quit();
    });
    QObject::connect(&burstDeadline, &QTimer::timeout, &burstLoop, [&]() {
        burstTimedOut = true;
        burstLoop.quit();
    });

    for (int i = 0; i < BurstFrameCount; ++i) {
        CHECK(burstQueue.schedule(burstDeviceIds.at(i % burstDeviceIds.size()),
                                  QByteArray::number(i), 25),
              "production-capacity burst accepts every in-capacity frame");
    }
    CHECK(!burstQueue.schedule(QStringLiteral("DEMO-CAN-001"),
                               QByteArray("overflow"), 25),
          "production-capacity burst rejects the first over-capacity frame");
    CHECK(burstQueue.pendingCount() == BurstFrameCount,
          "burst pending count is bounded by the production capacity");
    CHECK(burstQueue.peakPendingCount() == BurstFrameCount,
          "burst records the full production-capacity high-water mark");
    CHECK(burstQueue.queueFullCount() == 1,
          "burst records exactly one over-capacity rejection");

    burstDeadline.start(3000);
    if (burstDelivered < BurstFrameCount)
        burstLoop.exec();
    burstDeadline.stop();

    CHECK(!burstTimedOut, "all in-capacity burst frames are delivered before the deadline");
    CHECK(burstDelivered == BurstFrameCount,
          "every accepted burst frame is delivered exactly once");
    CHECK(!unexpectedBurstFrame, "burst delivery preserves valid device identity and payload");
    CHECK(burstPerDevice.at(0) == 86
              && burstPerDevice.at(1) == 85
              && burstPerDevice.at(2) == 85,
          "burst frames are distributed across CAN, CAN-FD and TCP demo devices");
    CHECK(burstQueue.pendingCount() == 0,
          "burst delivery drains all pending frames");
    CHECK(burstQueue.deliveredCount() == static_cast<quint64>(BurstFrameCount),
          "burst delivery counter matches accepted frames");
    CHECK(burstQueue.cancelledCount() == 0,
          "successfully delivered burst frames are not counted as cancelled");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(burstQueue.findChildren<QTimer *>().isEmpty(),
          "delivered single-shot timers are released after deferred deletion");

    // Repeated stop/reconfigure cycles must not retain active timers or emit stale RX.
    DemoRxDelayQueue churnQueue(3);
    int staleChurnDeliveries = 0;
    QObject::connect(&churnQueue, &DemoRxDelayQueue::frameReady, &app,
                     [&](const QString &, const QByteArray &, int) {
        ++staleChurnDeliveries;
    });
    constexpr int ChurnCycles = 50;
    for (int cycle = 0; cycle < ChurnCycles; ++cycle) {
        for (const QString &deviceId : burstDeviceIds) {
            CHECK(churnQueue.schedule(deviceId, QByteArray::number(cycle), 5000),
                  "reconfigure cycle schedules one frame per demo device");
        }
        CHECK(churnQueue.pendingCount() == 3,
              "reconfigure cycle fills only its three-device capacity");
        CHECK(churnQueue.cancelAll() == 3,
              "reconfigure cycle cancels all pending demo RX frames");
        CHECK(churnQueue.pendingCount() == 0,
              "reconfigure cycle releases all queue capacity");
        CHECK(churnQueue.cancelAll() == 0,
              "repeated cancellation is idempotent");
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(churnQueue.findChildren<QTimer *>().isEmpty(),
          "repeated cancellation releases every owned timer");
    CHECK(churnQueue.cancelledCount() == static_cast<quint64>(ChurnCycles * 3),
          "repeated cancellation accounting is exact");
    CHECK(churnQueue.peakPendingCount() == 3,
          "repeated cancellation preserves the observed high-water mark");
    waitFor(30);
    CHECK(staleChurnDeliveries == 0,
          "rapid stop/reconfigure cycles never deliver stale delayed RX frames");

    std::cout << "ALL DEMO RX DELAY QUEUE TESTS PASSED." << std::endl;
    return 0;
}
