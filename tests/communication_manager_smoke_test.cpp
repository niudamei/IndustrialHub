#include "src/communication/communicationmanager.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QAtomicPointer>
#include <iostream>
#include <functional>

class ThreadProbeCommunication : public ICommunication
{
public:
    explicit ThreadProbeCommunication(QObject *parent = nullptr)
        : ICommunication(parent), m_ownerThread(QThread::currentThread()) {}

    bool open(const DeviceInfo &, QString *) override
    {
        m_openThread = QThread::currentThread();
        m_open = true;
        emit connectionStateChanged(true);
        return true;
    }

    void close() override
    {
        m_open = false;
        emit connectionStateChanged(false);
    }

    bool isOpen() const override { return m_open; }

    bool sendData(const QByteArray &data, QString *) override
    {
        m_sendThread.storeRelease(QThread::currentThread());
        if (!m_open)
            return false;
        emit dataReceived(data);
        return true;
    }

    QThread *ownerThread() const { return m_ownerThread; }
    QThread *openThread() const { return m_openThread; }
    QThread *sendThread() const { return m_sendThread.loadAcquire(); }

private:
    QThread *m_ownerThread = nullptr;
    QThread *m_openThread = nullptr;
    QAtomicPointer<QThread> m_sendThread{nullptr};
    bool m_open = false;
};


static bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return condition();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    CommunicationManager manager;
    QString error;

    // 模拟通信：验证管理器能够按设备编号路由数据，并发布 TX/RX 事件。
    DeviceInfo simulated;
    simulated.devId = QStringLiteral("SIM-001");
    simulated.devName = QStringLiteral("模拟设备");
    simulated.commType = QStringLiteral("SIMULATED");

    QByteArray simulatedRx;
    QByteArray simulatedTx;
    bool simulatedConnected = false;
    QObject::connect(&manager, &CommunicationManager::deviceConnectionStateChanged,
                     [&](const QString &id, bool connected) {
        if (id == simulated.devId && connected)
            simulatedConnected = true;
    });
    QObject::connect(&manager, &CommunicationManager::deviceDataSent,
                     [&](const QString &id, const QByteArray &data) {
        if (id == simulated.devId)
            simulatedTx.append(data);
    });
    QObject::connect(&manager, &CommunicationManager::deviceDataReceived,
                     [&](const QString &id, const QByteArray &data) {
        if (id == simulated.devId)
            simulatedRx.append(data);
    });

    if (!manager.connectDevice(simulated, &error)) {
        std::cerr << "FAIL: simulated connect: " << error.toStdString() << std::endl;
        return 1;
    }
    if (!waitUntil([&]() { return simulatedConnected && manager.isConnected(simulated.devId); }, 500)) {
        std::cerr << "FAIL: simulated device did not connect." << std::endl;
        return 2;
    }

    const QByteArray simulatedPayload = QByteArray::fromHex("11223344");
    if (!manager.sendData(simulated.devId, simulatedPayload, &error)
        || !waitUntil([&]() { return simulatedTx == simulatedPayload; }, 1000)
        || !waitUntil([&]() { return simulatedRx == simulatedPayload; }, 1000)) {
        std::cerr << "FAIL: simulated TX/RX routing failed: " << error.toStdString() << std::endl;
        return 3;
    }
    if (!manager.isConnected(QStringLiteral("sim-001"))) {
        std::cerr << "FAIL: device identifiers should be case-insensitive." << std::endl;
        return 10;
    }
    std::cout << "PASS: manager routes simulated TX/RX data and normalizes device IDs." << std::endl;

    // TCP：本机启动临时服务端，不依赖真实设备或外部网络。
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        std::cerr << "FAIL: local TCP server listen failed." << std::endl;
        return 4;
    }

    QTcpSocket *tcpPeer = nullptr;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&]() {
        tcpPeer = server.nextPendingConnection();
        QObject::connect(tcpPeer, &QTcpSocket::readyRead, tcpPeer, [peer = tcpPeer]() {
            peer->write(peer->readAll());
        });
    });

    DeviceInfo tcp;
    tcp.devId = QStringLiteral("TCP-001");
    tcp.devName = QStringLiteral("本地 TCP 设备");
    tcp.commType = QStringLiteral("TCP");
    tcp.devAddr = QStringLiteral("127.0.0.1:%1").arg(server.serverPort());

    QByteArray tcpRx;
    QByteArray tcpTx;
    bool tcpConnected = false;
    bool tcpDisconnected = false;
    int tcpConnectCount = 0;
    QObject::connect(&manager, &CommunicationManager::deviceConnectionStateChanged,
                     [&](const QString &id, bool connected) {
        if (id == tcp.devId && connected) {
            tcpConnected = true;
            ++tcpConnectCount;
        } else if (id == tcp.devId) {
            tcpDisconnected = true;
        }
    });
    QObject::connect(&manager, &CommunicationManager::deviceDataSent,
                     [&](const QString &id, const QByteArray &data) {
        if (id == tcp.devId)
            tcpTx.append(data);
    });
    QObject::connect(&manager, &CommunicationManager::deviceDataReceived,
                     [&](const QString &id, const QByteArray &data) {
        if (id == tcp.devId)
            tcpRx.append(data);
    });

    if (!manager.connectDevice(tcp, &error)
        || !waitUntil([&]() { return tcpConnected && manager.isConnected(tcp.devId); }, 2000)) {
        std::cerr << "FAIL: manager TCP connect: " << error.toStdString() << std::endl;
        return 5;
    }

    const QByteArray tcpPayload = QByteArray::fromHex("A1B2C3D4");
    if (!manager.sendData(tcp.devId, tcpPayload, &error)
        || !waitUntil([&]() { return tcpTx == tcpPayload; }, 1000)
        || !waitUntil([&]() { return tcpRx == tcpPayload; }, 2000)) {
        std::cerr << "FAIL: manager TCP TX/RX routing failed: " << error.toStdString() << std::endl;
        return 6;
    }
    std::cout << "PASS: manager publishes TCP TX and routes TCP RX data." << std::endl;

    // TCP 连接意外中断后，管理器应按退避策略自动恢复连接。
    if (!tcpPeer) {
        std::cerr << "FAIL: TCP server did not accept the initial connection." << std::endl;
        return 12;
    }
    const int initialConnectCount = tcpConnectCount;
    tcpPeer->abort();
    if (!waitUntil([&]() { return tcpDisconnected; }, 1000)) {
        std::cerr << "FAIL: TCP disconnect event was not observed." << std::endl;
        return 13;
    }
    if (!waitUntil([&]() { return tcpConnectCount > initialConnectCount
                                  && manager.isConnected(tcp.devId); }, 5000)) {
        std::cerr << "FAIL: TCP automatic reconnect did not restore the connection." << std::endl;
        return 14;
    }
    std::cout << "PASS: TCP connection automatically recovers after an unexpected disconnect." << std::endl;

    // 每个设备的驱动对象、open 和 send 都必须运行在独立 I/O 线程。
    ThreadProbeCommunication *probe = nullptr;
    if (!manager.registerFactory(QStringLiteral("THREAD-PROBE"),
            [&probe](QObject *parent) {
                probe = new ThreadProbeCommunication(parent);
                return probe;
            })) {
        std::cerr << "FAIL: could not register thread probe factory." << std::endl;
        return 16;
    }
    DeviceInfo probeDevice;
    probeDevice.devId = QStringLiteral("THREAD-001");
    probeDevice.commType = QStringLiteral("THREAD-PROBE");
    if (!manager.connectDevice(probeDevice, &error)
        || !waitUntil([&]() { return manager.isConnected(probeDevice.devId); }, 1000)) {
        std::cerr << "FAIL: worker-thread probe device did not connect." << std::endl;
        return 17;
    }
    if (!probe || probe->ownerThread() == QThread::currentThread()
        || probe->ownerThread() != probe->openThread()) {
        std::cerr << "FAIL: communication driver was not created/opened in its I/O thread." << std::endl;
        return 18;
    }
    if (!manager.sendData(probeDevice.devId, QByteArray::fromHex("CAFE"), &error)
        || !waitUntil([&]() { return probe->sendThread() != nullptr; }, 1000)
        || probe->sendThread() != probe->ownerThread()) {
        std::cerr << "FAIL: sendData did not execute in the device I/O thread." << std::endl;
        return 19;
    }
    std::cout << "PASS: communication driver creation, open and send run off the UI thread." << std::endl;

    ThreadProbeCommunication *probe2 = nullptr;
    manager.registerFactory(QStringLiteral("THREAD-PROBE-2"),
            [&probe2](QObject *parent) {
        probe2 = new ThreadProbeCommunication(parent);
        return probe2;
    });
    DeviceInfo probeDevice2;
    probeDevice2.devId = QStringLiteral("THREAD-002");
    probeDevice2.commType = QStringLiteral("THREAD-PROBE-2");
    if (!manager.connectDevice(probeDevice2, &error)
        || !waitUntil([&]() { return manager.isConnected(probeDevice2.devId); }, 1000)
        || !probe2 || probe2->ownerThread() == probe->ownerThread()) {
        std::cerr << "FAIL: different devices must have independent I/O threads." << std::endl;
        return 24;
    }
    std::cout << "PASS: each device owns an independent I/O thread." << std::endl;

    // 首次 TCP 连接失败后，设备仍应进入自动重连流程。
    QTcpServer lateServer;
    if (!lateServer.listen(QHostAddress::LocalHost, 0)) {
        std::cerr << "FAIL: could not reserve a port for initial-connect retry test." << std::endl;
        return 20;
    }
    const quint16 latePort = lateServer.serverPort();
    lateServer.close();

    DeviceInfo lateTcp;
    lateTcp.devId = QStringLiteral("TCP-LATE-001");
    lateTcp.commType = QStringLiteral("TCP");
    lateTcp.devAddr = QStringLiteral("127.0.0.1:%1").arg(latePort);
    bool lateReconnectScheduled = false;
    bool lateTcpConnected = false;
    QObject::connect(&manager, &CommunicationManager::deviceReconnecting,
                     [&](const QString &id, int, int) {
        if (id == lateTcp.devId)
            lateReconnectScheduled = true;
    });
    QObject::connect(&manager, &CommunicationManager::deviceConnectionStateChanged,
                     [&](const QString &id, bool connected) {
        if (id == lateTcp.devId && connected)
            lateTcpConnected = true;
    });
    if (!manager.connectDevice(lateTcp, &error)
        || !waitUntil([&]() { return lateReconnectScheduled; }, 2500)) {
        std::cerr << "FAIL: initial TCP failure did not schedule automatic reconnect." << std::endl;
        return 21;
    }
    if (!lateServer.listen(QHostAddress::LocalHost, latePort)) {
        std::cerr << "FAIL: could not start TCP server after initial failure." << std::endl;
        return 22;
    }
    if (!waitUntil([&]() { return lateTcpConnected && manager.isConnected(lateTcp.devId); }, 4000)) {
        std::cerr << "FAIL: TCP did not recover after the initial connection failure." << std::endl;
        return 23;
    }
    std::cout << "PASS: initial TCP connection failure is retried and recovers." << std::endl;

    if (!manager.isConnected(simulated.devId) || !manager.isConnected(tcp.devId)) {
        std::cerr << "FAIL: managing one device affected another device." << std::endl;
        return 7;
    }

    manager.disconnectDevice(tcp.devId);
    if (!waitUntil([&]() { return !manager.isConnected(tcp.devId); }, 500)) {
        std::cerr << "FAIL: TCP device did not disconnect." << std::endl;
        return 8;
    }
    const int tcpConnectCountAfterManualDisconnect = tcpConnectCount;
    waitUntil([&]() { return false; }, 1200);
    if (tcpConnectCount != tcpConnectCountAfterManualDisconnect) {
        std::cerr << "FAIL: manual TCP disconnect unexpectedly triggered auto reconnect." << std::endl;
        return 15;
    }
    if (!manager.isConnected(simulated.devId)) {
        std::cerr << "FAIL: disconnecting TCP also disconnected simulated device." << std::endl;
        return 9;
    }

    manager.releaseDevice(simulated.devId);
    manager.releaseDevice(tcp.devId);
    manager.releaseDevice(probeDevice.devId);
    manager.releaseDevice(probeDevice2.devId);
    manager.releaseDevice(lateTcp.devId);
    if (!manager.managedDeviceIds().isEmpty()) {
        std::cerr << "FAIL: released devices remain managed." << std::endl;
        return 11;
    }
    std::cout << "PASS: per-device disconnect and release." << std::endl;
    std::cout << "ALL COMMUNICATION MANAGER TESTS PASSED." << std::endl;
    return 0;
}
