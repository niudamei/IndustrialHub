#include "CustomFolder/customtreeview.h"
#include <QPainter>
#include <QPainterPath>

CustomTreeView::CustomTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomTreeView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(1,1,-1,-1);
    int radius = 8;
    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    painter.setBrush(QColor(22,26,32));
    painter.setPen(QPen(QColor(40,70,110),1));
    painter.drawPath(path);

    QTreeView::paintEvent(event);
}
