#include "settingspage.h"
#include "ui_settingspage.h"

#include <QApplication>
#include <QCheckBox>
#include <QRadioButton>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsPage)
{
    ui->setupUi(this);
    QSettings settings;
    const bool dark = settings.value(QStringLiteral("ui/darkTheme"), false).toBool();
    const bool minimizeToTray = settings.value(QStringLiteral("ui/minimizeToTray"), true).toBool();
    ui->radioDarkTheme->setChecked(dark);
    ui->radioLightTheme->setChecked(!dark);

    m_trayCheckBox = new QCheckBox(QStringLiteral("点击窗口关闭按钮时最小化到系统托盘"), this);
    m_trayCheckBox->setChecked(minimizeToTray);
    m_trayCheckBox->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    ui->verticalLayout->addWidget(m_trayCheckBox);

    connect(ui->radioDarkTheme, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        QSettings().setValue(QStringLiteral("ui/darkTheme"), true);
        emit themeChanged(true);
    });
    connect(ui->radioLightTheme, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) return;
        QSettings().setValue(QStringLiteral("ui/darkTheme"), false);
        emit themeChanged(false);
    });
    connect(m_trayCheckBox, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings().setValue(QStringLiteral("ui/minimizeToTray"), enabled);
        emit minimizeToTrayChanged(enabled);
    });
    emit themeChanged(dark);
    emit minimizeToTrayChanged(minimizeToTray);
}

SettingsPage::~SettingsPage()
{
    delete ui;
}
