#ifndef COMPASSWIDGET_H
#define COMPASSWIDGET_H

#include <QWidget>
#include <QPainter>

class CompassWidget : public QWidget
{
    Q_OBJECT
public:
    CompassWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);  // 抗锯齿

        int side = qMin(width(), height());
        painter.translate(width() / 2, height() / 2);
        painter.scale(side / 200.0, side / 200.0);  // 归一化到 200x200

        drawCompass(painter);
    }

private:
    void drawCompass(QPainter &painter);

public slots:
    void updateHeading();
    void updateAngle(double angle = 0);
private:
    int heading;  // 指南针的方向（模拟传感器角度）
    double m_angle = 0;
};

#endif // COMPASSWIDGET_H
