#ifndef SIMULATORPAGE_H
#define SIMULATORPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QTableWidget;
class QTimer;
class ModbusTcpSimulator;

class SimulatorPage : public QWidget
{
    Q_OBJECT
public:
    explicit SimulatorPage(QWidget *parent = nullptr);
    ~SimulatorPage() override;

private:
    void refreshRegisters();
    void setStatus(const QString &text, bool running);

    ModbusTcpSimulator *m_modbus = nullptr;
    QLineEdit *m_addressEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QSpinBox *m_unitSpin = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_registerTable = nullptr;
    QTimer *m_valueTimer = nullptr;
    quint32 m_tick = 0;
};

#endif
