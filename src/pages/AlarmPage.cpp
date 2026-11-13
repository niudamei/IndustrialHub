#include "AlarmPage.h"
#include "ui_alarmpage.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QStringList>
#include <QAbstractItemView>
#include <QVBoxLayout>
#include <QUuid>
#include <QTextStream>
#include <QStringConverter>
#include <QtMath>

namespace {
QString severityText(const QString &value)
{
    if (value == QStringLiteral("Critical")) return QStringLiteral("严重");
    if (value == QStringLiteral("Warning")) return QStringLiteral("警告");
    if (value == QStringLiteral("Info")) return QStringLiteral("提示");
    return value;
}

QString stateText(const QString &value)
{
    if (value == QStringLiteral("ACTIVE")) return QStringLiteral("活动");
    if (value == QStringLiteral("ACKNOWLEDGED")) return QStringLiteral("已确认");
    if (value == QStringLiteral("RECOVERED")) return QStringLiteral("已恢复");
    if (value == QStringLiteral("RECOVERED_ACK")) return QStringLiteral("恢复且已确认");
    return value;
}

QString csvField(QString value)
{
    value.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

QString durationText(qint64 milliseconds)
{
    const qint64 seconds = qMax<qint64>(0, milliseconds / 1000);
    return QStringLiteral("%1小时 %2分 %3秒")
        .arg(seconds / 3600).arg((seconds % 3600) / 60).arg(seconds % 60);
}
}

AlarmPage::AlarmPage(AlarmManager *alarmManager, QWidget *parent)
    : QWidget(parent), ui(new Ui::AlarmPage), m_alarmManager(alarmManager)
{
    ui->setupUi(this);
    buildManagementUi();
    connect(ui->alarmTable, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { acknowledgeSelected(); });
    if (m_alarmManager) {
        connect(m_alarmManager, &AlarmManager::alarmEventChanged, this,
                [this](const AlarmEvent &) { refreshEvents(); });
        connect(m_alarmManager, &AlarmManager::rulesChanged,
                this, &AlarmPage::refreshRules);
    }
    refreshRules();
    refreshEvents();
}

AlarmPage::~AlarmPage()
{
    delete ui;
}

void AlarmPage::setPermissions(bool manageRules, bool acknowledge)
{
    m_canManageRules = manageRules;
    m_canAcknowledge = acknowledge;
    for (QPushButton *button : findChildren<QPushButton *>()) {
        const QString text = button->text();
        if (text.contains(QStringLiteral("规则")) || text == QStringLiteral("启用/禁用"))
            button->setEnabled(m_canManageRules);
        else if (text.contains(QStringLiteral("确认")))
            button->setEnabled(m_canAcknowledge);
    }
}

void AlarmPage::buildManagementUi()
{
    ui->alarmTable->setColumnCount(10);
    ui->alarmTable->setHorizontalHeaderLabels({
        QStringLiteral("触发时间"), QStringLiteral("设备"), QStringLiteral("变量"),
        QStringLiteral("当前值"), QStringLiteral("等级"), QStringLiteral("状态"),
        QStringLiteral("报警说明"), QStringLiteral("确认人"), QStringLiteral("恢复时间"),
        QStringLiteral("持续时间")
    });
    ui->alarmTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->alarmTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    ui->alarmTable->verticalHeader()->setVisible(false);
    ui->alarmTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->alarmTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->alarmTable->setAlternatingRowColors(true);

    auto *toolbar = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("新增规则"), this);
    auto *editButton = new QPushButton(QStringLiteral("编辑规则"), this);
    auto *toggleButton = new QPushButton(QStringLiteral("启用/禁用"), this);
    auto *deleteButton = new QPushButton(QStringLiteral("删除规则"), this);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    auto *ackButton = new QPushButton(QStringLiteral("确认选中报警"), this);
    auto *exportButton = new QPushButton(QStringLiteral("导出筛选结果 CSV"), this);
    toolbar->addWidget(addButton);
    toolbar->addWidget(editButton);
    toolbar->addWidget(toggleButton);
    toolbar->addWidget(deleteButton);
    toolbar->addStretch(1);
    toolbar->addWidget(refreshButton);
    toolbar->addWidget(ackButton);
    toolbar->addWidget(exportButton);
    ui->verticalLayout->insertLayout(0, toolbar);

