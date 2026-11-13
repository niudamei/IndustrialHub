#include "HistoryPage.h"

#include <QDate>
#include <QtConcurrent/QtConcurrentRun>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLineSeries>
#include <QValueAxis>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QStringList>
#include <QTime>
#include <QGridLayout>
#include <QHash>
#include <QPainter>
#include <limits>

namespace {
QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

struct ScopedHistoryDatabase
{
    QString connectionName = QStringLiteral("HistoryQuery_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database;
    ~ScopedHistoryDatabase()
    {
        if (database.isValid()) {
            database.close();
            database = QSqlDatabase();
            QSqlDatabase::removeDatabase(connectionName);
        }
    }
};

CommunicationPageResult loadCommunicationPage(
    const QString &databasePath, const QDateTime &start, const QDateTime &end,
    const QString &deviceId, const QString &direction, int offset, int limit,
    int cachedCount, bool refreshCount)
{
    CommunicationPageResult result;
    ScopedHistoryDatabase connection;
    connection.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection.connectionName);
    connection.database.setDatabaseName(databasePath);
    connection.database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!connection.database.open()) {
        result.error = QStringLiteral("打开历史查询连接失败：%1").arg(connection.database.lastError().text());
        return result;
    }

    QString where = QStringLiteral(" WHERE timestamp_ms >= :start AND timestamp_ms <= :end");
    const QString normalizedDevice = deviceId.trimmed();
    if (!normalizedDevice.isEmpty()) where += QStringLiteral(" AND device_id = :device");
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        where += QStringLiteral(" AND direction = :direction");

    if (refreshCount) {
        QSqlQuery countQuery(connection.database);
        countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM communication_history") + where);
        countQuery.bindValue(QStringLiteral(":start"), start.toMSecsSinceEpoch());
        countQuery.bindValue(QStringLiteral(":end"), end.toMSecsSinceEpoch());
        if (!normalizedDevice.isEmpty()) countQuery.bindValue(QStringLiteral(":device"), normalizedDevice);
        if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
            countQuery.bindValue(QStringLiteral(":direction"), direction);
        if (!countQuery.exec() || !countQuery.next()) {
            result.error = QStringLiteral("统计通信历史失败：%1").arg(countQuery.lastError().text());
            return result;
        }
        result.totalCount = countQuery.value(0).toInt();
    } else {
        result.totalCount = cachedCount;
    }

    QSqlQuery query(connection.database);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp_ms, device_id, direction, payload FROM communication_history")
        + where + QStringLiteral(" ORDER BY timestamp_ms DESC, id DESC LIMIT :limit OFFSET :offset"));
    query.bindValue(QStringLiteral(":start"), start.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":end"), end.toMSecsSinceEpoch());
    if (!normalizedDevice.isEmpty()) query.bindValue(QStringLiteral(":device"), normalizedDevice);
    if (direction == QStringLiteral("TX") || direction == QStringLiteral("RX"))
        query.bindValue(QStringLiteral(":direction"), direction);
    query.bindValue(QStringLiteral(":limit"), qMax(1, limit));
    query.bindValue(QStringLiteral(":offset"), qMax(0, offset));
    if (!query.exec()) {
        result.error = QStringLiteral("查询通信历史失败：%1").arg(query.lastError().text());
        return result;
    }
    while (query.next()) {
        CommunicationHistoryRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong());
        record.deviceId = query.value(2).toString();
        record.direction = query.value(3).toString();
        record.payload = query.value(4).toByteArray();
        result.records.append(record);
    }
    result.ok = true;
    return result;
}

