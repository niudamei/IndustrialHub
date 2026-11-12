#ifndef SIMULATEDCOMMUNICATION_H
#define SIMULATEDCOMMUNICATION_H

#include "icommunication.h"

// 用于没有真实硬件时验证上层通信流程的回环模拟器。
// sendData() 成功后会延迟触发 dataReceived()，返回相同的数据。
class SimulatedCommunication : public ICommunication
{
    Q_OBJECT

public:
    explicit SimulatedCommunication(QObject *parent = nullptr);
    ~SimulatedCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data,
                  QString *errorMessage = nullptr) override;

private:
    bool setError(const QString &message, QString *errorMessage);

    bool m_open = false;
    QString m_deviceId;
};

#endif // SIMULATEDCOMMUNICATION_H