    auto *rulesGroup = new QGroupBox(QStringLiteral("阈值规则管理"), this);
    auto *rulesLayout = new QVBoxLayout(rulesGroup);
    m_ruleTable = new QTableWidget(rulesGroup);
    m_ruleTable->setColumnCount(8);
    m_ruleTable->setHorizontalHeaderLabels({
        QStringLiteral("规则名称"), QStringLiteral("设备"), QStringLiteral("变量 ID"),
        QStringLiteral("低限"), QStringLiteral("高限"), QStringLiteral("等级"),
        QStringLiteral("状态"), QStringLiteral("规则 ID")
    });
    m_ruleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ruleTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ruleTable->setColumnHidden(7, true);
    m_ruleTable->verticalHeader()->setVisible(false);
    m_ruleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ruleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ruleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ruleTable->setAlternatingRowColors(true);
    m_ruleTable->setMaximumHeight(210);
    rulesLayout->addWidget(m_ruleTable);
    ui->verticalLayout->insertWidget(1, rulesGroup);

    auto *stats = new QHBoxLayout;
    m_activeSummary = new QLabel(QStringLiteral("活动报警：0"), this);
    m_todaySummary = new QLabel(QStringLiteral("今日报警：0"), this);
    m_durationSummary = new QLabel(QStringLiteral("平均持续时间：--"), this);
    stats->addWidget(m_activeSummary);
    stats->addSpacing(24);
    stats->addWidget(m_todaySummary);
    stats->addSpacing(24);
    stats->addWidget(m_durationSummary);
    stats->addStretch(1);
    ui->verticalLayout->insertLayout(2, stats);

    auto *filters = new QHBoxLayout;
    m_deviceFilter = new QLineEdit(this);
    m_deviceFilter->setPlaceholderText(QStringLiteral("按设备 ID 筛选"));
    m_deviceFilter->setClearButtonEnabled(true);
    m_severityFilter = new QComboBox(this);
    m_severityFilter->addItem(QStringLiteral("全部等级"), QString());
    m_severityFilter->addItem(QStringLiteral("提示"), QStringLiteral("Info"));
    m_severityFilter->addItem(QStringLiteral("警告"), QStringLiteral("Warning"));
    m_severityFilter->addItem(QStringLiteral("严重"), QStringLiteral("Critical"));
    m_stateFilter = new QComboBox(this);
    m_stateFilter->addItem(QStringLiteral("全部状态"), QString());
    m_stateFilter->addItem(QStringLiteral("活动"), QStringLiteral("ACTIVE"));
    m_stateFilter->addItem(QStringLiteral("已确认"), QStringLiteral("ACKNOWLEDGED"));
    m_stateFilter->addItem(QStringLiteral("已恢复"), QStringLiteral("RECOVERED"));
    m_stateFilter->addItem(QStringLiteral("恢复且已确认"), QStringLiteral("RECOVERED_ACK"));
    filters->addWidget(new QLabel(QStringLiteral("事件筛选："), this));
    filters->addWidget(m_deviceFilter, 1);
    filters->addWidget(m_severityFilter);
    filters->addWidget(m_stateFilter);
    ui->verticalLayout->insertLayout(3, filters);

    connect(addButton, &QPushButton::clicked, this, &AlarmPage::addThresholdRule);
    connect(editButton, &QPushButton::clicked, this, &AlarmPage::editSelectedRule);
    connect(toggleButton, &QPushButton::clicked, this, &AlarmPage::toggleSelectedRule);
    connect(deleteButton, &QPushButton::clicked, this, &AlarmPage::deleteSelectedRule);
    connect(refreshButton, &QPushButton::clicked, this, [this] {
        refreshRules();
        refreshEvents();
    });
    connect(ackButton, &QPushButton::clicked, this, &AlarmPage::acknowledgeSelected);
    connect(exportButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出报警记录"), QStringLiteral("alarm_events.csv"),
            QStringLiteral("CSV 文件 (*.csv)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
            return;
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << QChar(0xFEFF)
               << "触发时间,设备,变量,当前值,等级,状态,报警说明,确认人,恢复时间,持续时间\n";
        for (int row = 0; row < ui->alarmTable->rowCount(); ++row) {
            QStringList fields;
            for (int col = 0; col < 10; ++col)
                fields << csvField(ui->alarmTable->item(row, col)
                                       ? ui->alarmTable->item(row, col)->text() : QString());
            stream << fields.join(',') << '\n';
        }
        QMessageBox::information(this, QStringLiteral("导出完成"),
                                 QStringLiteral("已导出 %1 条筛选后的报警记录。")
                                     .arg(ui->alarmTable->rowCount()));
    });
    connect(m_deviceFilter, &QLineEdit::textChanged, this, &AlarmPage::refreshEvents);
    connect(m_severityFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshEvents(); });
    connect(m_stateFilter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshEvents(); });
    connect(m_ruleTable, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { editSelectedRule(); });
}

