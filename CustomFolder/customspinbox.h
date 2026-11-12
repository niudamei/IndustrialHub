#ifndef CUSTOMSPINBOX_H
#define CUSTOMSPINBOX_H
#include <QSpinBox>

class CustomSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit CustomSpinBox(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMSPINBOX_H
