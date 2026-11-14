#ifndef MODBUSTCPSIMULATOR_H
#define MODBUSTCPSIMULATOR_H

#include <QObject>
#include <QModbusDataUnit>
#include <QModbusDataUnitMap>

class QModbusTcpServer;

class ModbusTcpSimulator : public QObject
{
    Q_OBJECT
public:
    explicit ModbusTcpSimulator(QObject *parent = nullptr);
    ~ModbusTcpSimulator() override;

    bool start(const QString &address, quint16 port, int unitId = 1,
               QString *errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    quint16 port() const;
    int unitId() const;
    bool setRegister(QModbusDataUnit::RegisterType type, quint16 address,
                     quint16 value);
    bool registerValue(QModbusDataUnit::RegisterType type, quint16 address,
                       quint16 *value) const;
    QModbusTcpServer *server() const;

signals:
    void runningChanged(bool running);
    void registerWritten(QModbusDataUnit::RegisterType type, int address, int size);
    void errorOccurred(const QString &message);

private:
    QModbusTcpServer *m_server = nullptr;
    quint16 m_port = 0;
    int m_unitId = 1;
};

#endif
