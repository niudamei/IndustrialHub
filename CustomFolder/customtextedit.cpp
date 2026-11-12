#include "CustomFolder/customtextedit.h"
#include <QPainter>
#include <QPainterPath>

CustomTextEdit::CustomTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomTextEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(1,1,-1,-1);
    int radius = 8;
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    painter.setBrush(QColor(22,26,32));
    QColor border = hasFocus() ? QColor(40,160,255) : QColor(40,70,110);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);

    QTextEdit::paintEvent(event);
}
