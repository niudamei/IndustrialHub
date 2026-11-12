#include "CustomFolder/customsplitter.h"
#include <QPainter>

CustomSplitter::CustomSplitter(QWidget *parent)
    : QSplitter(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomSplitter::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(40,70,110),1));
    QSplitter::paintEvent(event);
}
