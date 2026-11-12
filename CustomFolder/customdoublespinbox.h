#ifndef CUSTOMDOUBLESPINBOX_H
#define CUSTOMDOUBLESPINBOX_H
#include <QDoubleSpinBox>

class CustomDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit CustomDoubleSpinBox(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMDOUBLESPINBOX_H