BusinessPageResult loadBusinessPage(
    const QString &databasePath, const QDateTime &start, const QDateTime &end,
    const QString &deviceId, const QString &variableId, int offset, int limit,
    int cachedCount, bool refreshCount)
{
    BusinessPageResult result;
    ScopedHistoryDatabase connection;
    connection.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection.connectionName);
    connection.database.setDatabaseName(databasePath);
    connection.database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!connection.database.open()) {
        result.error = QStringLiteral("打开业务历史查询连接失败：%1").arg(connection.database.lastError().text());
        return result;
    }

    QString where = QStringLiteral(" WHERE timestamp_ms >= ? AND timestamp_ms <= ?");
    if (!deviceId.trimmed().isEmpty()) where += QStringLiteral(" AND device_id = ?");
    if (!variableId.trimmed().isEmpty()) where += QStringLiteral(" AND variable_id = ?");

    if (refreshCount) {
        QSqlQuery countQuery(connection.database);
        countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM business_data_history") + where);
        int bind = 0;
        countQuery.bindValue(bind++, start.toMSecsSinceEpoch());
        countQuery.bindValue(bind++, end.toMSecsSinceEpoch());
        if (!deviceId.trimmed().isEmpty()) countQuery.bindValue(bind++, deviceId.trimmed());
        if (!variableId.trimmed().isEmpty()) countQuery.bindValue(bind++, variableId.trimmed());
        if (!countQuery.exec() || !countQuery.next()) {
            result.error = QStringLiteral("统计业务变量历史失败：%1").arg(countQuery.lastError().text());
            return result;
        }
        result.totalCount = countQuery.value(0).toInt();
    } else {
        result.totalCount = cachedCount;
    }

    QSqlQuery query(connection.database);
    query.prepare(QStringLiteral(
        "SELECT id, timestamp_ms, device_id, variable_id, variable_name, value, unit, quality, source_protocol "
        "FROM business_data_history") + where
        + QStringLiteral(" ORDER BY timestamp_ms DESC, id DESC LIMIT ? OFFSET ?"));
    int bind = 0;
    query.bindValue(bind++, start.toMSecsSinceEpoch());
    query.bindValue(bind++, end.toMSecsSinceEpoch());
    if (!deviceId.trimmed().isEmpty()) query.bindValue(bind++, deviceId.trimmed());
    if (!variableId.trimmed().isEmpty()) query.bindValue(bind++, variableId.trimmed());
    query.bindValue(bind++, qMax(1, limit));
    query.bindValue(bind, qMax(0, offset));
    if (!query.exec()) {
        result.error = QStringLiteral("查询业务变量历史失败：%1").arg(query.lastError().text());
        return result;
    }
    while (query.next()) {
        BusinessDataHistoryRecord record;
        record.id = query.value(0).toLongLong();
        record.point.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(1).toLongLong());
        record.point.deviceId = query.value(2).toString();
        record.point.variableId = query.value(3).toString();
        record.point.name = query.value(4).toString();
        record.point.value = query.value(5).toDouble();
        record.point.unit = query.value(6).toString();
        record.point.quality = static_cast<DataQuality>(query.value(7).toInt());
        record.point.sourceProtocol = query.value(8).toString();
        result.records.append(record);
    }
    result.ok = true;
    return result;
}

}

