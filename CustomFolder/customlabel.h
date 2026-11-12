#ifndef CUSTOMLABEL_H
#define CUSTOMLABEL_H
#include <QLabel>

class CustomLabel : public QLabel
{
    Q_OBJECT
public:
    explicit CustomLabel(QWidget *parent = nullptr);
    void setOnline(bool ok);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    bool m_online = false;
};
#endif // CUSTOMLABEL_H
