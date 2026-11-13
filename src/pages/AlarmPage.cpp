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
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), this);
    auto *ackButton = new QPushButton(QStringLiteral("确认选中报警"), this);
    actions->addWidget(refreshButton);
    actions->addWidget(ackButton);
    actions->addStretch(1);
    ui->verticalLayout->insertLayout(0, actions);
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
