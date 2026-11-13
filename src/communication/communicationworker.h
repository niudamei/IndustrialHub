#ifndef COMMUNICATIONWORKER_H
#define COMMUNICATIONWORKER_H

#include "src/communication/icommunication.h"

#include <QAtomicInteger>
#include <QObject>
#include <QThread>
#include <functional>

class CommunicationWorker : public QObject
{
    Q_OBJECT
public:
    using Factory = std::function<ICommunication *(QObject *parent)>;

    explicit CommunicationWorker(Factory factory, QObject *parent = nullptr);
    ~CommunicationWorker() override;

    bool isReady() const;
    bool open(const DeviceInfo &device, QString *errorMessage = nullptr);
    void close();
    bool sendData(const QByteArray &data, QString *errorMessage = nullptr);
    bool isOpen() const;

signals:
    void connectionStateChanged(bool connected);
    void dataReceived(const QByteArray &data);
    void dataSent(const QByteArray &data);
    void errorOccurred(const QString &message);

private:
    QThread m_thread;
    QObject *m_context = nullptr;
    ICommunication *m_communication = nullptr; // accessed only in m_thread
    Factory m_factory;
    QAtomicInteger<bool> m_ready{false};
    QAtomicInteger<bool> m_open{false};
};

#endif // COMMUNICATIONWORKER_H