void AlarmPage::refreshRules()
{
    if (!m_ruleTable || !m_alarmManager) return;
    m_ruleTable->setRowCount(0);
    for (const AlarmRule &rule : m_alarmManager->rules()) {
        const int row = m_ruleTable->rowCount();
        m_ruleTable->insertRow(row);
        const QStringList values {
            rule.name, rule.deviceId, rule.variableId,
            rule.lowEnabled ? QString::number(rule.lowLimit, 'g', 10) : QStringLiteral("--"),
            rule.highEnabled ? QString::number(rule.highLimit, 'g', 10) : QStringLiteral("--"),
            severityText(rule.severity), rule.enabled ? QStringLiteral("启用") : QStringLiteral("禁用"),
            rule.ruleId
        };
        for (int col = 0; col < values.size(); ++col)
            m_ruleTable->setItem(row, col, new QTableWidgetItem(values.at(col)));
    }
}

void AlarmPage::refreshEvents()
{
    if (!m_alarmManager) return;
    QString error;
    const QList<AlarmEvent> records = m_alarmManager->events(5000, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("报警查询失败"), error);
        return;
    }
    updateStatistics(records);
    ui->alarmTable->setRowCount(0);
    const QString device = m_deviceFilter->text().trimmed();
    const QString severity = m_severityFilter->currentData().toString();
    const QString state = m_stateFilter->currentData().toString();
    for (const AlarmEvent &event : records) {
        if (!device.isEmpty() && !event.deviceId.contains(device, Qt::CaseInsensitive)) continue;
        if (!severity.isEmpty() && event.severity != severity) continue;
        if (!state.isEmpty() && event.state != state) continue;
        const QDateTime end = event.recoveredAt.isValid()
            ? event.recoveredAt : QDateTime::currentDateTime();
        const QStringList values {
            event.triggeredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            event.deviceId,
            event.variableName.isEmpty() ? event.variableId : event.variableName,
            QStringLiteral("%1 %2").arg(QString::number(event.value, 'g', 10), event.unit),
            severityText(event.severity), stateText(event.state), event.message,
            event.acknowledgedBy,
            event.recoveredAt.isValid()
                ? event.recoveredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QStringLiteral("--"),
            durationText(event.triggeredAt.msecsTo(end))
        };
        const int row = ui->alarmTable->rowCount();
        ui->alarmTable->insertRow(row);
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            if (col == 0) item->setData(Qt::UserRole, event.id);
            ui->alarmTable->setItem(row, col, item);
        }
    }
}

void AlarmPage::updateStatistics(const QList<AlarmEvent> &records)
{
    int active = 0;
    int today = 0;
    qint64 durationSum = 0;
    int durationCount = 0;
    const QDate todayDate = QDate::currentDate();
    const QDateTime now = QDateTime::currentDateTime();
    for (const AlarmEvent &event : records) {
        if (event.state == QStringLiteral("ACTIVE")
            || event.state == QStringLiteral("ACKNOWLEDGED")) ++active;
        if (event.triggeredAt.date() == todayDate) ++today;
        if (event.triggeredAt.isValid()) {
            const QDateTime end = event.recoveredAt.isValid() ? event.recoveredAt : now;
            durationSum += event.triggeredAt.msecsTo(end);
            ++durationCount;
        }
    }
    m_activeSummary->setText(QStringLiteral("活动报警：%1").arg(active));
    m_todaySummary->setText(QStringLiteral("今日报警：%1").arg(today));
    m_durationSummary->setText(QStringLiteral("平均持续时间：%1")
        .arg(durationCount ? durationText(durationSum / durationCount) : QStringLiteral("--")));
}

QString AlarmPage::selectedRuleId() const
{
    const int row = m_ruleTable ? m_ruleTable->currentRow() : -1;
    return row >= 0 && m_ruleTable->item(row, 7)
        ? m_ruleTable->item(row, 7)->text() : QString();
}

