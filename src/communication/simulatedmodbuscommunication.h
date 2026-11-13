#ifndef SIMULATEDMODBUSCOMMUNICATION_H
#define SIMULATEDMODBUSCOMMUNICATION_H

#include "icommunication.h"

#include <QHash>

class SimulatedModbusCommunication : public ICommunication
{
    Q_OBJECT
public:
    explicit SimulatedModbusCommunication(QObject *parent = nullptr);
    ~SimulatedModbusCommunication() override;

    bool open(const DeviceInfo &device, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool sendData(const QByteArray &data, QString *errorMessage = nullptr) override;

private:
    bool setError(const QString &message, QString *errorMessage);
    QByteArray makeException(quint8 function, quint8 exceptionCode) const;
    QByteArray addCrc(QByteArray frame) const;

    QString m_deviceId;
    quint8 m_unitId = 1;
    bool m_open = false;
    QHash<quint16, quint16> m_registers;
};

#endif // SIMULATEDMODBUSCOMMUNICATION_H
