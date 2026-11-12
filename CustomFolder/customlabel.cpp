#include "CustomFolder/customlabel.h"
#include <QPainter>

CustomLabel::CustomLabel(QWidget *parent)
    : QLabel(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}
void CustomLabel::setOnline(bool ok){
    m_online = ok;
}

void CustomLabel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = this->rect();
    QRect bgRect = r.adjusted(1,1,-1,-1);
    // 放大文字留白，上下左右多空一点
    QRect textRect = r.adjusted(8, 3, -8, -3);

    painter.setBrush(QColor(25,25,25));
    painter.setPen(QColor(80,80,80));
    painter.drawRoundedRect(bgRect, 4, 4);

    QFont f;
    f.setPointSize(12);
    painter.setFont(f);
    if(m_online)
    {
        painter.setPen(QColor(60, 220, 80));
    }
    else
    {
        painter.setPen(QColor(255, 80, 80));
    }
    painter.drawText(textRect, Qt::AlignCenter, this->text());
}
