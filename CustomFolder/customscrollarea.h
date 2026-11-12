#ifndef CUSTOMSCROLLAREA_H
#define CUSTOMSCROLLAREA_H
#include <QScrollArea>

class CustomScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    explicit CustomScrollArea(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMSCROLLAREA_H