HistoryPage::HistoryPage(DataManager *dataManager, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::HistoryPage),
      m_dataManager(dataManager)
{
    ui->setupUi(this);
    m_communicationWatcher = new QFutureWatcher<CommunicationPageResult>(this);
    connect(m_communicationWatcher, &QFutureWatcher<CommunicationPageResult>::finished,
            this, [this]() { applyCommunicationResult(m_communicationWatcher->result()); });
    m_businessWatcher = new QFutureWatcher<BusinessPageResult>(this);
    connect(m_businessWatcher, &QFutureWatcher<BusinessPageResult>::finished,
            this, [this]() { applyBusinessResult(m_businessWatcher->result()); });

    // 将原有通信帧查询区放入独立页签，新增业务变量历史页签。
    auto *tabs = new QTabWidget(this);
    auto *communicationTab = new QWidget(tabs);
    auto *communicationLayout = new QVBoxLayout(communicationTab);
    QLayout *rootLayout = ui->verticalLayout;
    while (QLayoutItem *item = rootLayout->takeAt(0)) {
        if (item->widget()) {
            communicationLayout->addWidget(item->widget());
            delete item;
        } else if (item->layout()) {
            // A layout is itself a QLayoutItem; ownership transfers to the new layout.
            communicationLayout->addLayout(item->layout());
        } else {
            delete item;
        }
    }
    tabs->addTab(communicationTab, QStringLiteral("通信原始帧"));
    auto *businessTab = new QWidget(tabs);
    auto *businessLayout = new QVBoxLayout(businessTab);

    auto *filterGroup = new QGroupBox(QStringLiteral("业务变量历史筛选"), businessTab);
    auto *filterLayout = new QGridLayout(filterGroup);
    m_businessStartEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1), filterGroup);
    m_businessEndEdit = new QDateTimeEdit(QDateTime::currentDateTime(), filterGroup);
    for (QDateTimeEdit *edit : {m_businessStartEdit, m_businessEndEdit}) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    m_businessDeviceEdit = new QLineEdit(filterGroup);
    m_businessDeviceEdit->setPlaceholderText(QStringLiteral("留空表示全部设备"));
    m_businessVariableCombo = new QComboBox(filterGroup);
    m_businessVariableCombo->addItem(QStringLiteral("全部变量"), QString());
    m_businessVariableCombo->addItem(QStringLiteral("温度"), QStringLiteral("temperature"));
    m_businessVariableCombo->addItem(QStringLiteral("转速"), QStringLiteral("rpm"));
    m_businessVariableCombo->addItem(QStringLiteral("电流"), QStringLiteral("current"));
    m_businessQueryButton = new QPushButton(QStringLiteral("查询"), filterGroup);
    m_businessExportButton = new QPushButton(QStringLiteral("导出当前页 CSV"), filterGroup);
    filterLayout->addWidget(new QLabel(QStringLiteral("开始时间："), filterGroup), 0, 0);
    filterLayout->addWidget(m_businessStartEdit, 0, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("结束时间："), filterGroup), 0, 2);
    filterLayout->addWidget(m_businessEndEdit, 0, 3);
    filterLayout->addWidget(new QLabel(QStringLiteral("设备编号："), filterGroup), 1, 0);
    filterLayout->addWidget(m_businessDeviceEdit, 1, 1);
    filterLayout->addWidget(new QLabel(QStringLiteral("业务变量："), filterGroup), 1, 2);
    filterLayout->addWidget(m_businessVariableCombo, 1, 3);
    filterLayout->addWidget(m_businessQueryButton, 0, 4);
    filterLayout->addWidget(m_businessExportButton, 1, 4);
    businessLayout->addWidget(filterGroup);

    m_businessChartView = new QChartView(businessTab);
    m_businessChartView->setMinimumHeight(220);
    m_businessChartView->setRenderHint(QPainter::Antialiasing);
    businessLayout->addWidget(m_businessChartView);

    m_businessTable = new QTableWidget(businessTab);
    m_businessTable->setColumnCount(7);
    m_businessTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"), QStringLiteral("设备编号"), QStringLiteral("变量"),
        QStringLiteral("数值"), QStringLiteral("单位"), QStringLiteral("质量"), QStringLiteral("来源协议")
    });
    m_businessTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_businessTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_businessTable->verticalHeader()->setVisible(false);
    m_businessTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_businessTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_businessTable->setAlternatingRowColors(true);
    businessLayout->addWidget(m_businessTable, 1);

    auto *businessPagination = new QHBoxLayout();
    m_businessPreviousButton = new QPushButton(QStringLiteral("上一页"), businessTab);
    m_businessNextButton = new QPushButton(QStringLiteral("下一页"), businessTab);
    m_businessResultLabel = new QLabel(QStringLiteral("等待查询"), businessTab);
    businessPagination->addWidget(m_businessPreviousButton);
    businessPagination->addWidget(m_businessNextButton);
    businessPagination->addWidget(m_businessResultLabel, 1);
    businessLayout->addLayout(businessPagination);
    tabs->addTab(businessTab, QStringLiteral("业务变量历史"));
    rootLayout->addWidget(tabs);

    connect(m_businessQueryButton, &QPushButton::clicked, this, [this]() {
        m_businessOffset = 0;
        m_businessCountValid = false;
        queryBusinessHistory();
    });
    connect(m_businessPreviousButton, &QPushButton::clicked,
            this, &HistoryPage::showPreviousBusinessPage);
    connect(m_businessNextButton, &QPushButton::clicked,
            this, &HistoryPage::showNextBusinessPage);
    connect(m_businessExportButton, &QPushButton::clicked,
            this, &HistoryPage::exportBusinessCurrentPage);

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
        m_communicationCountValid = false;
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
        m_businessQueryButton->setEnabled(false);
        m_businessExportButton->setEnabled(false);
    } else {
        queryHistory();
        queryBusinessHistory();
    }
}

HistoryPage::~HistoryPage()
{
    delete ui;
}

void HistoryPage::queryHistory()
{
    if (!m_dataManager || (m_communicationWatcher && m_communicationWatcher->isRunning()))
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
    const QString direction = ui->directionCombo->currentText() == QStringLiteral("全部方向")
        ? QString() : ui->directionCombo->currentText();
    const QString path = m_dataManager->databasePath();
    const QString device = ui->deviceFilterEdit->text();
    const int cachedCount = m_totalCount;
    const bool refreshCount = !m_communicationCountValid;

    ui->queryButton->setEnabled(false);
    ui->previousButton->setEnabled(false);
    ui->nextButton->setEnabled(false);
    ui->resultLabel->setText(QStringLiteral("正在查询历史数据…"));
    m_communicationWatcher->setFuture(QtConcurrent::run(
        [path, start, end, device, direction, offset = m_offset, cachedCount, refreshCount]() {
            return loadCommunicationPage(path, start, end, device, direction,
                                          offset, 200, cachedCount, refreshCount);
        }));
}

