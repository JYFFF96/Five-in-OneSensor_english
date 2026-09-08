#include "polarplot.h"
#include <cmath>
#include <QToolTip>

PolarPlot::PolarPlot(QWidget *parent)
    : QWidget(parent)
{
}

void PolarPlot::setSatelliteData(QVector<SatelliteData> data) {
    satelliteData = data;
    countryCounts.clear();
    for (auto sat : satelliteData) {
        QString country = getCountry(sat.name);
        countryCounts[country]++;
    }
    update();
}

void PolarPlot::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawPolarGrid(painter);
    drawSatellites(painter);
    drawLegend(painter);
}

void PolarPlot::drawPolarGrid(QPainter &painter) {
    int width = this->width();
    int height = this->height();
    int radius = qMin(width, height) / 2 - 20; // 极坐标半径
    QPointF center(width / 2, height / 2); // 极坐标中心点

    painter.save();
    painter.translate(center);
    painter.setPen(Qt::white);

    // 绘制同心圆，并标注俯仰角刻度
    for (int i = 1; i <= 6; ++i) { // 6 层同心圆，每层 15°
        int r = radius * i / 6; // 每层的半径
        painter.drawEllipse(QPointF(0, 0), r, r);

        // 标注俯仰角刻度值
        int elevation = /*90 - */i * 15; // 计算俯仰角
        if (elevation >= 0) { // 只标注 0° 到 90°
            if (elevation > 0 && elevation < 90) { // 跳过 0° 和 90°
                QString label = QString::number(elevation) + "°";
                QFontMetrics metrics(painter.font());
                int textWidth = metrics.horizontalAdvance(label);
                int textHeight = metrics.height();

                // 在竖轴右侧标注刻度值
                painter.drawText(QPointF(-textWidth / 2 + 1, -r + textHeight / 2 -4), label);
            }
        }
    }

    // 绘制角度线和刻度值
    for (int angle = 0; angle < 360; angle += 30) {
        // 调整角度：0° 在上方，顺时针方向增加
        double adjustedAngle = 90 - angle; // 将 0° 放在上方，并顺时针增加
        if (adjustedAngle < 0) adjustedAngle += 360; // 确保角度在 0-360 范围内

        double rad = qDegreesToRadians(adjustedAngle); // 将角度转换为弧度
        double x = radius * std::cos(rad); // 角度线终点的 X 坐标
        double y = -radius * std::sin(rad); // 角度线终点的 Y 坐标

        // 绘制角度线
        painter.drawLine(QPointF(0, 0), QPointF(x, y));

        // 在角度线外侧绘制刻度值
        QString label = QString::number(angle) + "°"; // 刻度值文本
        QFontMetrics metrics(painter.font());
        int textWidth = metrics.horizontalAdvance(label); // 文本宽度
        int textHeight = metrics.height(); // 文本高度

        // 计算刻度值的位置
        double offset = 10; // 刻度值与角度线的偏移量
        double labelX = (radius + offset) * std::cos(rad); // 刻度值 X 坐标
        double labelY = -(radius + offset) * std::sin(rad); // 刻度值 Y 坐标

        // 调整文本位置，使其居中
        labelX -= textWidth / 2;
        labelY += textHeight / 4;

        painter.drawText(QPointF(labelX, labelY), label);
    }

    painter.restore();
}

void PolarPlot::drawSatellites(QPainter &painter) {
    int width = this->width();
    int height = this->height();
    int radius = qMin(width, height) / 2 - 20;
    QPointF center(width / 2, height / 2);

    painter.save();
    painter.translate(center);

    for (const auto &sat : satelliteData) {
        QPointF pos = polarToCartesian(sat.azimuth, sat.elevation);
        pos *= radius / 90.0; // 缩放俯仰角

        QString country = getCountry(sat.name);
        QColor color = getCountryColor(country);
        painter.setBrush(color);
        painter.setPen(Qt::white);
        painter.drawEllipse(pos, 5, 5);
    }

    painter.restore();
}

