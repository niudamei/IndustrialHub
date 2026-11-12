#include "CustomFolder/customspinbox.h"
#include <QPainter>
#include <QPainterPath>

CustomSpinBox::CustomSpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomSpinBox::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(1,1,-1,-1);
    int radius = 8;
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    painter.setBrush(QColor(22,26,32));
    QColor border = hasFocus() ? QColor(40,160,255) : QColor(40,70,110);
    painter.setPen(QPen(border,1));
    painter.drawPath(path);

    // 父类绘制数字、上下箭头
    QSpinBox::paintEvent(event);
}
