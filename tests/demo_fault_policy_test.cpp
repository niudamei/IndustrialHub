#include "src/simulation/demofaultpolicy.h"

#include <iostream>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << std::endl; \
            return 1; \
        } \
    } while (false)

int main()
{
    using DemoFaultPolicy::Mode;

    CHECK(DemoFaultPolicy::modeFromId(QStringLiteral("delay")) == Mode::Delay,
          "delay mode is recognized");
    CHECK(DemoFaultPolicy::modeFromId(QStringLiteral("unknown")) == Mode::Normal,
          "unknown mode safely falls back to normal");
    CHECK(DemoFaultPolicy::isDemoDeviceId(QStringLiteral("demo-can-001")),
          "reserved CAN demo ID is recognized case-insensitively");
    CHECK(DemoFaultPolicy::isDemoDeviceId(QStringLiteral("DEMO-CANFD-002")),
          "reserved CAN-FD demo ID is recognized");
    CHECK(DemoFaultPolicy::isDemoDeviceId(QStringLiteral("DEMO-TCP-003")),
          "reserved TCP demo ID is recognized");
    CHECK(!DemoFaultPolicy::isDemoDeviceId(QStringLiteral("DEV001")),
          "real device IDs are never classified as demo");
    CHECK(!DemoFaultPolicy::isDemoDeviceId(QStringLiteral("DEMO-CAN-999")),
          "unknown demo-like IDs are not accepted");
    CHECK(DemoFaultPolicy::isDemoDeviceId(QStringLiteral(" DEMO-CAN-001 ")),
          "demo IDs are recognized after trimming whitespace");
    CHECK(DemoFaultPolicy::findReservedDemoIdCollision(
              QStringList{QStringLiteral("DEV001"), QStringLiteral(" demo-canfd-002 ")})
              == QStringLiteral("demo-canfd-002"),
          "legacy reserved IDs are detected after trimming and case folding");
    CHECK(DemoFaultPolicy::findReservedDemoIdCollision(
              QStringList{QStringLiteral("DEV001"), QStringLiteral("DEMO-CAN-999")}).isEmpty(),
          "ordinary and unknown demo-like IDs do not block simulator startup");
    CHECK(DemoFaultPolicy::dataSourceLabel(QStringLiteral("DEMO-CANFD-002"))
              == QStringLiteral("演示数据"),
          "reserved demo devices are labeled as demo data");
    CHECK(DemoFaultPolicy::dataSourceLabel(QStringLiteral(" DEV001 "))
              == QStringLiteral("驱动通信"),
          "non-demo device traffic is labeled as driver traffic");
    CHECK(DemoFaultPolicy::dataSourceLabel(QStringLiteral("--"))
              == QStringLiteral("系统"),
          "global events are labeled as system events");
    CHECK(DemoFaultPolicy::appliesToDevice(QString(), QStringLiteral("DEMO-CAN-001")),
          "empty target selects all demo devices");
    CHECK(DemoFaultPolicy::appliesToDevice(QStringLiteral("demo-can-001"),
                                           QStringLiteral("DEMO-CAN-001")),
          "device target comparison is case-insensitive");

    CHECK(DemoFaultPolicy::shouldDisconnect(Mode::Disconnect, QString(),
                                             QStringLiteral("DEMO-TCP-003")),
          "disconnect mode applies to all demo devices");
    CHECK(!DemoFaultPolicy::shouldDisconnect(Mode::Disconnect,
                                              QStringLiteral("DEMO-TCP-003"),
                                              QStringLiteral("DEMO-CAN-001")),
          "targeted disconnect leaves other demo devices connected");

    CHECK(DemoFaultPolicy::shouldDropRx(Mode::Drop, QString(),
                                         QStringLiteral("DEMO-CAN-001"), 3, 3),
          "every third RX frame is dropped");
    CHECK(!DemoFaultPolicy::shouldDropRx(Mode::Drop, QString(),
                                          QStringLiteral("DEMO-CAN-001"), 2, 3),
          "non-matching frame is retained");
    CHECK(!DemoFaultPolicy::shouldDropRx(Mode::Drop,
                                          QStringLiteral("DEMO-TCP-003"),
                                          QStringLiteral("DEMO-CAN-001"), 3, 3),
          "drop policy does not affect an unselected device");
    CHECK(!DemoFaultPolicy::shouldDropRx(Mode::Normal, QString(),
                                          QStringLiteral("DEMO-CAN-001"), 3, 3),
          "normal mode never drops RX frames");

    CHECK(DemoFaultPolicy::rxDelayMs(Mode::Delay, QString(),
                                      QStringLiteral("DEMO-CANFD-002"), 750) == 750,
          "delay mode returns the configured RX delay");
    CHECK(DemoFaultPolicy::rxDelayMs(Mode::Delay,
                                      QStringLiteral("DEMO-TCP-003"),
                                      QStringLiteral("DEMO-CANFD-002"), 750) == 0,
          "delay does not affect an unselected device");
    CHECK(DemoFaultPolicy::rxDelayMs(Mode::Delay, QString(),
                                      QStringLiteral("DEMO-CANFD-002"), 0) == 0,
          "non-positive delay is disabled");

    std::cout << "ALL DEMO FAULT POLICY TESTS PASSED." << std::endl;
    return 0;
}
