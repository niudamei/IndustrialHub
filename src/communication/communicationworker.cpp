#include "communicationworker.h"

#include <QMetaObject>

CommunicationWorker::CommunicationWorker(Factory factory, QObject *parent)
    : QObject(parent),
      m_context(new QObject)
{
    m_context->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_context, &QObject::deleteLater);
    m_thread.start();

    QObject *context = m_context;
    bool created = false;
    const bool invoked = QMetaObject::invokeMethod(context, [this, factory, &created]() {
        m_communication = factory ? factory(nullptr) : nullptr;
        created = m_communication != nullptr;
        if (!m_communication)
            return;

        connect(m_communication, &ICommunication::connectionStateChanged,
                m_context, [this](bool connected) {
            m_open.storeRelease(connected);
            QMetaObject::invokeMethod(this, [this, connected]() {
                emit connectionStateChanged(connected);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
        connect(m_communication, &ICommunication::dataReceived,
                m_context, [this](const QByteArray &data) {
            QMetaObject::invokeMethod(this, [this, data]() {
                emit dataReceived(data);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
        connect(m_communication, &ICommunication::errorOccurred,
                m_context, [this](const QString &message) {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit errorOccurred(message);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
    }, Qt::BlockingQueuedConnection);

    m_ready.storeRelease(invoked && created);
}

CommunicationWorker::~CommunicationWorker()
{
    if (m_context && m_thread.isRunning()) {
        QObject *context = m_context;
        QMetaObject::invokeMethod(context, [this]() {
            if (m_communication) {
                m_communication->close();
                delete m_communication;
                m_communication = nullptr;
            }
        }, Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait();
        m_context = nullptr;
    }
}

bool CommunicationWorker::isReady() const
{
    return m_ready.loadAcquire();
}

bool CommunicationWorker::open(const DeviceInfo &device, QString *errorMessage)
{
    if (!isReady()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("通信工作线程初始化失败。");
        return false;
    }

    bool result = false;
    QString error;
    QObject *context = m_context;
    const bool invoked = QMetaObject::invokeMethod(context, [this, &device, &result, &error]() {
        if (m_communication)
            result = m_communication->open(device, &error);
        else
            error = QStringLiteral("通信实例不存在。");
    }, Qt::BlockingQueuedConnection);
    if (!invoked)
        error = QStringLiteral("无法向通信工作线程提交连接请求。");
    if (errorMessage)
        *errorMessage = error;
    return invoked && result;
}

void CommunicationWorker::close()
{
    if (!isReady() || !m_context)
        return;
    QObject *context = m_context;
    QMetaObject::invokeMethod(context, [this]() {
        if (m_communication)
            m_communication->close();
    }, Qt::BlockingQueuedConnection);
}

bool CommunicationWorker::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isReady() || !m_context) {
        if (errorMessage)
            *errorMessage = QStringLiteral("通信工作线程尚未就绪。");
        return false;
    }
    if (!isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("设备尚未连接，发送请求未入队。");
        return false;
    }

    QObject *context = m_context;
    const bool invoked = QMetaObject::invokeMethod(context, [this, data]() {
        if (!m_communication)
            return;
        QString error;
        if (m_communication->sendData(data, &error)) {
            QMetaObject::invokeMethod(this, [this, data]() {
                emit dataSent(data);
            }, Qt::QueuedConnection);
        }
    }, Qt::QueuedConnection);

    if (!invoked && errorMessage)
        *errorMessage = QStringLiteral("无法向设备 I/O 线程提交发送请求。");
    return invoked;
}

bool CommunicationWorker::isOpen() const
{
    return m_open.loadAcquire();
}