void HistoryPage::applyCommunicationResult(const CommunicationPageResult &result)
{
    ui->queryButton->setEnabled(true);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("查询失败"), result.error);
        updatePageInfo();
        return;
    }

    m_currentRecords = result.records;
    m_totalCount = result.totalCount;
    m_communicationCountValid = true;
    ui->historyTable->setUpdatesEnabled(false);
    ui->historyTable->setRowCount(m_currentRecords.size());
    for (int row = 0; row < m_currentRecords.size(); ++row) {
        const CommunicationHistoryRecord &record = m_currentRecords.at(row);
        const QStringList values = {
            record.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            record.deviceId, record.direction,
            QString::number(record.payload.size()),
            QString::fromLatin1(record.payload.toHex(' ').toUpper())
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            if (column == 3) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->historyTable->setItem(row, column, item);
        }
    }
    ui->historyTable->setUpdatesEnabled(true);
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

void HistoryPage::queryBusinessHistory()
{
    if (!m_dataManager || !m_businessStartEdit || !m_businessEndEdit
        || (m_businessWatcher && m_businessWatcher->isRunning()))
        return;

    const QDateTime start = m_businessStartEdit->dateTime();
    const QDateTime end = m_businessEndEdit->dateTime();
    if (start > end) {
        QMessageBox::warning(this, QStringLiteral("查询条件无效"),
                             QStringLiteral("开始时间不能晚于结束时间。"));
        return;
    }

    const QString path = m_dataManager->databasePath();
    const QString device = m_businessDeviceEdit->text();
    const QString variable = m_businessVariableCombo->currentData().toString();
    const int cachedCount = m_businessTotalCount;
    const bool refreshCount = !m_businessCountValid;
    m_businessQueryButton->setEnabled(false);
    m_businessPreviousButton->setEnabled(false);
    m_businessNextButton->setEnabled(false);
    m_businessResultLabel->setText(QStringLiteral("正在查询业务变量历史…"));
    m_businessWatcher->setFuture(QtConcurrent::run(
        [path, start, end, device, variable, offset = m_businessOffset, cachedCount, refreshCount]() {
            return loadBusinessPage(path, start, end, device, variable,
                                    offset, 200, cachedCount, refreshCount);
        }));
}

void HistoryPage::applyBusinessResult(const BusinessPageResult &result)
{
    m_businessQueryButton->setEnabled(true);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("业务历史查询失败"), result.error);
        updateBusinessPageInfo();
        return;
    }

    m_businessRecords = result.records;
    m_businessTotalCount = result.totalCount;
    m_businessCountValid = true;
    m_businessTable->setUpdatesEnabled(false);
    m_businessTable->setRowCount(m_businessRecords.size());
    for (int row = 0; row < m_businessRecords.size(); ++row) {
        const BusinessDataPoint &point = m_businessRecords.at(row).point;
        const QString quality = point.quality == DataQuality::Good
            ? QStringLiteral("Good")
            : QStringLiteral("非 Good (%1)").arg(static_cast<int>(point.quality));
        const QStringList values = {
            point.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
            point.deviceId, point.name, QString::number(point.value, 'g', 12),
            point.unit, quality, point.sourceProtocol
        };
        for (int column = 0; column < values.size(); ++column)
            m_businessTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
    m_businessTable->setUpdatesEnabled(true);
    updateBusinessPageInfo();
    renderBusinessChart();
}

void HistoryPage::updateBusinessPageInfo()
{
    const int currentPage = m_businessTotalCount == 0 ? 0 : (m_businessOffset / PageSize) + 1;
    const int totalPages = m_businessTotalCount == 0
        ? 0 : (m_businessTotalCount + PageSize - 1) / PageSize;
    m_businessResultLabel->setText(
        QStringLiteral("共 %1 条记录 | 第 %2 / %3 页 | 每页 %4 条")
            .arg(m_businessTotalCount).arg(currentPage).arg(totalPages).arg(PageSize));
    m_businessPreviousButton->setEnabled(m_businessOffset > 0);
    m_businessNextButton->setEnabled(m_businessOffset + PageSize < m_businessTotalCount);
    m_businessExportButton->setEnabled(!m_businessRecords.isEmpty());
}

void HistoryPage::showPreviousBusinessPage()
{
    if (m_businessOffset <= 0)
        return;
    m_businessOffset = qMax(0, m_businessOffset - PageSize);
    queryBusinessHistory();
}

void HistoryPage::showNextBusinessPage()
{
    if (m_businessOffset + PageSize >= m_businessTotalCount)
        return;
    m_businessOffset += PageSize;
    queryBusinessHistory();
}

