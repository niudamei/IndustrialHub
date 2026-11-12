#include "CustomProgressBar.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPainterPath>

CustomProgressBar::CustomProgressBar(QWidget *parent)
    : QProgressBar(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomProgressBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect r = rect();
    int radius = r.height()/2;

    // 背景
    QPainterPath bgPath;
    bgPath.addRoundedRect(r, radius, radius);
    painter.setBrush(QColor(35,35,40));
    painter.setPen(Qt::NoPen);
    painter.drawPath(bgPath);

    // 进度区域
    int w = r.width() * value() / (maximum() - minimum());
    QRect progressRect(0,0,w, r.height());
    QPainterPath proPath;
    proPath.addRoundedRect(progressRect, radius, radius);

    QLinearGradient grad(progressRect.topLeft(), progressRect.bottomRight());
    grad.setColorAt(0, QColor(20,130,220));
    grad.setColorAt(1, QColor(8,60,120));
    painter.setBrush(grad);
    painter.drawPath(proPath);

    // 文字
    painter.setPen(Qt::white);
    QFont f = painter.font();
    f.setPointSize(11);
    painter.setFont(f);
    painter.drawText(r, Qt::AlignCenter, QString("%1%").arg(value()));
}
