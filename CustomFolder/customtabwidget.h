#ifndef CUSTOMTABWIDGET_H
#define CUSTOMTABWIDGET_H
#include <QTabWidget>

class CustomTabWidget : public QTabWidget
{
    Q_OBJECT
public:
    explicit CustomTabWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMTABWIDGET_H
