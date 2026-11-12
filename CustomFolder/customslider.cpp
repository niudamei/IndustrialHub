#include "CustomFolder/customslider.h"
#include <QPainter>
#include <QPainterPath>

CustomSlider::CustomSlider(QWidget *parent)
    : QSlider(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void CustomSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect rc = rect();
    int trackH = 6;
    int knobR = 10;
    int trackY = rc.center().y() - trackH/2;

    // 轨道背景
    QRect trackRect(knobR, trackY, rc.width() - knobR*2, trackH);
    QPainterPath trackBg;
    trackBg.addRoundedRect(trackRect, trackH/2, trackH/2);
    painter.setBrush(QColor(35,35,40));
    painter.setPen(Qt::NoPen);
    painter.drawPath(trackBg);

    // 已滑过进度
    double ratio = (maximum() == minimum()) ? 0.0 : (value() - minimum()) / double(maximum() - minimum());
    int progressW = qRound(trackRect.width() * ratio);
    QRect progressRect(trackRect.x(), trackY, progressW, trackH);
    QPainterPath progressPath;
    progressPath.addRoundedRect(progressRect, trackH/2, trackH/2);
    painter.setBrush(QColor(20,130,220));
    painter.drawPath(progressPath);

    // 滑块
    int knobX = trackRect.x() + progressW;
    QRect knobRect(knobX - knobR, rc.center().y() - knobR, knobR*2, knobR*2);
    painter.setBrush(QColor(20,130,220));
    painter.setPen(QPen(Qt::white, 1));
    painter.drawEllipse(knobRect);
}
