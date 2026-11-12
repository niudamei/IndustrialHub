#ifndef ICOMMUNICATION_H
#define ICOMMUNICATION_H

#include <QObject>
#include <QByteArray>
#include <QString>

#include "src/pages/deviceinfo.h"

// 所有底层通信驱动的统一接口。
// TCP、UDP、串口、CAN 等实现类只需实现这组操作，页面无需感知具体协议。
class ICommunication : public QObject
{
    Q_OBJECT

public:
    explicit ICommunication(QObject *parent = nullptr);
    ~ICommunication() override = default;

    ICommunication(const ICommunication &) = delete;
    ICommunication &operator=(const ICommunication &) = delete;

    virtual bool open(const DeviceInfo &device, QString *errorMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool sendData(const QByteArray &data,
                          QString *errorMessage = nullptr) = 0;

signals:
    void connectionStateChanged(bool connected);
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
};

#endif // ICOMMUNICATION_H
