#include "SimulatorPage.h"

#include "src/simulation/modbustcpsimulator.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QModbusDataUnit>

SimulatorPage::SimulatorPage(QWidget *parent)
    : QWidget(parent), m_modbus(new ModbusTcpSimulator(this))
{
    auto *root = new QVBoxLayout(this);
    auto *intro = new QLabel(
        QStringLiteral("独立模拟设备区：启动标准 Modbus TCP 从站，供通信配置中的 Modbus TCP 主站连接。"
                       "此处的寄存器值仅为演示数据，不代表真实设备地址。"), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *connectionBox = new QGroupBox(QStringLiteral("Modbus TCP 从站"), this);
    auto *connectionLayout = new QHBoxLayout(connectionBox);
    m_addressEdit = new QLineEdit(QStringLiteral("127.0.0.1"), connectionBox);
    m_portSpin = new QSpinBox(connectionBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(1502);
    m_unitSpin = new QSpinBox(connectionBox);
    m_unitSpin->setRange(1, 247);
    m_unitSpin->setValue(1);
    m_startButton = new QPushButton(QStringLiteral("启动模拟从站"), connectionBox);
    m_stopButton = new QPushButton(QStringLiteral("停止"), connectionBox);
    connectionLayout->addWidget(new QLabel(QStringLiteral("监听地址"), connectionBox));
    connectionLayout->addWidget(m_addressEdit);
    connectionLayout->addWidget(new QLabel(QStringLiteral("端口"), connectionBox));
    connectionLayout->addWidget(m_portSpin);
    connectionLayout->addWidget(new QLabel(QStringLiteral("Unit ID"), connectionBox));
    connectionLayout->addWidget(m_unitSpin);
    connectionLayout->addWidget(m_startButton);
    connectionLayout->addWidget(m_stopButton);
    root->addWidget(connectionBox);

    m_statusLabel = new QLabel(QStringLiteral("状态：未启动"), this);
    root->addWidget(m_statusLabel);
    m_registerTable = new QTableWidget(8, 4, this);
    m_registerTable->setHorizontalHeaderLabels({
        QStringLiteral("寄存器类型"), QStringLiteral("地址（0起始）"),
        QStringLiteral("当前值"), QStringLiteral("说明")
    });
    m_registerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_registerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_registerTable, 1);
    m_refreshButton = new QPushButton(QStringLiteral("刷新寄存器"), this);
    root->addWidget(m_refreshButton);

    m_valueTimer = new QTimer(this);
    m_valueTimer->setInterval(1000);
    connect(m_valueTimer, &QTimer::timeout, this, [this]() {
        ++m_tick;
        const quint16 temperature = static_cast<quint16>(253 + (m_tick % 8));
        const quint16 current = static_cast<quint16>(125 + (m_tick % 5));
        const quint16 rpm = static_cast<quint16>(1200 + (m_tick % 60) * 5);
        m_modbus->setRegister(QModbusDataUnit::InputRegisters, 0, temperature);
        m_modbus->setRegister(QModbusDataUnit::InputRegisters, 1, current);
        m_modbus->setRegister(QModbusDataUnit::InputRegisters, 2, rpm);
        refreshRegisters();
    });
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!m_modbus->start(m_addressEdit->text(),
                             static_cast<quint16>(m_portSpin->value()),
                             m_unitSpin->value(), &error)) {
            QMessageBox::warning(this, QStringLiteral("启动失败"), error);
            return;
        }
        setStatus(QStringLiteral("运行中：%1:%2，Unit ID=%3")
                      .arg(m_addressEdit->text()).arg(m_portSpin->value())
                      .arg(m_unitSpin->value()), true);
        m_valueTimer->start();
        refreshRegisters();
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        m_valueTimer->stop();
        m_modbus->stop();
        setStatus(QStringLiteral("状态：已停止"), false);
    });
    connect(m_refreshButton, &QPushButton::clicked,
            this, &SimulatorPage::refreshRegisters);
    connect(m_modbus, &ModbusTcpSimulator::errorOccurred,
            this, [this](const QString &message) {
        setStatus(QStringLiteral("错误：%1").arg(message), false);
    });
    connect(m_modbus, &ModbusTcpSimulator::registerWritten,
            this, [this](QModbusDataUnit::RegisterType, int, int) {
        refreshRegisters();
    });
    setStatus(QStringLiteral("状态：未启动"), false);
    refreshRegisters();
}

SimulatorPage::~SimulatorPage()
{
    if (m_valueTimer)
        m_valueTimer->stop();
    if (m_modbus)
        m_modbus->stop();
}

void SimulatorPage::refreshRegisters()
{
    struct Row { QModbusDataUnit::RegisterType type; quint16 address; const char *name; };
    const Row rows[] = {
        {QModbusDataUnit::InputRegisters, 0, "温度（0.1 °C）"},
        {QModbusDataUnit::InputRegisters, 1, "电流（0.01 A）"},
        {QModbusDataUnit::InputRegisters, 2, "转速（rpm）"},
        {QModbusDataUnit::InputRegisters, 3, "状态字"},
        {QModbusDataUnit::HoldingRegisters, 0, "可写示例寄存器"},
        {QModbusDataUnit::HoldingRegisters, 1, "保留"},
        {QModbusDataUnit::Coils, 0, "线圈"},
        {QModbusDataUnit::DiscreteInputs, 0, "离散输入"}
    };
    for (int row = 0; row < 8; ++row) {
        quint16 value = 0;
        const bool available = m_modbus->registerValue(rows[row].type, rows[row].address, &value);
        const QString type = rows[row].type == QModbusDataUnit::InputRegisters
            ? QStringLiteral("输入寄存器")
            : rows[row].type == QModbusDataUnit::HoldingRegisters
                ? QStringLiteral("保持寄存器")
                : rows[row].type == QModbusDataUnit::Coils
                    ? QStringLiteral("线圈") : QStringLiteral("离散输入");
        const QStringList values = {
            type, QString::number(rows[row].address),
            available ? QString::number(value) : QStringLiteral("—"),
            QString::fromLatin1(rows[row].name)
        };
        for (int col = 0; col < 4; ++col) {
            auto *item = m_registerTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_registerTable->setItem(row, col, item);
            }
            item->setText(values.at(col));
        }
    }
}

void SimulatorPage::setStatus(const QString &text, bool running)
{
    m_statusLabel->setText(text);
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
}
