#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QList>
#include <QWidget>

#include "src/core/datamanager.h"
#include "ui_historypage.h"

class QChartView;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QPushButton;
class QTableWidget;

class HistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPage(DataManager *dataManager, QWidget *parent = nullptr);
    ~HistoryPage() override;

private slots:
    void queryHistory();
    void showPreviousPage();
    void showNextPage();
    void exportCurrentPage();
    void queryBusinessHistory();
    void showPreviousBusinessPage();
    void showNextBusinessPage();
    void exportBusinessCurrentPage();

private:
    void updatePageInfo();
    void updateBusinessPageInfo();
    void renderBusinessChart();

    Ui::HistoryPage *ui = nullptr;
    DataManager *m_dataManager = nullptr;
    QList<CommunicationHistoryRecord> m_currentRecords;
    int m_totalCount = 0;
    int m_offset = 0;
    static constexpr int PageSize = 200;

    QDateTimeEdit *m_businessStartEdit = nullptr;
    QDateTimeEdit *m_businessEndEdit = nullptr;
    QLineEdit *m_businessDeviceEdit = nullptr;
    QComboBox *m_businessVariableCombo = nullptr;
    QPushButton *m_businessQueryButton = nullptr;
    QPushButton *m_businessExportButton = nullptr;
    QPushButton *m_businessPreviousButton = nullptr;
    QPushButton *m_businessNextButton = nullptr;
    QLabel *m_businessResultLabel = nullptr;
    QTableWidget *m_businessTable = nullptr;
    QChartView *m_businessChartView = nullptr;
    QList<BusinessDataHistoryRecord> m_businessRecords;
    int m_businessTotalCount = 0;
    int m_businessOffset = 0;
};

#endif // HISTORYPAGE_H
