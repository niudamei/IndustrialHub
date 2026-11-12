#include "CustomFolder/customcombobox.h"
#include <QPainter>
#include <QPainterPath>

CustomComboBox::CustomComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomComboBox::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(1,1,-1,-1);
    int radius = 8;

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    // 背景+边框
    painter.setBrush(QColor(22,26,32));
    QColor border = hasFocus() ? QColor(40,160,255) : QColor(40,70,110);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);

    // 绘制下拉三角
    int triW = 10;
    int triX = r.right() - 14;
    int triY = r.center().y();
    QPainterPath tri;
    tri.moveTo(triX, triY - 3);
    tri.lineTo(triX + triW, triY - 3);
    tri.lineTo(triX + triW/2, triY + 3);
    tri.closeSubpath();
    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);
    painter.drawPath(tri);

    // 调用父类绘制文字、下拉列表原生逻辑
    QComboBox::paintEvent(event);
}
