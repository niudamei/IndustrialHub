#include "CustomLineEdit.h"
#include <QPainter>
#include <QPainterPath>

CustomLineEdit::CustomLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomLineEdit::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect().adjusted(1,1,-1,-1);
    int radius = 8;

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);

    // 背景
    painter.setBrush(QColor(22,26,32));
    QColor borderColor = hasFocus() ? QColor(40,160,255) : QColor(40,70,110);
    painter.setPen(QPen(borderColor,1));
    painter.drawPath(path);

    // 调用原生绘制文字光标（重点！保留输入、光标、选中文字功能）
    QLineEdit::paintEvent(event);
}
