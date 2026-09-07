#include "simulatepolarplot.h"
#include <cmath>
#include <QToolTip>

QStringList satelliteList = {"BeiDou","GPS","GLONASS","Galileo","QZS","Other"};

SimulatePolarPlot::SimulatePolarPlot(QWidget *parent)
    : QWidget(parent)
{
    // 初始化卫星类型颜色
    countryColors["GPS"] = Qt::blue;
    countryColors["GLONASS"] = Qt::yellow;
    countryColors["BeiDou"] = Qt::red;
    countryColors["Galileo"] = Qt::cyan;
    countryColors["QZS"] = Qt::magenta;
    countryColors["Other"] = Qt::gray;
}

void SimulatePolarPlot::setSatelliteData(const QVector<SatelliteGsvData> &data) {
    satelliteData = data;
    countryCounts.clear();

    // 统计每种卫星类型的数量
    for (const auto &sat : satelliteData) {
        QString type = getSatelliteType(sat.name); // 获取卫星类型
        countryCounts[type]++;
    }
    update();
}

QString SimulatePolarPlot::getSatelliteType(const QString &name) const {
    if (name.startsWith("GPS")) return "GPS";
    if (name.startsWith("GLONASS")) return "GLONASS";
    if (name.startsWith("BeiDou")) return "BeiDou";
    if (name.startsWith("Galileo")) return "Galileo";
    if (name.startsWith("QZS")) return "QZS";
    return "Other";
}

void SimulatePolarPlot::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawPolarGrid(painter);
    drawSatellites(painter);
    drawLegend(painter);
}

void SimulatePolarPlot::drawPolarGrid(QPainter &painter) {
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

void SimulatePolarPlot::drawSatellites(QPainter &painter) {
    int width = this->width();
    int height = this->height();
    int radius = qMin(width, height) / 2 - 20;
    QPointF center(width / 2, height / 2);

    painter.save();
    painter.translate(center);

    for (const auto &sat : satelliteData) {
        QPointF pos = polarToCartesian(sat.azimuth, sat.elevation);
        pos *= radius / 90.0; // 缩放俯仰角

        QString type = getSatelliteType(sat.name);
        QColor color = getCountryColor(type);
        painter.setBrush(color);
        painter.setPen(Qt::white);

        // 绘制更大的圆圈
        int circleRadius = 10; // 圆圈半径
        painter.drawEllipse(pos, circleRadius, circleRadius);

        // 在圆圈中显示卫星编号
        QString number = sat.name.split(" ").last(); // 提取编号
        painter.setPen(Qt::white);
        QRect textRect(pos.x()-10, pos.y()-10, 20, 20);
        painter.drawText(textRect, Qt::AlignCenter, number);
    }

    painter.restore();
}

void SimulatePolarPlot::drawLegend(QPainter &painter) {
    int x = 10; // 图例起始 X 坐标
    int y = 30; // 图例起始 Y 坐标（从顶部开始）
    int rectSize = 15; // 图例颜色方块的大小

    painter.setPen(Qt::white);
    for (int i = 0; i < satelliteList.count(); ++i) {
        QString type = satelliteList.at(i);
        int count;
        if (countryCounts.contains(type)) {
            count = countryCounts.value(type);
        } else {
            count = 0;
        }
        QColor color = getCountryColor(type); // 获取颜色

        // 绘制图例
        painter.setBrush(color);
        painter.drawRect(x, y, rectSize, rectSize);
        painter.drawText(x + rectSize + 5, y + rectSize, QString("%1: %2").arg(type).arg(count));
        y += 20;
    }
}

QPointF SimulatePolarPlot::polarToCartesian(double azimuth, double elevation) const {
    double adjustedAzimuth = 90 - azimuth; // 将 0° 放在上方，并顺时针增加
    if (adjustedAzimuth < 0) adjustedAzimuth += 360; // 确保角度在 0-360 范围内
    double rad = qDegreesToRadians(azimuth);
    double x = elevation * std::cos(rad);
    double y = -elevation * std::sin(rad);
    return QPointF(x, y);
}

QColor SimulatePolarPlot::getCountryColor(const QString &country) const {
    return countryColors.value(country, Qt::white);
}

void SimulatePolarPlot::mousePressEvent(QMouseEvent *event) {
    int width = this->width();
    int height = this->height();
    int radius = qMin(width, height) / 2 - 20;
    QPointF center(width / 2, height / 2);

    QPointF mousePos = event->pos() - center;
    double distance = std::hypot(mousePos.x(), mousePos.y());

    if (distance <= radius) {
        double azimuth = qRadiansToDegrees(std::atan2(-mousePos.y(), mousePos.x()));
        if (azimuth < 0) azimuth += 360;

        double elevation = distance * 90.0 / radius;

        for (const auto &sat : satelliteData) {
            if (std::abs(sat.azimuth - azimuth) < 5 && std::abs(sat.elevation - elevation) < 5) {
                QString info = QString("Name: %1\nAzimuth: %2°\nElevation: %3°\nSNR: %4")
                                   .arg(sat.name)
                                   .arg(sat.azimuth, 0, 'f', 1)
                                   .arg(sat.elevation, 0, 'f', 1)
                                   .arg(sat.snr >= 0 ? QString::number(sat.snr) : "N/A");
                QToolTip::showText(event->globalPos(), info, this);
                break;
            }
        }
    }
}