void HistoryPage::renderBusinessChart()
{
    auto *chart = new QChart();
    const QString selectedVariable = m_businessVariableCombo->currentData().toString();
    if (selectedVariable.isEmpty()) {
        chart->setTitle(QStringLiteral("请选择单一变量查看历史曲线；全部变量模式仅用于表格浏览"));
        m_businessChartView->setChart(chart);
        return;
    }

    chart->setTitle(QStringLiteral("业务变量历史曲线：%1").arg(m_businessVariableCombo->currentText()));
    chart->legend()->setVisible(true);
    auto *axisX = new QDateTimeAxis(chart);
    axisX->setFormat(QStringLiteral("HH:mm:ss"));
    axisX->setTitleText(QStringLiteral("采集时间"));
    auto *axisY = new QValueAxis(chart);
    axisY->setTitleText(QStringLiteral("变量值"));
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    QHash<QString, QLineSeries *> seriesByDevice;
    QDateTime minTime;
    QDateTime maxTime;
    qreal minValue = std::numeric_limits<qreal>::max();
    qreal maxValue = std::numeric_limits<qreal>::lowest();
    QString unit;

    // 查询结果按时间倒序返回，反向遍历后曲线按时间正序绘制。
    for (auto it = m_businessRecords.crbegin(); it != m_businessRecords.crend(); ++it) {
        const BusinessDataPoint &point = it->point;
        if (point.variableId.compare(selectedVariable, Qt::CaseInsensitive) != 0)
            continue;
        QLineSeries *series = seriesByDevice.value(point.deviceId, nullptr);
        if (!series) {
            series = new QLineSeries(chart);
            series->setName(point.deviceId);
            chart->addSeries(series);
            series->attachAxis(axisX);
            series->attachAxis(axisY);
            seriesByDevice.insert(point.deviceId, series);
        }
        if (unit.isEmpty())
            unit = point.unit;
        series->append(static_cast<qreal>(point.timestamp.toMSecsSinceEpoch()), point.value);
        minTime = !minTime.isValid() || point.timestamp < minTime ? point.timestamp : minTime;
        maxTime = !maxTime.isValid() || point.timestamp > maxTime ? point.timestamp : maxTime;
        minValue = qMin(minValue, point.value);
        maxValue = qMax(maxValue, point.value);
    }

    axisY->setTitleText(unit.isEmpty()
        ? QStringLiteral("变量值") : QStringLiteral("变量值（%1）").arg(unit));
    if (minTime.isValid() && maxTime.isValid()) {
        if (minTime == maxTime) {
            minTime = minTime.addSecs(-1);
            maxTime = maxTime.addSecs(1);
        }
        axisX->setRange(minTime, maxTime);
        const qreal margin = qMax<qreal>(0.1, (maxValue - minValue) * 0.1);
        axisY->setRange(minValue - margin, maxValue + margin);
    } else {
        axisX->setRange(QDateTime::currentDateTime().addSecs(-60),
                        QDateTime::currentDateTime());
        axisY->setRange(0.0, 100.0);
    }

    m_businessChartView->setChart(chart);
}

void HistoryPage::exportBusinessCurrentPage()
{
    if (m_businessRecords.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("没有数据"),
                                 QStringLiteral("当前页面没有可导出的业务历史记录。"));
        return;
    }

    const QString fileName = QStringLiteral("business_data_history_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出当前页业务历史"), fileName,
        QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }

    QByteArray output = QByteArray::fromHex("EFBBBF");
    const QStringList header = {
        QStringLiteral("时间"), QStringLiteral("设备编号"), QStringLiteral("变量 ID"),
        QStringLiteral("变量名称"), QStringLiteral("数值"), QStringLiteral("单位"),
        QStringLiteral("质量"), QStringLiteral("来源协议")
    };
    output += header.join(QLatin1Char(',')).toUtf8() + "\r\n";
    for (const BusinessDataHistoryRecord &record : m_businessRecords) {
        const BusinessDataPoint &point = record.point;
        const QString quality = point.quality == DataQuality::Good
            ? QStringLiteral("Good") : QString::number(static_cast<int>(point.quality));
        const QStringList columns = {
            csvEscape(point.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))),
            csvEscape(point.deviceId), csvEscape(point.variableId), csvEscape(point.name),
            csvEscape(QString::number(point.value, 'g', 12)), csvEscape(point.unit),
            csvEscape(quality), csvEscape(point.sourceProtocol)
        };
        output += columns.join(QLatin1Char(',')).toUtf8() + "\r\n";
    }

    if (file.write(output) != output.size()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    file.close();
    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出当前页 %1 条业务变量历史记录。")
            .arg(m_businessRecords.size()));
}
