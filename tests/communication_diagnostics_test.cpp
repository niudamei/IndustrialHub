#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "src/communication/communicationmanager.h"
#include "src/core/communicationdiagnostics.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("IndustrialHubTests"));
    QCoreApplication::setApplicationName(QStringLiteral("CommunicationDiagnosticsTest"));
    QStandardPaths::setTestModeEnabled(true);
    const QString dbPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                               .filePath(QStringLiteral("industrialhub.db"));
    QFile::remove(dbPath);

    CommunicationManager manager;
    {
    CommunicationDiagnostics diagnostics(&manager);
    if (!diagnostics.isReady()) {
        qCritical() << "FAIL: diagnostics initialization:" << diagnostics.lastError();
        return 1;
    }

    emit manager.deviceConnectionStateChanged(QStringLiteral("DEV-A"), true);
    emit manager.deviceDataSent(QStringLiteral("DEV-A"), QByteArray(4, char(0x11)));
    emit manager.deviceDataReceived(QStringLiteral("DEV-A"), QByteArray(7, char(0x22)));
    emit manager.deviceErrorOccurred(QStringLiteral("DEV-A"), QStringLiteral("serial port timeout"));
    emit manager.deviceReconnecting(QStringLiteral("DEV-A"), 2, 2000);
    emit manager.deviceDataSent(QStringLiteral("DEV-B"), QByteArray(2, char(0x33)));
    diagnostics.recordHealthTimeout(QStringLiteral("DEV-A"), 3500);

    const auto a = diagnostics.statisticsFor(QStringLiteral("DEV-A"));
    const auto b = diagnostics.statisticsFor(QStringLiteral("DEV-B"));
    const auto events = diagnostics.recentEvents(100);
    const auto deviceEvents = diagnostics.recentEvents(100, QStringLiteral("DEV-A"));
    const auto timeEvents = diagnostics.recentEvents(100, QString(),
        QDateTime::currentDateTime().addSecs(-60),
        QDateTime::currentDateTime().addSecs(60));

    if (a.txCount != 1 || a.rxCount != 1 || a.txBytes != 4 || a.rxBytes != 7
        || a.errorCount != 1 || a.reconnectCount != 1
        || b.txCount != 1 || b.txBytes != 2 || events.size() != 6
        || deviceEvents.size() != 6 || timeEvents.size() != 7) {
        qCritical() << "FAIL: unexpected statistics/events"
                    << a.txCount << a.rxCount << a.txBytes << a.rxBytes
                    << a.errorCount << a.reconnectCount << b.txCount << events.size();
        return 1;
    }

    bool foundPortError = false;
    for (const auto &event : events)
        foundPortError = foundPortError || event.category == QStringLiteral("超时");
    if (!foundPortError) {
        qCritical() << "FAIL: timeout classification not found";
        return 1;
    }

    }
    {
        CommunicationDiagnostics reopened(nullptr);
        if (!reopened.isReady() || reopened.statisticsFor(QStringLiteral("DEV-A")).txCount != 1
            || reopened.recentEvents(100).size() != 7) {
            qCritical() << "FAIL: diagnostic events were not persisted across reopen";
            return 1;
        }
    }
    qInfo() << "PASS: diagnostics persistence, event capture, classification and per-device statistics";
    return 0;
}
