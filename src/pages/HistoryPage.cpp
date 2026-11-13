#include "HistoryPage.h"

#include <QDate>
#include <QDateEdit>
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


HistoryExportResult exportHistoryCsv(
    const QString &databasePath, const QString &filePath, bool business,
    const QDateTime &start, const QDateTime &end,
    const QString &deviceId, const QString &filter)
{
    HistoryExportResult result;
    ScopedHistoryDatabase connection;
    connection.database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection.connectionName);
    connection.database.setDatabaseName(databasePath);
    connection.database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
    if (!connection.database.open()) {
        result.error = QStringLiteral("打开历史数据库失败：%1").arg(connection.database.lastError().text());
        return result;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.error = QStringLiteral("创建 CSV 文件失败：%1").arg(file.errorString());
        return result;
    }
    auto writeLine = [&file](const QStringList &columns) {
        const QByteArray line = columns.join(QLatin1Char(',')).toUtf8() + "\r\n";
        return file.write(line) == line.size();
    };
    if (file.write(QByteArray::fromHex("EFBBBF")) != 3) {
        result.error = QStringLiteral("写入 CSV 文件头失败：%1").arg(file.errorString());
        return result;
    }

    QSqlQuery query(connection.database);
    QString sql;
    if (!business) {
        sql = QStringLiteral("SELECT timestamp_ms, device_id, direction, payload FROM communication_history WHERE timestamp_ms >= ? AND timestamp_ms <= ?");
        if (!deviceId.trimmed().isEmpty()) sql += QStringLiteral(" AND device_id = ?");
        if (filter == QStringLiteral("TX") || filter == QStringLiteral("RX")) sql += QStringLiteral(" AND direction = ?");
    } else {
        sql = QStringLiteral("SELECT timestamp_ms, device_id, variable_id, variable_name, value, unit, quality, source_protocol FROM business_data_history WHERE timestamp_ms >= ? AND timestamp_ms <= ?");
        if (!deviceId.trimmed().isEmpty()) sql += QStringLiteral(" AND device_id = ?");
        if (!filter.trimmed().isEmpty()) sql += QStringLiteral(" AND variable_id = ?");
    }
    sql += QStringLiteral(" ORDER BY timestamp_ms ASC, id ASC");
    query.prepare(sql);
    int bind = 0;
    query.bindValue(bind++, start.toMSecsSinceEpoch());
    query.bindValue(bind++, end.toMSecsSinceEpoch());
    if (!deviceId.trimmed().isEmpty()) query.bindValue(bind++, deviceId.trimmed());
    if ((!business && (filter == QStringLiteral("TX") || filter == QStringLiteral("RX")))
        || (business && !filter.trimmed().isEmpty())) query.bindValue(bind++, filter);
    if (!query.exec()) {
        result.error = QStringLiteral("执行全量导出查询失败：%1").arg(query.lastError().text());
        return result;
    }

    if (!business) {
        if (!writeLine({QStringLiteral("时间"), QStringLiteral("设备编号"), QStringLiteral("方向"), QStringLiteral("字节数"), QStringLiteral("原始数据 HEX")})) {
            result.error = file.errorString(); return result;
        }
        while (query.next()) {
            const QByteArray payload = query.value(3).toByteArray();
            if (!writeLine({
                csvEscape(QDateTime::fromMSecsSinceEpoch(query.value(0).toLongLong()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))),
                csvEscape(query.value(1).toString()), csvEscape(query.value(2).toString()),
                csvEscape(QString::number(payload.size())),
                csvEscape(QString::fromLatin1(payload.toHex(' ').toUpper()))})) {
                result.error = QStringLiteral("写入 CSV 失败：%1").arg(file.errorString()); return result;
            }
            ++result.rows;
        }
    } else {
        if (!writeLine({QStringLiteral("时间"), QStringLiteral("设备编号"), QStringLiteral("变量 ID"), QStringLiteral("变量名称"), QStringLiteral("数值"), QStringLiteral("单位"), QStringLiteral("质量"), QStringLiteral("来源协议")})) {
            result.error = file.errorString(); return result;
        }
        while (query.next()) {
            const int q = query.value(6).toInt();
            const QString quality = q == static_cast<int>(DataQuality::Good) ? QStringLiteral("Good") : QString::number(q);
            if (!writeLine({
                csvEscape(QDateTime::fromMSecsSinceEpoch(query.value(0).toLongLong()).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))),
                csvEscape(query.value(1).toString()), csvEscape(query.value(2).toString()),
                csvEscape(query.value(3).toString()), csvEscape(QString::number(query.value(4).toDouble(), 'g', 12)),
                csvEscape(query.value(5).toString()), csvEscape(quality), csvEscape(query.value(7).toString())})) {
                result.error = QStringLiteral("写入 CSV 失败：%1").arg(file.errorString()); return result;
            }
            ++result.rows;
        }
    }
    file.flush();
    if (file.error() != QFileDevice::NoError) {
        result.error = QStringLiteral("完成 CSV 文件写入失败：%1").arg(file.errorString());
        return result;
    }
    file.close();
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
    m_exportWatcher = new QFutureWatcher<HistoryExportResult>(this);
    connect(m_exportWatcher, &QFutureWatcher<HistoryExportResult>::finished, this,
            [this]() { applyExportResult(m_exportWatcher->result()); });
    m_retentionPreviewWatcher = new QFutureWatcher<HistoryStoragePreview>(this);
    connect(m_retentionPreviewWatcher, &QFutureWatcher<HistoryStoragePreview>::finished,
            this, [this]() { applyHistoryPreview(m_retentionPreviewWatcher->result()); });
    m_retentionCleanupWatcher = new QFutureWatcher<HistoryCleanupResult>(this);
    connect(m_retentionCleanupWatcher, &QFutureWatcher<HistoryCleanupResult>::finished,
            this, [this]() { applyHistoryCleanup(m_retentionCleanupWatcher->result()); });
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
    m_businessExportAllButton = new QPushButton(QStringLiteral("导出全部筛选结果"), filterGroup);
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
    filterLayout->addWidget(m_businessExportAllButton, 2, 4);
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

    auto *retentionGroup = new QGroupBox(QStringLiteral("历史数据维护（仅手动清理）"), this);
    auto *retentionLayout = new QGridLayout(retentionGroup);
    m_retentionCutoffEdit = new QDateEdit(QDate::currentDate().addDays(-180), retentionGroup);
    m_retentionCutoffEdit->setCalendarPopup(true);
    m_retentionCutoffEdit->setMaximumDate(QDate::currentDate());
    m_retentionCutoffEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_retentionPreviewButton = new QPushButton(QStringLiteral("统计并预览"), retentionGroup);
    m_retentionCleanupButton = new QPushButton(QStringLiteral("确认清理过期数据"), retentionGroup);
    m_retentionCleanupButton->setEnabled(false);
    m_retentionStatusLabel = new QLabel(
        QStringLiteral("默认不自动删除任何历史记录。请先预览，再单独确认清理。"), retentionGroup);
    m_retentionStatusLabel->setWordWrap(true);
    retentionLayout->addWidget(new QLabel(QStringLiteral("清理截止日期："), retentionGroup), 0, 0);
    retentionLayout->addWidget(m_retentionCutoffEdit, 0, 1);
    retentionLayout->addWidget(m_retentionPreviewButton, 0, 2);
    retentionLayout->addWidget(m_retentionCleanupButton, 0, 3);
    retentionLayout->addWidget(m_retentionStatusLabel, 1, 0, 1, 4);
    rootLayout->addWidget(retentionGroup);

    connect(m_retentionPreviewButton, &QPushButton::clicked,
            this, &HistoryPage::previewHistoryCleanup);
    connect(m_retentionCleanupButton, &QPushButton::clicked,
            this, &HistoryPage::startHistoryCleanup);
    connect(m_retentionCutoffEdit, &QDateEdit::dateChanged, this, [this](const QDate &) {
        m_lastRetentionPreview = HistoryStoragePreview{};
        m_retentionCleanupButton->setEnabled(false);
        m_retentionStatusLabel->setText(QStringLiteral("截止日期已修改，请重新统计预览。"));
    });

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
    connect(m_businessExportAllButton, &QPushButton::clicked,
            this, &HistoryPage::exportBusinessAll);

    auto invalidateBusinessQuery = [this]() {
        m_businessOffset = 0;
        m_businessTotalCount = 0;
        m_businessCountValid = false;
        m_businessPreviousButton->setEnabled(false);
        m_businessNextButton->setEnabled(false);
        m_businessResultLabel->setText(QStringLiteral("查询条件已修改，请点击查询"));
    };
    connect(m_businessStartEdit, &QDateTimeEdit::dateTimeChanged, this,
            [invalidateBusinessQuery](const QDateTime &) { invalidateBusinessQuery(); });
    connect(m_businessEndEdit, &QDateTimeEdit::dateTimeChanged, this,
            [invalidateBusinessQuery](const QDateTime &) { invalidateBusinessQuery(); });
    connect(m_businessDeviceEdit, &QLineEdit::textChanged, this,
            [invalidateBusinessQuery](const QString &) { invalidateBusinessQuery(); });
    connect(m_businessVariableCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [invalidateBusinessQuery](int) { invalidateBusinessQuery(); });

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
    m_communicationExportAllButton = new QPushButton(QStringLiteral("导出全部筛选结果"), ui->dataGroup);
    if (auto *grid = qobject_cast<QGridLayout *>(ui->dataGroup->layout()))
        grid->addWidget(m_communicationExportAllButton, 2, 5);
    connect(m_communicationExportAllButton, &QPushButton::clicked,
            this, &HistoryPage::exportCommunicationAll);

    auto invalidateCommunicationQuery = [this]() {
        m_offset = 0;
        m_totalCount = 0;
        m_communicationCountValid = false;
        ui->previousButton->setEnabled(false);
        ui->nextButton->setEnabled(false);
        ui->resultLabel->setText(QStringLiteral("查询条件已修改，请点击查询"));
    };
    connect(ui->startDateTimeEdit, &QDateTimeEdit::dateChanged, this,
            [invalidateCommunicationQuery](const QDate &) { invalidateCommunicationQuery(); });
    connect(ui->endDateTimeEdit, &QDateTimeEdit::dateChanged, this,
            [invalidateCommunicationQuery](const QDate &) { invalidateCommunicationQuery(); });
    connect(ui->deviceFilterEdit, &QLineEdit::textChanged, this,
            [invalidateCommunicationQuery](const QString &) { invalidateCommunicationQuery(); });
    connect(ui->directionCombo, &QComboBox::currentTextChanged, this,
            [invalidateCommunicationQuery](const QString &) { invalidateCommunicationQuery(); });

    if (!m_dataManager || !m_dataManager->isReady()) {
        ui->resultLabel->setText(m_dataManager
            ? QStringLiteral("历史数据库不可用：%1").arg(m_dataManager->lastError())
            : QStringLiteral("历史数据服务未初始化。"));
        ui->queryButton->setEnabled(false);
        ui->exportButton->setEnabled(false);
        m_businessQueryButton->setEnabled(false);
        m_businessExportButton->setEnabled(false);
        m_businessExportAllButton->setEnabled(false);
        m_communicationExportAllButton->setEnabled(false);
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
    ui->startDateTimeEdit->setEnabled(false);
    ui->endDateTimeEdit->setEnabled(false);
    ui->deviceFilterEdit->setEnabled(false);
    ui->directionCombo->setEnabled(false);
    ui->exportButton->setEnabled(false);
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
    ui->startDateTimeEdit->setEnabled(true);
    ui->endDateTimeEdit->setEnabled(true);
    ui->deviceFilterEdit->setEnabled(true);
    ui->directionCombo->setEnabled(true);
    ui->exportButton->setEnabled(!m_currentRecords.isEmpty());
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


void HistoryPage::exportCommunicationAll()
{
    if (!m_dataManager || m_exportWatcher->isRunning()
        || m_retentionCleanupWatcher->isRunning()) return;
    const QDate sd = ui->startDateTimeEdit->date();
    const QDate ed = ui->endDateTimeEdit->date();
    if (sd > ed) {
        QMessageBox::warning(this, QStringLiteral("查询条件无效"), QStringLiteral("开始日期不能晚于结束日期。"));
        return;
    }
    const QString name = QStringLiteral("communication_history_all_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("导出全部通信历史"), name, QStringLiteral("CSV 文件 (*.csv)"));
    if (file.isEmpty()) return;
    const QDateTime start(sd, QTime(0, 0, 0));
    const QDateTime end(ed, QTime(23, 59, 59, 999));
    const QString direction = ui->directionCombo->currentText() == QStringLiteral("全部方向") ? QString() : ui->directionCombo->currentText();
    const QString db = m_dataManager->databasePath();
    const QString device = ui->deviceFilterEdit->text();
    ui->resultLabel->setText(QStringLiteral("正在后台导出全部通信历史…"));
    m_communicationExportAllButton->setEnabled(false);
    m_businessExportAllButton->setEnabled(false);
    m_exportWatcher->setFuture(QtConcurrent::run([db, file, start, end, device, direction]() {
        return exportHistoryCsv(db, file, false, start, end, device, direction);
    }));
}

void HistoryPage::exportBusinessAll()
{
    if (!m_dataManager || m_exportWatcher->isRunning()
        || m_retentionCleanupWatcher->isRunning()) return;
    const QDateTime start = m_businessStartEdit->dateTime();
    const QDateTime end = m_businessEndEdit->dateTime();
    if (start > end) {
        QMessageBox::warning(this, QStringLiteral("查询条件无效"), QStringLiteral("开始时间不能晚于结束时间。"));
        return;
    }
    const QString name = QStringLiteral("business_data_history_all_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString file = QFileDialog::getSaveFileName(this, QStringLiteral("导出全部业务历史"), name, QStringLiteral("CSV 文件 (*.csv)"));
    if (file.isEmpty()) return;
    const QString db = m_dataManager->databasePath();
    const QString device = m_businessDeviceEdit->text();
    const QString variable = m_businessVariableCombo->currentData().toString();
    m_businessResultLabel->setText(QStringLiteral("正在后台导出全部业务历史…"));
    m_businessExportAllButton->setEnabled(false);
    m_communicationExportAllButton->setEnabled(false);
    m_exportWatcher->setFuture(QtConcurrent::run([db, file, start, end, device, variable]() {
        return exportHistoryCsv(db, file, true, start, end, device, variable);
    }));
}

void HistoryPage::applyExportResult(const HistoryExportResult &result)
{
    m_communicationExportAllButton->setEnabled(true);
    m_businessExportAllButton->setEnabled(true);
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("全量导出失败"), result.error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出全部筛选结果，共 %1 条记录。").arg(result.rows));
}


void HistoryPage::previewHistoryCleanup()
{
    if (!m_dataManager || !m_dataManager->isReady()
        || m_retentionPreviewWatcher->isRunning()
        || m_retentionCleanupWatcher->isRunning()
        || (m_exportWatcher && m_exportWatcher->isRunning())
        || (m_communicationWatcher && m_communicationWatcher->isRunning())
        || (m_businessWatcher && m_businessWatcher->isRunning()))
        return;
    const QString databasePath = m_dataManager->databasePath();
    const QDateTime cutoff(m_retentionCutoffEdit->date(), QTime(0, 0, 0));
    m_retentionPreviewButton->setEnabled(false);
    m_retentionCleanupButton->setEnabled(false);
    m_retentionStatusLabel->setText(QStringLiteral("正在后台统计历史数据，请稍候…"));
    m_retentionPreviewWatcher->setFuture(QtConcurrent::run([databasePath, cutoff]() {
        return HistoryMaintenance::inspect(databasePath, cutoff);
    }));
}

void HistoryPage::applyHistoryPreview(const HistoryStoragePreview &preview)
{
    m_retentionPreviewButton->setEnabled(true);
    m_lastRetentionPreview = preview;
    if (!preview.ok) {
        m_retentionCleanupButton->setEnabled(false);
        m_retentionStatusLabel->setText(QStringLiteral("统计失败：%1").arg(preview.error));
        return;
    }
    const qint64 totalOld = preview.communicationRowsToDelete + preview.businessRowsToDelete;
    const double payloadMiB = static_cast<double>(preview.communicationPayloadBytesToDelete) / (1024.0 * 1024.0);
    const double dbMiB = static_cast<double>(preview.databaseFileBytes) / (1024.0 * 1024.0);
    m_retentionStatusLabel->setText(
        QStringLiteral("数据库文件约 %1 MiB；现有通信记录 %2 条、业务记录 %3 条。截止 %4 之前可清理：通信 %5 条（载荷约 %6 MiB）、业务 %7 条，共 %8 条。请核对后再点“确认清理过期数据”。")
            .arg(dbMiB, 0, 'f', 2)
            .arg(preview.communicationRows)
            .arg(preview.businessRows)
            .arg(preview.cutoff.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            .arg(preview.communicationRowsToDelete)
            .arg(payloadMiB, 0, 'f', 2)
            .arg(preview.businessRowsToDelete)
            .arg(totalOld));
    m_retentionCleanupButton->setEnabled(totalOld > 0
        && preview.cutoff.date() == m_retentionCutoffEdit->date());
}

void HistoryPage::startHistoryCleanup()
{
    if (!m_dataManager || m_retentionCleanupWatcher->isRunning()
        || m_exportWatcher->isRunning()
        || m_communicationWatcher->isRunning()
        || m_businessWatcher->isRunning()
        || !m_lastRetentionPreview.ok)
        return;
    const QDateTime cutoff(m_retentionCutoffEdit->date(), QTime(0, 0, 0));
    if (cutoff != m_lastRetentionPreview.cutoff) {
        m_retentionCleanupButton->setEnabled(false);
        QMessageBox::warning(this, QStringLiteral("需要重新预览"),
                             QStringLiteral("清理截止日期已变化，请先重新统计预览。"));
        return;
    }
    const qint64 comm = m_lastRetentionPreview.communicationRowsToDelete;
    const qint64 business = m_lastRetentionPreview.businessRowsToDelete;
    const QString prompt = QStringLiteral(
        "即将永久删除截止日期 %1 之前的历史数据：\n通信原始帧：%2 条\n业务变量：%3 条\n\n此操作不可撤销，建议先导出备份。确定继续吗？")
        .arg(cutoff.toString(QStringLiteral("yyyy-MM-dd")))
        .arg(comm).arg(business);
    if (QMessageBox::question(this, QStringLiteral("确认永久清理"), prompt,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes)
        return;

    const QString databasePath = m_dataManager->databasePath();
    m_retentionPreviewButton->setEnabled(false);
    m_retentionCleanupButton->setEnabled(false);
    m_retentionStatusLabel->setText(QStringLiteral("正在后台分批清理历史记录…"));
    m_retentionCleanupWatcher->setFuture(QtConcurrent::run([databasePath, cutoff]() {
        return HistoryMaintenance::cleanupOlderThan(databasePath, cutoff, 1000);
    }));
}

void HistoryPage::applyHistoryCleanup(const HistoryCleanupResult &result)
{
    if (!result.ok) {
        m_retentionStatusLabel->setText(
            QStringLiteral("清理未能完整完成：%1；本次已删除通信 %2 条、业务 %3 条。请重新预览确认剩余数据。")
                .arg(result.error).arg(result.communicationRowsDeleted)
                .arg(result.businessRowsDeleted));
        m_retentionPreviewButton->setEnabled(true);
        m_retentionCleanupButton->setEnabled(false);
        return;
    }
    m_retentionStatusLabel->setText(
        QStringLiteral("清理完成：通信原始帧 %1 条，业务变量 %2 条。正在重新统计剩余数据…")
            .arg(result.communicationRowsDeleted).arg(result.businessRowsDeleted));
    previewHistoryCleanup();
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
    m_businessStartEdit->setEnabled(false);
    m_businessEndEdit->setEnabled(false);
    m_businessDeviceEdit->setEnabled(false);
    m_businessVariableCombo->setEnabled(false);
    m_businessExportButton->setEnabled(false);
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
    m_businessStartEdit->setEnabled(true);
    m_businessEndEdit->setEnabled(true);
    m_businessDeviceEdit->setEnabled(true);
    m_businessVariableCombo->setEnabled(true);
    m_businessExportButton->setEnabled(!m_businessRecords.isEmpty());
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
