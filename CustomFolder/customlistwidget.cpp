#include "CustomListWidget.h"

NavItemDelegate::NavItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void NavItemDelegate::paint(QPainter *painter,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QString text = index.data(Qt::DisplayRole).toString();
    const bool selected = option.state & QStyle::State_Selected;
    const bool hover    = option.state & QStyle::State_MouseOver;

    // 左右留空隙，让圆角不贴边，和你截图风格一致
    QRect bgRect = option.rect.adjusted(6, 4, -6, -4);

    if (selected)
    {
        // 选中：亮科技蓝，和截图右侧蓝色选中块一致
        painter->setBrush(QColor(37, 99, 235));
        painter->setPen(Qt::NoPen);
    }
    else if (hover)
    {
        // 悬浮：比列表背景浅一点的深蓝灰
        painter->setBrush(QColor(30, 41, 59));
        painter->setPen(Qt::NoPen);
    }
    else
    {
        // 未选中：透明，不画背景
        painter->setBrush(Qt::transparent);
        painter->setPen(Qt::NoPen);
    }

    painter->drawRoundedRect(bgRect, 6, 6);

    // 文字：未选中淡蓝灰，选中白色
    QFont f;
    f.setPointSize(13);
    painter->setFont(f);

    if (selected)
        painter->setPen(QColor(255, 255, 255));
    else
        painter->setPen(QColor(148, 163, 184));

    painter->drawText(bgRect, Qt::AlignCenter, text);
    painter->restore();
}

QSize NavItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    return QSize(option.rect.width(), 40); // 行高稍微大一点，更接近截图
}

// ============================================================

CustomListWidget::CustomListWidget(QWidget *parent)
    : QListWidget(parent)
{
    setItemDelegate(new NavItemDelegate(this));
    viewport()->setStyleSheet("background-color: #0F172A;");
    viewport()->setAutoFillBackground(false);
    QPalette p = palette();
    p.setColor(QPalette::Base, QColor(15, 23, 42));
    setPalette(p);

    setFrameShape(QListWidget::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSpacing(4);
}

