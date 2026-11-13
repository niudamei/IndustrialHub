#include "src/core/devicehealthmonitor.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << std::endl; return 1; } } while (false)

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    DeviceHealthMonitor monitor;
    monitor.setTimeoutMs(300);
    monitor.registerDevice(QStringLiteral(" DEV-A "));
    monitor.registerDevice(QStringLiteral("DEV-B"));

    int onlineChanges = 0;
    int timeoutSignals = 0;
    QObject::connect(&monitor, &DeviceHealthMonitor::deviceOnlineChanged,
                     [&](const QString &, bool) { ++onlineChanges; });
    QObject::connect(&monitor, &DeviceHealthMonitor::deviceTimedOut,
                     [&](const QString &, qint64) { ++timeoutSignals; });

    monitor.setConnected(QStringLiteral("dev-a"), true);
    monitor.setConnected(QStringLiteral("DEV-B"), true);
    monitor.markActivity(QStringLiteral("DEV-A"));
    CHECK(monitor.isOnline(QStringLiteral("DEV-A")), "RX activity marks only its connected device online");
    CHECK(!monitor.isOnline(QStringLiteral("DEV-B")), "connected device without RX is not online");

    QEventLoop waitLoop;
    QTimer::singleShot(450, &waitLoop, &QEventLoop::quit);
    waitLoop.exec();
    CHECK(!monitor.isOnline(QStringLiteral("DEV-A")), "silent device transitions offline after timeout");
    CHECK(timeoutSignals == 2, "first-frame and online silence timeouts are each emitted once");
    CHECK(!monitor.isOnline(QStringLiteral("DEV-B")), "device without a first RX frame remains offline");

    monitor.setConnected(QStringLiteral("DEV-A"), false);
    monitor.markActivity(QStringLiteral("DEV-A"));
    CHECK(!monitor.isOnline(QStringLiteral("DEV-A")), "activity while disconnected does not mark online");
    monitor.setConnected(QStringLiteral("DEV-A"), true);
    monitor.markActivity(QStringLiteral("DEV-A"));
    CHECK(monitor.isOnline(QStringLiteral("DEV-A")), "RX activity restores online state");
    CHECK(onlineChanges == 3, "online/offline/recovery transitions are emitted exactly once");

    monitor.setConnected(QStringLiteral("DEV-A"), false);
    CHECK(!monitor.isOnline(QStringLiteral("DEV-A")), "manual disconnect forces offline");
    CHECK(monitor.lastSeen(QStringLiteral("DEV-A")).isValid() == false,
          "manual disconnect clears stale last-seen timestamp");
    const int timeoutCountAfterDisconnect = timeoutSignals;
    QEventLoop disconnectWait;
    QTimer::singleShot(400, &disconnectWait, &QEventLoop::quit);
    disconnectWait.exec();
    CHECK(timeoutSignals == timeoutCountAfterDisconnect,
          "manual disconnect does not produce a later timeout event");

    std::cout << "ALL DEVICE HEALTH MONITOR TESTS PASSED." << std::endl;
    return 0;
}
