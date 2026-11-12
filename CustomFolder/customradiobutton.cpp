#include "CustomFolder/customradiobutton.h"
#include <QPainter>
#include <QPainterPath>

CustomRadioButton::CustomRadioButton(QWidget *parent)
    : QRadioButton(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomRadioButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    int iconSize = 16;
    QRect iconRect(2, (height() - iconSize)/2, iconSize, iconSize);

    if(isChecked())
    {
        painter.setBrush(QColor(20,130,220));
        painter.setPen(QPen(QColor(40,70,110),1));
        painter.drawEllipse(iconRect);
        // 内部圆点
        QRect inner = iconRect.adjusted(4,4,-4,-4);
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(inner);
    }
    else
    {
        painter.setBrush(QColor(22,26,32));
        painter.setPen(QPen(QColor(40,70,110),1));
        painter.drawEllipse(iconRect);
    }
    // 原生绘制文字
    QRadioButton::paintEvent(event);
}
