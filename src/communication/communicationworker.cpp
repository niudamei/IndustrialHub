#include "communicationworker.h"

#include <QMetaObject>
#include <QPointer>
#include <QDebug>

CommunicationWorker::CommunicationWorker(Factory factory, QObject *parent)
    : QObject(parent),
      m_context(new QObject),
      m_factory(std::move(factory))
{
    m_context->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_context, &QObject::deleteLater);
    m_thread.start();

    QObject *context = m_context;
    const bool invoked = QMetaObject::invokeMethod(context, [this, context]() {
        ICommunication *communication = m_factory ? m_factory(nullptr) : nullptr;
        if (!communication)
            return;

        connect(communication, &ICommunication::connectionStateChanged,
                context, [this](bool connected) {
            m_open.storeRelease(connected);
            QMetaObject::invokeMethod(this, [this, connected]() {
                emit connectionStateChanged(connected);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
        connect(communication, &ICommunication::dataReceived,
                context, [this](const QByteArray &data) {
            QMetaObject::invokeMethod(this, [this, data]() {
                emit dataReceived(data);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
        connect(communication, &ICommunication::errorOccurred,
                context, [this](const QString &message) {
            QMetaObject::invokeMethod(this, [this, message]() {
                emit errorOccurred(message);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);

        context->setProperty("communicationReady", true);
        context->setProperty("communicationPointer",
                             QVariant::fromValue<qulonglong>(
                                 reinterpret_cast<qulonglong>(communication)));
    }, Qt::BlockingQueuedConnection);

    if (invoked) {
        // The communication object is created in its owning I/O thread.
        m_ready.storeRelease(context->property("communicationReady").toBool());
    }
}

CommunicationWorker::~CommunicationWorker()
{
    if (m_context && m_thread.isRunning()) {
        QObject *context = m_context;
        QMetaObject::invokeMethod(context, [context]() {
            const qulonglong raw = context->property("communicationPointer").toULongLong();
            auto *communication = reinterpret_cast<ICommunication *>(raw);
            if (communication) {
                communication->close();
                delete communication;
            }
            context->setProperty("communicationPointer", QVariant());
            context->setProperty("communicationReady", false);
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
    const bool invoked = QMetaObject::invokeMethod(context, [context, &device, &result, &error]() {
        const qulonglong raw = context->property("communicationPointer").toULongLong();
        auto *communication = reinterpret_cast<ICommunication *>(raw);
        if (communication)
            result = communication->open(device, &error);
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
    QMetaObject::invokeMethod(context, [context]() {
        const qulonglong raw = context->property("communicationPointer").toULongLong();
        auto *communication = reinterpret_cast<ICommunication *>(raw);
        if (communication)
            communication->close();
    }, Qt::BlockingQueuedConnection);
}

bool CommunicationWorker::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!isReady() || !m_context) {
        if (errorMessage)
            *errorMessage = QStringLiteral("通信工作线程尚未就绪。");
        return false;
    }
    bool result = false;
    QString error;
    QObject *context = m_context;
    QMetaObject::invokeMethod(context, [context, &data, &result, &error]() {
        const qulonglong raw = context->property("communicationPointer").toULongLong();
        auto *communication = reinterpret_cast<ICommunication *>(raw);
        if (communication)
            result = communication->sendData(data, &error);
        else
            error = QStringLiteral("通信实例不存在。");
    }, Qt::BlockingQueuedConnection);
    if (errorMessage)
        *errorMessage = error;
    return result;
}

bool CommunicationWorker::isOpen() const
{
    return m_open.loadAcquire();
}
