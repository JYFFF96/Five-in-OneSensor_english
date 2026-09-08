#include "compasswidget.h"

CompassWidget::CompassWidget(QWidget *parent)
    : QWidget(parent),
      heading(0)
{
    setAttribute(Qt::WA_TranslucentBackground);  // 允许透明背景
    setStyleSheet("background:transparent;");   // 让 QWidget 透明
}

void CompassWidget::drawCompass(QPainter &painter)
{
    // 1. 旋转背景
    painter.save();
    painter.rotate(0);  // 旋转整个背景
    painter.setPen(QPen(Qt::white, 1));
    painter.setBrush(QBrush(Qt::transparent));
    painter.drawEllipse(-95, -95, 190, 190);  // 画外圈
    painter.drawEllipse(-74, -74, 148, 148);  // 画外圈

    // 画三条线
    for (int i = 0; i < 8; ++i) {
        painter.drawLine(0, -50, 0, 50);  // 绘制线段（中心点为 0,0，上下 50）
        painter.rotate(45);  // 旋转 90°
    }

    // 2. 绘制刻度
    painter.setPen(QPen(Qt::white, 1));
    painter.setFont(QFont("Arial", 4, QFont::Bold));
    for (int i = 0; i < 360; i += 20) {
        painter.save();
        painter.rotate(i);
        painter.drawLine(0, -80, 0, -75);
//            painter.drawText(-5, -80, QString(QString::number(i)));
        painter.drawText(QRect(-8, -92, 16, 16), Qt::AlignCenter, QString(QString::number(i)));

        painter.restore();
    }
    painter.setPen(QPen(Qt::white, 1));
    for (int i = 10; i < 360; i += 20) {
        painter.save();
        painter.rotate(i);
        painter.drawLine(0, -90, 0, -75);
        painter.restore();
    }
    for (int i = 0; i < 360; i += 2) {
        painter.save();
        painter.rotate(i);
        painter.drawLine(0, -77, 0, -75);
        painter.restore();
    }

    // 3. 绘制方位字母 (N, E, S, W)
    painter.setFont(QFont("Arial", 10, QFont::Normal));
    painter.setPen(QPen(Qt::red, 1));
    painter.drawText(-8, -53, "N");
    painter.setPen(QPen(Qt::green, 1));
    painter.drawText(53, 8, "E");
    painter.drawText(-8, 73, "S");
    painter.drawText(-70, 8, "W");

    painter.restore();  // 结束背景旋转

    // 4. 绘制指针（始终朝上）
//        painter.setPen(Qt::NoPen);
//        painter.setBrush(QBrush(Qt::red));
//        QPointF points[3] = {QPointF(0, -60), QPointF(-10, 0), QPointF(10, 0)};
//        painter.drawPolygon(points, 3);

    painter.save();
    painter.rotate(m_angle);  // 旋转到当前角度
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::red));
    QPointF points[3] = {QPointF(0, -80), QPointF(-10, 0), QPointF(10, 0)};
    painter.drawPolygon(points, 3);
    painter.setBrush(QBrush(Qt::gray));
    QPointF points1[3] = {QPointF(0, 80), QPointF(-10, 0), QPointF(10, 0)};
    painter.drawPolygon(points1, 3);
    painter.restore();

    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QBrush(Qt::black));
    painter.drawEllipse(-5, -5, 10, 10);  // 画外圈
}

void CompassWidget::updateHeading()
{
    heading = (heading+2) % 360;  // 让指南针背景不断旋转
    update();  // 触发重绘
}

void CompassWidget::updateAngle(double angle)
{
    m_angle = angle;  // 让指南针不断旋转
    update();  // 触发重绘
}
