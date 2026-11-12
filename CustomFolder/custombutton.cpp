#include "CustomButton.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>

CustomButton::CustomButton(QWidget *parent)
    : QPushButton(parent)
{
    setFixedSize(80, 40);
    setStyleSheet("border:none; background:none;");
}

void CustomButton::paintEvent(QPaintEvent *event)
{
    QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect rect = this->rect();
    //QRect btnRect = rect.adjusted(4, 4, -4, -4);
    QRect btnRect = rect;
    int radius = 14;

    QPainterPath path;
    path.addRoundedRect(btnRect, radius, radius);

    // 发光效果：只有悬浮、可用状态才开启外发光
    if (isEnabled() && underMouse() && !isDown())
    {
        QColor glowColor(40, 160, 255, 120);
        painter.setPen(QPen(glowColor, 6));
        painter.drawPath(path);
    }

    QLinearGradient gradient(btnRect.topLeft(), btnRect.bottomRight());
    if (!isEnabled())
    {
        // 禁用：暗灰
        gradient.setColorAt(0, QColor(80, 80, 85));
        gradient.setColorAt(1, QColor(50, 50, 55));
    }
    else if (isDown())
    {
        // 按下：更深，下沉感
        gradient.setColorAt(0, QColor(12, 55, 100));
        gradient.setColorAt(1, QColor(6, 30, 60));
    }
    else if (underMouse())
    {
        // hover：亮蓝
        gradient.setColorAt(0, QColor(20, 130, 220));
        gradient.setColorAt(1, QColor(10, 80, 160));
    }
    else
    {
        // 常态：深色科技蓝
        gradient.setColorAt(0, QColor(15, 90, 170));
        gradient.setColorAt(1, QColor(8, 50, 100));
    }

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawPath(path);

    // 顶部高光细线，增加金属质感
    if (isEnabled())
    {
        QPen topLine(QColor(255,255,255,60), 1.2);
        painter.setPen(topLine);
        QRect highlightRect = btnRect.adjusted(2,2,-2, -(btnRect.height()/2));
        painter.drawRoundedRect(highlightRect, radius-2, radius-2);
    }

    // 文字
    painter.setPen(QColor(255,255,255));
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(btnRect, Qt::AlignCenter, text());
}
