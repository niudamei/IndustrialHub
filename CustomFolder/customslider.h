#ifndef CUSTOMSLIDER_H
#define CUSTOMSLIDER_H
#include <QSlider>

class CustomSlider : public QSlider
{
    Q_OBJECT
public:
    explicit CustomSlider(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMSLIDER_H
