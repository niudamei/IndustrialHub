#include "CustomCheckBox.h"
#include <QPainter>
#include <QPainterPath>

CustomCheckBox::CustomCheckBox(QWidget *parent)
    : QCheckBox(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomCheckBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    int iconSize = 16;
    QRect iconRect(2, (height()-iconSize)/2, iconSize, iconSize);

    QPainterPath boxPath;
    boxPath.addRoundedRect(iconRect, 4,4);

    if(isChecked())
    {
        painter.setBrush(QColor(20,130,220));
        painter.setPen(Qt::NoPen);
        painter.drawPath(boxPath);
        // 对勾
        painter.setPen(QPen(Qt::white,2));
        painter.drawLine(iconRect.x()+3, iconRect.y()+8, iconRect.x()+6, iconRect.y()+11);
        painter.drawLine(iconRect.x()+6, iconRect.y()+11, iconRect.x()+12, iconRect.y()+4);
    }
    else
    {
        painter.setBrush(QColor(22,26,32));
        painter.setPen(QPen(QColor(40,70,110),1));
        painter.drawPath(boxPath);
    }

    // 文字绘制，调用原生
    QCheckBox::paintEvent(event);
}