void AlarmPage::addThresholdRule()
{
    if (!m_canManageRules) return;
    editRuleDialog();
}

void AlarmPage::editSelectedRule()
{
    if (!m_canManageRules) return;
    const QString id = selectedRuleId();
    if (id.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条规则。"));
        return;
    }
    editRuleDialog(id);
}

void AlarmPage::editRuleDialog(const QString &ruleId)
{
    if (!m_canManageRules || !m_alarmManager) return;
    AlarmRule rule;
    const bool editing = !ruleId.isEmpty();
    if (editing) {
        bool found = false;
        for (const AlarmRule &candidate : m_alarmManager->rules()) {
            if (candidate.ruleId == ruleId) { rule = candidate; found = true; break; }
        }
        if (!found) return;
    } else {
        rule.ruleId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        rule.severity = QStringLiteral("Warning");
    }

    QDialog dialog(this);
    dialog.setWindowTitle(editing ? QStringLiteral("编辑报警规则") : QStringLiteral("新增报警规则"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(rule.name, &dialog);
    auto *device = new QLineEdit(rule.deviceId, &dialog);
    auto *variable = new QLineEdit(rule.variableId, &dialog);
    auto *lowEnabled = new QCheckBox(QStringLiteral("启用低限"), &dialog);
    auto *low = new QDoubleSpinBox(&dialog);
    low->setRange(-1.0e12, 1.0e12); low->setDecimals(6); low->setValue(rule.lowLimit);
    lowEnabled->setChecked(rule.lowEnabled);
    auto *highEnabled = new QCheckBox(QStringLiteral("启用高限"), &dialog);
    auto *high = new QDoubleSpinBox(&dialog);
    high->setRange(-1.0e12, 1.0e12); high->setDecimals(6); high->setValue(rule.highLimit);
    highEnabled->setChecked(rule.highEnabled);
    auto *severity = new QComboBox(&dialog);
    severity->addItem(QStringLiteral("提示"), QStringLiteral("Info"));
    severity->addItem(QStringLiteral("警告"), QStringLiteral("Warning"));
    severity->addItem(QStringLiteral("严重"), QStringLiteral("Critical"));
    severity->setCurrentIndex(qMax(0, severity->findData(rule.severity)));
    auto *enabled = new QCheckBox(QStringLiteral("规则启用"), &dialog);
    enabled->setChecked(rule.enabled);
    form->addRow(QStringLiteral("规则名称："), name);
    form->addRow(QStringLiteral("设备 ID："), device);
    form->addRow(QStringLiteral("变量 ID："), variable);
    form->addRow(lowEnabled, low);
    form->addRow(highEnabled, high);
    form->addRow(QStringLiteral("报警等级："), severity);
    form->addRow(QString(), enabled);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    rule.name = name->text().trimmed();
    rule.deviceId = device->text().trimmed();
    rule.variableId = variable->text().trimmed();
    rule.lowEnabled = lowEnabled->isChecked();
    rule.lowLimit = low->value();
    rule.highEnabled = highEnabled->isChecked();
    rule.highLimit = high->value();
    rule.severity = severity->currentData().toString();
    rule.enabled = enabled->isChecked();
    QString error;
    if (!m_alarmManager->addRule(rule, &error)) {
        QMessageBox::warning(this, QStringLiteral("保存规则失败"), error);
        return;
    }
    refreshRules();
}

void AlarmPage::toggleSelectedRule()
{
    if (!m_canManageRules) return;
    const QString id = selectedRuleId();
    if (id.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条规则。"));
        return;
    }
    for (const AlarmRule &rule : m_alarmManager->rules()) {
        if (rule.ruleId != id) continue;
        QString error;
        if (!m_alarmManager->setRuleEnabled(id, !rule.enabled, &error))
            QMessageBox::warning(this, QStringLiteral("操作失败"), error);
        return;
    }
}

void AlarmPage::deleteSelectedRule()
{
    if (!m_canManageRules) return;
    const QString id = selectedRuleId();
    if (id.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一条规则。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("删除规则"),
            QStringLiteral("确定删除选中的报警规则吗？历史报警记录不会删除。"))
        != QMessageBox::Yes) return;
    QString error;
    if (!m_alarmManager->removeRule(id, &error))
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
    refreshRules();
}

void AlarmPage::acknowledgeSelected()
{
    if (!m_canAcknowledge) return;
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
