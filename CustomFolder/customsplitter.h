#ifndef CUSTOMSPLITTER_H
#define CUSTOMSPLITTER_H
#include <QSplitter>

class CustomSplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit CustomSplitter(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
};
#endif // CUSTOMSPLITTER_H
