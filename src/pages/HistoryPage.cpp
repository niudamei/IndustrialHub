#include "HistoryPage.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QStringList>
#include <QTime>

namespace {
QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}
}

HistoryPage::HistoryPage(DataManager *dataManager, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HistoryPage),
      m_dataManager(dataManager)
{
    ui->setupUi(this);

    const QDate today = QDate::currentDate();
    ui->startDateTimeEdit->setCalendarPopup(true);
    ui->endDateTimeEdit->setCalendarPopup(true);
    ui->startDateTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    ui->endDateTimeEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    ui->startDateTimeEdit->setDate(today.addDays(-1));
    ui->endDateTimeEdit->setDate(today);

    ui->directionCombo->addItems({
        QStringLiteral("全部方向"),
        QStringLiteral("RX"),
        QStringLiteral("TX")
    });

    ui->historyTable->setColumnCount(5);
    ui->historyTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("设备编号"),
        QStringLiteral("方向"),
        QStringLiteral("字节数"),
        QStringLiteral("原始数据（HEX）")
    });
    ui->historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->historyTable->verticalHeader()->setVisible(false);
    ui->historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->historyTable->setAlternatingRowColors(true);

    connect(ui->queryButton, &QPushButton::clicked, this, [this]() {
        m_offset = 0;
        queryHistory();
    });
    connect(ui->previousButton, &QPushButton::clicked,
            this, &HistoryPage::showPreviousPage);
    connect(ui->nextButton, &QPushButton::clicked,
            this, &HistoryPage::showNextPage);
    connect(ui->exportButton, &QPushButton::clicked,
            this, &HistoryPage::exportCurrentPage);

    if (!m_dataManager || !m_dataManager->isReady()) {
        ui->resultLabel->setText(m_dataManager
            ? QStringLiteral("历史数据库不可用：%1").arg(m_dataManager->lastError())
            : QStringLiteral("历史数据服务未初始化。"));
        ui->queryButton->setEnabled(false);
        ui->exportButton->setEnabled(false);
    } else {
        queryHistory();
    }
}

HistoryPage::~HistoryPage()
{
    delete ui;
}

void HistoryPage::queryHistory()
{
    if (!m_dataManager)
        return;

    const QDate startDate = ui->startDateTimeEdit->date();
    const QDate endDate = ui->endDateTimeEdit->date();
    if (startDate > endDate) {
        QMessageBox::warning(this, QStringLiteral("查询条件无效"),
                             QStringLiteral("开始日期不能晚于结束日期。"));
        return;
    }

    const QDateTime start(startDate, QTime(0, 0, 0));
    const QDateTime end(endDate, QTime(23, 59, 59, 999));
    const QString direction = ui->directionCombo->currentText()
                                  == QStringLiteral("全部方向")
                              ? QString() : ui->directionCombo->currentText();

    QString error;
    if (!m_dataManager->queryCommunicationHistory(
            start, end, ui->deviceFilterEdit->text(), direction,
            m_offset, PageSize, m_currentRecords, m_totalCount, &error)) {
        QMessageBox::warning(this, QStringLiteral("查询失败"), error);
        return;
    }

    ui->historyTable->setRowCount(m_currentRecords.size());
    for (int row = 0; row < m_currentRecords.size(); ++row) {
        const CommunicationHistoryRecord &record = m_currentRecords.at(row);
        const QStringList values = {
            record.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            record.deviceId,
            record.direction,
            QString::number(record.payload.size()),
            QString::fromLatin1(record.payload.toHex(' ').toUpper())
        };

        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 3)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->historyTable->setItem(row, column, item);
        }
    }

    updatePageInfo();
}

void HistoryPage::showPreviousPage()
{
    if (m_offset <= 0)
        return;
    m_offset = qMax(0, m_offset - PageSize);
    queryHistory();
}

void HistoryPage::showNextPage()
{
    if (m_offset + PageSize >= m_totalCount)
        return;
    m_offset += PageSize;
    queryHistory();
}

void HistoryPage::updatePageInfo()
{
    const int currentPage = m_totalCount == 0 ? 0 : (m_offset / PageSize) + 1;
    const int totalPages = m_totalCount == 0
        ? 0 : (m_totalCount + PageSize - 1) / PageSize;

    ui->resultLabel->setText(
        QStringLiteral("共 %1 条记录 | 第 %2 / %3 页 | 每页 %4 条")
            .arg(m_totalCount).arg(currentPage).arg(totalPages).arg(PageSize));
    ui->previousButton->setEnabled(m_offset > 0);
    ui->nextButton->setEnabled(m_offset + PageSize < m_totalCount);
    ui->exportButton->setEnabled(!m_currentRecords.isEmpty());
}

void HistoryPage::exportCurrentPage()
{
    if (m_currentRecords.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有数据"),
                                 QStringLiteral("当前页面没有可导出的记录。"));
        return;
    }

    const QString fileName = QStringLiteral("communication_history_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出当前页历史数据"), fileName,
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }

    QByteArray output = QByteArray::fromHex("EFBBBF");
    QStringList header = {
        QStringLiteral("时间"), QStringLiteral("设备编号"),
        QStringLiteral("方向"), QStringLiteral("字节数"),
        QStringLiteral("原始数据 HEX")
    };
    output += header.join(QLatin1Char(',')).toUtf8();
    output += "\r\n";

    for (const CommunicationHistoryRecord &record : m_currentRecords) {
        const QStringList columns = {
            csvEscape(record.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))),
            csvEscape(record.deviceId),
            csvEscape(record.direction),
            csvEscape(QString::number(record.payload.size())),
            csvEscape(QString::fromLatin1(record.payload.toHex(' ').toUpper()))
        };
        output += columns.join(QLatin1Char(',')).toUtf8();
        output += "\r\n";
    }

    if (file.write(output) != output.size()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    file.close();

    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出当前页 %1 条通信记录。").arg(m_currentRecords.size()));
}
