#include "AlarmPage.h"
#include "ui_alarmpage.h"
#include "src/core/alarmmanager.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QStringList>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QUuid>
#include <QtMath>
#include <QTableWidgetItem>

AlarmPage::AlarmPage(AlarmManager *alarmManager, QWidget *parent)
    : QWidget(parent), ui(new Ui::AlarmPage), m_alarmManager(alarmManager)
{
    ui->setupUi(this);
    ui->alarmTable->setColumnCount(9);
    ui->alarmTable->setHorizontalHeaderLabels({
        QStringLiteral("触发时间"), QStringLiteral("设备"), QStringLiteral("变量"),
        QStringLiteral("当前值"), QStringLiteral("等级"), QStringLiteral("状态"),
        QStringLiteral("报警说明"), QStringLiteral("确认人"), QStringLiteral("恢复时间")
    });
    ui->alarmTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->alarmTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    ui->alarmTable->verticalHeader()->setVisible(false);
    ui->alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->alarmTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->alarmTable->setAlternatingRowColors(true);

    auto *actions = new QHBoxLayout;
    auto *addRuleButton = new QPushButton(QStringLiteral("新增阈值规则"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    auto *ackButton = new QPushButton(QStringLiteral("确认选中报警"), this);
    actions->addWidget(addRuleButton);
    actions->addWidget(refreshButton);
    actions->addWidget(ackButton);
    actions->addStretch(1);
    ui->verticalLayout->insertLayout(0, actions);
    connect(addRuleButton, &QPushButton::clicked, this, &AlarmPage::addThresholdRule);
    connect(refreshButton, &QPushButton::clicked, this, &AlarmPage::refreshEvents);
    connect(ackButton, &QPushButton::clicked, this, &AlarmPage::acknowledgeSelected);
    if (m_alarmManager) {
        connect(m_alarmManager, &AlarmManager::alarmEventChanged, this,
                [this](const AlarmEvent &) { refreshEvents(); });
    }
    refreshEvents();
}

AlarmPage::~AlarmPage() { delete ui; }

void AlarmPage::refreshEvents()
{
    ui->alarmTable->setRowCount(0);
    if (!m_alarmManager) return;
    QString error;
    const QList<AlarmEvent> records = m_alarmManager->events(1000, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("报警查询失败"), error);
        return;
    }
    for (const AlarmEvent &event : records) {
        const int row = ui->alarmTable->rowCount();
        ui->alarmTable->insertRow(row);
        const QStringList values {
            event.triggeredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            event.deviceId,
            event.variableName.isEmpty() ? event.variableId : event.variableName,
            QStringLiteral("%1 %2").arg(QString::number(event.value, 'g', 10), event.unit),
            event.severity, event.state, event.message, event.acknowledgedBy,
            event.recoveredAt.isValid()
                ? event.recoveredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QStringLiteral("--")
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 0) item->setData(Qt::UserRole, event.id);
            ui->alarmTable->setItem(row, column, item);
        }
    }
}

void AlarmPage::addThresholdRule()
{
    if (!m_alarmManager) return;
    bool accepted = false;
    const QString deviceId = QInputDialog::getText(
        this, QStringLiteral("新增阈值规则"), QStringLiteral("设备 ID："),
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted) return;
    const QString variableId = QInputDialog::getText(
        this, QStringLiteral("新增阈值规则"), QStringLiteral("变量 ID（必须与解析器输出一致）："),
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted) return;
    const QString expression = QInputDialog::getText(
        this, QStringLiteral("设置阈值"), QStringLiteral("输入阈值，例如 low=10;high=80："),
        QLineEdit::Normal, QStringLiteral("high=80"), &accepted).trimmed();
    if (!accepted) return;

    AlarmRule rule;
    rule.ruleId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.deviceId = deviceId;
    rule.variableId = variableId;
    rule.name = variableId;
    const QStringList parts = expression.split(';', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QStringList pair = part.split('=');
        if (pair.size() != 2) {
            QMessageBox::warning(this, QStringLiteral("阈值格式错误"),
                                 QStringLiteral("请使用 low=数值;high=数值 格式。"));
            return;
        }
        bool ok = false;
        const double value = pair.at(1).trimmed().toDouble(&ok);
        if (!ok || !qIsFinite(value)) {
            QMessageBox::warning(this, QStringLiteral("阈值格式错误"),
                                 QStringLiteral("阈值必须是有效有限数值。"));
            return;
        }
        const QString key = pair.at(0).trimmed().toLower();
        if (key == QStringLiteral("low") && !rule.lowEnabled) {
            rule.lowEnabled = true;
            rule.lowLimit = value;
        } else if (key == QStringLiteral("high") && !rule.highEnabled) {
            rule.highEnabled = true;
            rule.highLimit = value;
        } else {
            QMessageBox::warning(this, QStringLiteral("阈值格式错误"),
                                 QStringLiteral("只允许各设置一次 low 和 high。"));
            return;
        }
    }
    const QStringList severities {
        QStringLiteral("Info"), QStringLiteral("Warning"), QStringLiteral("Critical")
    };
    rule.severity = QInputDialog::getItem(
        this, QStringLiteral("报警等级"), QStringLiteral("选择报警等级："),
        severities, 1, false, &accepted);
    if (!accepted) return;

    QString error;
    if (!m_alarmManager->addRule(rule, &error)) {
        QMessageBox::warning(this, QStringLiteral("添加规则失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("规则已添加"),
        QStringLiteral("规则已在本次运行中启用。请确保设备已绑定正确的业务解析器。"));
}

void AlarmPage::acknowledgeSelected()
{
    const int row = ui->alarmTable->currentRow();
    if (row < 0 || !ui->alarmTable->item(row, 0)) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条报警记录。"));
        return;
    }
    const qint64 eventId = ui->alarmTable->item(row, 0)->data(Qt::UserRole).toLongLong();
    bool accepted = false;
    const QString user = QInputDialog::getText(
        this, QStringLiteral("确认报警"), QStringLiteral("请输入确认人："),
        QLineEdit::Normal, QString(), &accepted).trimmed();
    if (!accepted) return;
    QString error;
    if (!m_alarmManager->acknowledge(eventId, user, &error)) {
        QMessageBox::warning(this, QStringLiteral("确认失败"), error);
        return;
    }
    refreshEvents();
}
