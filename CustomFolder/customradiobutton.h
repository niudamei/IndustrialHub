#ifndef CUSTOMRADIOBUTTON_H
#define CUSTOMRADIOBUTTON_H
#include <QRadioButton>

class CustomRadioButton : public QRadioButton
{
    Q_OBJECT
public:
    explicit CustomRadioButton(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMRADIOBUTTON_H