void PolarPlot::drawLegend(QPainter &painter) {
    int x = 10; // 图例起始 X 坐标
    int y = 30; // 图例起始 Y 坐标（从顶部开始）
    int rectSize = 15; // 图例颜色方块的大小

    painter.setPen(Qt::white);
    for (const QString &country : fixedCountries) {
        int count = countryCounts.value(country, 0); // 如果国家不存在，数量为 0
        QColor color = getCountryColor(country);

        // 绘制颜色方块
        painter.setBrush(color);
        painter.drawRect(x, y, rectSize, rectSize);

        // 绘制文本（国家和数量）
        painter.drawText(x + rectSize + 5, y + rectSize, QString("%1: %2").arg(country).arg(count));

        // 每行图例向下移动
        y += 20;
    }
}

QPointF PolarPlot::polarToCartesian(double azimuth, double elevation) const {
    // 调整角度：0° 在上方，顺时针方向增加
    double adjustedAzimuth = 90 - azimuth; // 将 0° 放在上方，并顺时针增加
    if (adjustedAzimuth < 0) adjustedAzimuth += 360; // 确保角度在 0-360 范围内

    double rad = qDegreesToRadians(adjustedAzimuth); // 将角度转换为弧度
    double x = elevation * std::cos(rad); // X 坐标
    double y = -elevation * std::sin(rad); // Y 坐标
    return QPointF(x, y);
}

QString PolarPlot::getCountry(const QString &name) const {
    if (name.contains("GPS", Qt::CaseInsensitive)) return "United States";
    if (name.contains("GLONASS", Qt::CaseInsensitive) || name.contains("COSMOS", Qt::CaseInsensitive)) return "Russia";
    if (name.contains("BEIDOU", Qt::CaseInsensitive)) return "China";
    if (name.contains("IRNSS", Qt::CaseInsensitive)) return "India";
    if (name.contains("QZS", Qt::CaseInsensitive)) return "Japan";
    if (name.contains("GALILEO", Qt::CaseInsensitive)) return "European Union"; // 新增欧洲分类
    return "Other";
}

QColor PolarPlot::getCountryColor(const QString &country) const {
    static QMap<QString, QColor> colorMap = {
        {"United States", Qt::blue},
        {"Russia", Qt::yellow},
        {"China", Qt::red},
        {"India", Qt::green},
        {"Japan", Qt::magenta},
        {"European Union", Qt::cyan},
        {"Other", Qt::gray}
    };
    return colorMap.value(country, Qt::gray); // 默认返回灰色
}

void PolarPlot::mousePressEvent(QMouseEvent *event) {
    int width = this->width();
    int height = this->height();
    int radius = qMin(width, height) / 2 - 20; // 极坐标半径
    QPointF center(width / 2, height / 2); // 极坐标中心点

    // 将鼠标点击位置转换为相对于中心点的坐标
    QPointF mousePos = event->pos() - center;

    // 计算鼠标点击位置的距离和角度
    double distance = std::hypot(mousePos.x(), mousePos.y()); // 距离中心点的距离
    double angle = qRadiansToDegrees(std::atan2(-mousePos.y(), mousePos.x())); // 角度
    if (angle < 0) angle += 360; // 确保角度在 0-360 范围内

    // 如果点击位置在极坐标范围内
    if (distance <= radius) {
        // 遍历卫星数据，查找点击位置附近的卫星
        for (const auto &sat : satelliteData) {
            // 将卫星的方位角和俯仰角转换为笛卡尔坐标
            QPointF satPos = polarToCartesian(sat.azimuth, sat.elevation);
            satPos *= radius / 90.0; // 缩放俯仰角

            // 计算卫星点与鼠标点击位置的距离
            double satDistance = std::hypot(mousePos.x() - satPos.x(), mousePos.y() - satPos.y());

            // 如果距离小于某个阈值（例如 10 像素），则认为点击了该卫星
            if (satDistance < 10) {
                // 显示卫星信息
                QString info = QString("Name: %1\nAzimuth: %2°\nPitch: %3°\nLongitude: %4\nLatitude: %5\nHeight: %6 km")
                                   .arg(sat.name)
                                   .arg(sat.azimuth)
                                   .arg(sat.elevation)
                                   .arg(sat.longitude)
                                   .arg(sat.latitude)
                                   .arg(sat.height);
                QToolTip::showText(event->globalPos(), info, this);
                break;
            }
        }
    }
}
