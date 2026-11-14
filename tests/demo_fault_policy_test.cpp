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
