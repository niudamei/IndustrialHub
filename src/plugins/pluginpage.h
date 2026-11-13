#ifndef PLUGINPAGE_H
#define PLUGINPAGE_H

#include <QWidget>

class PluginManager;
class QLineEdit;
class QTableWidget;

class PluginPage : public QWidget
{
    Q_OBJECT
public:
    explicit PluginPage(PluginManager *manager, QWidget *parent = nullptr);

private:
    void refresh();
    void loadSelectedFile();
    void unloadSelectedPlugin();

    PluginManager *m_manager = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // PLUGINPAGE_H
