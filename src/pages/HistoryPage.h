#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QList>
#include <QWidget>

#include "src/core/datamanager.h"
#include "ui_historypage.h"

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

private:
    void updatePageInfo();

    Ui::HistoryPage *ui = nullptr;
    DataManager *m_dataManager = nullptr;
    QList<CommunicationHistoryRecord> m_currentRecords;
    int m_totalCount = 0;
    int m_offset = 0;
    static constexpr int PageSize = 200;
};

#endif // HISTORYPAGE_H
