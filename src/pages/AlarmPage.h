#ifndef ALARMPAGE_H
#define ALARMPAGE_H

#include <QWidget>
#include <QList>
#include "ui_alarmpage.h"
#include "src/core/alarmmanager.h"

class QComboBox;
class QLineEdit;
class QTableWidget;
class QLabel;

class AlarmPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmPage(AlarmManager *alarmManager, QWidget *parent = nullptr);
    ~AlarmPage() override;

private slots:
    void refreshEvents();
    void refreshRules();
    void acknowledgeSelected();
    void addThresholdRule();
    void editSelectedRule();
    void deleteSelectedRule();
    void toggleSelectedRule();

private:
    void buildManagementUi();
    void editRuleDialog(const QString &ruleId = QString());
    void updateStatistics(const QList<AlarmEvent> &events);
    QString selectedRuleId() const;

    Ui::AlarmPage *ui;
    AlarmManager *m_alarmManager = nullptr;
    QTableWidget *m_ruleTable = nullptr;
    QLineEdit *m_deviceFilter = nullptr;
    QComboBox *m_severityFilter = nullptr;
    QComboBox *m_stateFilter = nullptr;
    QLabel *m_activeSummary = nullptr;
    QLabel *m_todaySummary = nullptr;
    QLabel *m_durationSummary = nullptr;
};

#endif // ALARMPAGE_H
