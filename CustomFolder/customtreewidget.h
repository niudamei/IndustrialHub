#ifndef CUSTOMTREEWIDGET_H
#define CUSTOMTREEWIDGET_H
#include <QTreeWidget>

class CustomTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit CustomTreeWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMTREEWIDGET_H
