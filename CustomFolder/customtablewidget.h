#ifndef CUSTOMTABLEWIDGET_H
#define CUSTOMTABLEWIDGET_H
#include <QTableWidget>

class CustomTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit CustomTableWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMTABLEWIDGET_H
