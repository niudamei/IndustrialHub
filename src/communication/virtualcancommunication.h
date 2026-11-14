#ifndef VIRTUALCANCOMMUNICATION_H
#define VIRTUALCANCOMMUNICATION_H

#include "icommunication.h"

#include <QPointer>

class QCanBusDevice;

// Qt SerialBus VirtualCAN 后端。ICommunication 数据沿用项目 SLCAN 文本帧约定，
// 内部转换为 QCanBusFrame；地址格式 can0 或 tcp://host:port/can0。
class VirtualCanCommunication : public ICommunication
{
    Q_OBJECT
public:
    explicit VirtualCanCommunication(bool canFd, QObject *parent = nullptr);
    ~VirtualCanCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data, QString *errorMessage = nullptr) override;

private:
    bool m_canFd = false;
    QPointer<QCanBusDevice> m_device;
};

#endif // VIRTUALCANCOMMUNICATION_H
