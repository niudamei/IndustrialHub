#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include "ui_settingspage.h"

class QRadioButton;
class QLabel;
class QComboBox;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

signals:
    void themeChanged(bool darkTheme);
    void minimizeToTrayChanged(bool enabled);

private:
    Ui::SettingsPage *ui;
    class QCheckBox *m_trayCheckBox = nullptr;
};

#endif // SETTINGSPAGE_H
