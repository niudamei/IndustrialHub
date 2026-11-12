#ifndef CUSTOMTREEVIEW_H
#define CUSTOMTREEVIEW_H
#include <QTreeView>

class CustomTreeView : public QTreeView
{
    Q_OBJECT
public:
    explicit CustomTreeView(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMTREEVIEW_H
