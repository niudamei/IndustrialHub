#include "CustomFolder/customtoolbutton.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>

CustomToolButton::CustomToolButton(QWidget *parent)
    : QToolButton(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomToolButton::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect rc = rect();
    int radius = 8;
    QPainterPath path;
    path.addRoundedRect(rc.adjusted(1,1,-1,-1), radius, radius);

    QLinearGradient grad(rc.topLeft(), rc.bottomRight());
    if(!isEnabled())
    {
        grad.setColorAt(0, QColor(80,80,85));
        grad.setColorAt(1, QColor(50,50,55));
    }
    else if(isDown())
    {
        grad.setColorAt(0, QColor(12,55,100));
        grad.setColorAt(1, QColor(6,30,60));
    }
    else if(underMouse())
    {
        grad.setColorAt(0, QColor(20,130,220));
        grad.setColorAt(1, QColor(10,80,160));
    }
    else
    {
        grad.setColorAt(0, QColor(15,90,170));
        grad.setColorAt(1, QColor(8,50,100));
    }
    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawPath(path);

    QToolButton::paintEvent(event);
}
