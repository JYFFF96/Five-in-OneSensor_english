#include "postioningview.h"

PostioningView::PostioningView(QWidget *parent)
    : QWidget{parent}
{}

void PostioningView::addCoordinate(double latitude, double longitude)
{
    if (!hasCenterPoint) {
        centerLat = latitude;
        centerLon = longitude;
        hasCenterPoint = true;
    }
    coordinates.append(QPointF(latitude, longitude));
    update();
}

void PostioningView::clearData() {
    coordinates.clear();
    hasCenterPoint = false;
    update();
}

void PostioningView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int centerX = width() / 2;
    int centerY = height() / 2;
    int maxRadius = static_cast<int>(50 * 10); // 缩放后的最大半径

    // 画同心圆
    painter.setPen(QPen(Qt::white, 1));
    for (int i = 1; i <= 10; ++i) {
        int radius = static_cast<int>(i * 50);
        painter.drawEllipse(QPoint(centerX, centerY), radius, radius);
    }

    // 画虚线十字线
    QPen dashPen(Qt::white, 2, Qt::DashLine);
    painter.setPen(dashPen);
    painter.drawLine(centerX - maxRadius, centerY, centerX + maxRadius, centerY);
    painter.drawLine(centerX, centerY - maxRadius, centerX, centerY + maxRadius);

    // 画比例尺
    QFont font("Arial", 10);
    painter.setFont(font);

    for (int i = 1; i <= 10; ++i) {
        int x = centerX + i*50; // 右侧横线下方
        int y = centerY + 15; // 稍微偏下一点
        int num = i * scaleList.at(currentIndex);
        QString str =  QString::number(num) + "m";
        if (num >= 1000) {
            str = QString::number(num/1000) + "km";
        }
        painter.drawText(x, y, str);
    }

    // 画坐标点
    if (hasCenterPoint) {
        painter.setPen(QPen(Qt::red, 4));
        for (const QPointF &point : coordinates) {
            double deltaLat = (point.x() - centerLat) * 111000; // 纬度偏移量（米）
            double deltaLon = (point.y() - centerLon) * 111000 * cos(centerLat * M_PI / 180); // 经度偏移量（米）

            int x = centerX + static_cast<int>(deltaLon / scaleList.at(currentIndex)*50); // 10m = 1px
            int y = centerY - static_cast<int>(deltaLat / scaleList.at(currentIndex)*50);

            painter.drawEllipse(QPoint(x, y), 1, 1); // 绘制小圆点，半径 3 像素
        }
    }
}

void PostioningView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        if (currentIndex == 0) {
            return;
        }
        currentIndex--;
    } else {
        if (currentIndex == scaleList.size()-1) {
            return;
        }
        currentIndex++;
    }
    update(); // 重新绘制
}
