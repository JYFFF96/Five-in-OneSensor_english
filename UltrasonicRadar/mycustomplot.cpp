#include "mycustomplot.h"


myCustomPlot::myCustomPlot(QWidget *parent) : QCustomPlot(parent)
{
    QPen axisPen(QColor(185,192,238)); // 红色
    axisPen.setWidth(2);
    this->xAxis->setBasePen(axisPen);
    this->yAxis->setBasePen(axisPen);

    double xRange = 6.93;
    double yRange = 3.85;
    this->xAxis->setRange(-xRange, xRange);
    this->yAxis->setRange(-yRange, yRange);

    // ✅ 启用网格
    QCPGrid *xGrid = this->xAxis->grid();
    QCPGrid *yGrid = this->yAxis->grid();
    xGrid->setSubGridVisible(true);
    yGrid->setSubGridVisible(true);
    xGrid->setPen(QPen(QColor(185,192,238), 1, Qt::DotLine));
    yGrid->setPen(QPen(QColor(185,192,238), 1, Qt::DotLine));

    // ✅ 允许缩放，禁用拖拽
    this->setInteraction(QCP::iRangeDrag, false);
    this->setInteraction(QCP::iRangeZoom, true);

    this->setBackground(QPixmap(":/images/background.png"));
    this->xAxis->setLabelColor(QColor(185,192,238));       // X 轴标题颜色（红色）
    this->xAxis->setTickLabelColor(QColor(185,192,238));   // X 轴刻度数字颜色（绿色）

    this->yAxis->setLabelColor(QColor(185,192,238));       // Y 轴标题颜色（蓝色）
    this->yAxis->setTickLabelColor(QColor(185,192,238)); // Y 轴刻度数字颜色（紫色）

    // 加载图片
    QPixmap pixmap(":/images/plotBackground.png");  // 替换为你的图片路径
    QCPItemPixmap *pixmapItem = new QCPItemPixmap(this);
    pixmapItem->setPixmap(pixmap); // 设置图片
    pixmapItem->setScaled(true, Qt::IgnoreAspectRatio);
    // pixmapItem->
    // pixmapItem->setTopLeft(customPlot->xAxis, customPlot->yAxis); // 或者使用其他方法设置位置
    pixmapItem->topLeft->setCoords(-3.5, 4.5); // 设置具体坐标
    pixmapItem->bottomRight->setCoords(3.5, -4.5); // 设置具体坐标

    initObstacles();


    setMouseTracking(true);  // 启用鼠标追踪
    connect(this, &QCustomPlot::mouseMove, this, &myCustomPlot::onMouseMove);

    // // 创建坐标显示文本
    // coordText = new QCPItemText(this);
    // coordText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    // coordText->setColor(Qt::red);
    // coordText->setFont(QFont("Arial", 10, QFont::Bold));
    // coordText->setPadding(QMargins(5, 2, 5, 2));
    // // coordText->setBackgroundColor(Qt::white); // 让背景更清晰
    // coordText->setText(""); // 初始为空


    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &myCustomPlot::arcFlashing);
    timer->setInterval(1000);
}

void myCustomPlot::onMouseMove(QMouseEvent *event)
{
    // 获取鼠标的像素坐标
    // double x = xAxis->pixelToCoord(event->pos().x());
    // double y = yAxis->pixelToCoord(event->pos().y());

    // // 设置文本位置为鼠标位置（+10 避免遮挡）
    // coordText->position->setType(QCPItemPosition::ptAbsolute);
    // coordText->position->setCoords(event->pos().x() + 10, event->pos().y() + 10);

    // // 更新文本内容
    // coordText->setText(QString("X: %1\nY: %2").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));

    // replot();  // 重新绘制
}

void myCustomPlot::wheelEvent(QWheelEvent *event)
{
    double factor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;

    // 以 (0,0) 作为缩放中心
    double xCenter = 0.0;
    double yCenter = 0.0;

    // 计算新的范围
    double xRangeHalf = xAxis->range().size() * factor * 0.5;
    double yRangeHalf = yAxis->range().size() * factor * 0.5;

    // 设定新的缩放范围
    xAxis->setRange(xCenter - xRangeHalf, xCenter + xRangeHalf);
    yAxis->setRange(yCenter - yRangeHalf, yCenter + yRangeHalf);

    emit sendRange(xAxis->range().upper, yAxis->range().upper);
    replot();  // 重新绘制
}

void myCustomPlot::resizeEvent(QResizeEvent *event)
{
    QCustomPlot::resizeEvent(event);  // 先调用基类的事件处理

    // 计算当前窗口的宽高比
    double aspectRatio = static_cast<double>(width()) / height();

    // 获取当前 Y 轴范围
    double yRangeHalf = yAxis->range().size() * 0.5;

    // 计算新的 X 轴范围，保持 1:1 显示比例
    double xRangeHalf = yRangeHalf * aspectRatio;

    // 设置新范围，保证比例不变
    xAxis->setRange(-xRangeHalf, xRangeHalf);
    yAxis->setRange(-yRangeHalf, yRangeHalf);

    xAxis->setScaleRatio(yAxis, 1.0);  // 强制 X/Y 比例一致

    replot();  // 重新绘制
}

void myCustomPlot::mousePressEvent(QMouseEvent *event)
{
    QCustomPlot::mousePressEvent(event); // 先处理 QCustomPlot 默认的事件
    if (parentWidget()) {
        QCoreApplication::sendEvent(parentWidget(), event); // 传递事件给父窗口
    }
}

void myCustomPlot::mouseMoveEvent(QMouseEvent *event)
{
    QCustomPlot::mouseMoveEvent(event); // 先处理 QCustomPlot 默认的事件
    if (parentWidget()) {
        QCoreApplication::sendEvent(parentWidget(), event); // 传递事件给父窗口
    }
}

void myCustomPlot::mouseReleaseEvent(QMouseEvent *event)
{
    QCustomPlot::mouseReleaseEvent(event); // 先处理 QCustomPlot 默认的事件
    if (parentWidget()) {
        QCoreApplication::sendEvent(parentWidget(), event); // 传递事件给父窗口
    }
}

void myCustomPlot::arcFlashing()
{
    if (arc) {
        arc->setVisible(!arc->visible());
        arc->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }
}

void myCustomPlot::UpdateObstaclesDistance(double distance1, double distance2, double distance3, double distance4)
{
    if (distance1 != this->distance1) {
        QPair<QVector<double>, QVector<double>> posPari1 = CalculatePostion(-0.07, 0.28, 130, distance1);
        LeftTop->setData(posPari1.first, posPari1.second);
        this->distance1 = distance1;
        // this->replot(); // 重新绘制
    }
    if (distance2 != this->distance2) {
        QPair<QVector<double>, QVector<double>> posPari2 = CalculatePostion(0.07, 0.28, 50, distance2);
        RightTop->setData(posPari2.first, posPari2.second);
        this->distance2 = distance2;
        // this->replot(); // 重新绘制
    }
    if (distance3 != this->distance3) {
        QPair<QVector<double>, QVector<double>> posPari3 = CalculatePostion(-0.07, -0.28, 230, distance3);
        LeftBottom->setData(posPari3.first, posPari3.second);
        this->distance3 = distance3;
        // this->replot(); // 重新绘制
    }
    if (distance4 != this->distance4) {
        QPair<QVector<double>, QVector<double>> posPari4 = CalculatePostion(0.07, -0.28, 310, distance4);
        RightBottom->setData(posPari4.first, posPari4.second);
        this->distance4 = distance4;
        // this->replot(); // 重新绘制
    }

    this->replot(QCustomPlot::rpQueuedReplot); // 重新绘制
}

void myCustomPlot::startBiaoding(int index, int distance)
{
    switch (index) {
    case 1: {
        drawRedArc(-0.07, 0.28, distance, 109, 150);
        break;
    }
    case 2: {
        drawRedArc(0.07, 0.28, distance, 30, 71);

        break;
    }
    case 3: {
        drawRedArc(-0.07, -0.28, distance, 210, 251);

        break;
    }
    case 4: {
        drawRedArc(0.07, -0.28, distance, 289, 330);

        break;
    }
    default:
        break;
    }
    timer->start();
}

void myCustomPlot::setArcColor(QColor color)
{
    if (arc) {
        if (arc->pen().color() != color) {
            arc->setPen(QPen(color, 2));  // 改成绿色
            replot();  // 重新绘制
        }
    }
}

// 删除圆弧
void myCustomPlot::deleteArc() {
    if (arc) {
        arc->data()->clear();
        this->replot();
    }
}

void myCustomPlot::deltePCurve()
{
    LeftTop->setData(QSharedPointer<QCPCurveDataContainer>(new QCPCurveDataContainer));
    distance1 = 0;
    RightTop->setData(QSharedPointer<QCPCurveDataContainer>(new QCPCurveDataContainer));
    distance2 = 0;
    LeftBottom->setData(QSharedPointer<QCPCurveDataContainer>(new QCPCurveDataContainer));
    distance3 = 0;
    RightBottom->setData(QSharedPointer<QCPCurveDataContainer>(new QCPCurveDataContainer));
    distance4 = 0;
    this->replot();
}

// void myCustomPlot::startTimer(bool flag)
// {
//     if (flag) {
//         if (!timer->isActive()) {
//             timer->start();
//         }
//     }
//     else {
//         if (timer->isActive()) {
//             timer->stop();
//             arc->setVisible(false);
//             arc->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
//         }
//     }
// }

void myCustomPlot::initObstacles()
{
    LeftTop = new QCPCurve(xAxis, yAxis);
    LeftTop->setPen(QPen(QColor(214,178,107), 2));
    LeftTop->setBrush(QBrush(QColor(214,178,107)));
    RightTop = new QCPCurve(xAxis, yAxis);
    RightTop->setPen(QPen(QColor(214,178,107), 2));
    RightTop->setBrush(QBrush(QColor(214,178,107)));
    LeftBottom = new QCPCurve(xAxis, yAxis);
    LeftBottom->setPen(QPen(QColor(214,178,107), 2));
    LeftBottom->setBrush(QBrush(QColor(214,178,107)));
    RightBottom = new QCPCurve(xAxis, yAxis);
    RightBottom->setPen(QPen(QColor(214,178,107), 2));
    RightBottom->setBrush(QBrush(QColor(214,178,107)));
    // RightBottom->setBrush(QBrush(Qt::cyan));
}

void myCustomPlot::drawRedArc(double centerX, double centerY, double radius, double startAngle, double endAngle)
{
    if (arc == nullptr) {
        arc = new QCPCurve(xAxis, yAxis);
    }

    // 准备数据
    // 角度转换为弧度
    double startRad = qDegreesToRadians(startAngle);
    double endRad = qDegreesToRadians(endAngle);

    const int numPoints = 100;
    QVector<double> x(numPoints), y(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        double theta = startRad + (endRad - startRad) * i / (numPoints - 1);  // 线性插值计算角度
        x[i] = centerX + radius * cos(theta);  // 计算 x 坐标
        y[i] = centerY + radius * sin(theta);  // 计算 y 坐标
    }

    // 设置数据
    arc->setData(x, y);
    xAxis->setScaleRatio(this->yAxis, 1.0);
    // 设置曲线的样式
    arc->setPen(QPen(Qt::red, 2));  // 线条颜色和粗细



    this->replot(); // 重新绘制
}

QPair<QVector<double>, QVector<double>> myCustomPlot::CalculatePostion(double ox, double oy, double angle, double distance)
{
    angle = qDegreesToRadians(angle); // 角度转换为弧度
    QPair<QVector<double>, QVector<double>> ret;
    QVector<double> xData, yData;
    double L = 0.1;  // 边长
    double h = sqrt(3) / 2 * L;  // 等边三角形高
    // 计算顶点 A
    double ax = ox + (distance + h) * cos(angle);
    double ay = oy + (distance + h) * sin(angle);

    // 计算底边中心 C
    double cx = ox + distance * cos(angle);
    double cy = oy + distance * sin(angle);

    // 计算 B1、B2
    double angle1 = angle + M_PI / 2;
    double angle2 = angle - M_PI / 2;
    double halfL = L / 2;

    double b1x = cx + halfL * cos(angle1);
    double b1y = cy + halfL * sin(angle1);

    double b2x = cx + halfL * cos(angle2);
    double b2y = cy + halfL * sin(angle2);

    xData << ax << b1x << b2x << ax;
    yData << ay << b1y << b2y << ay;
    ret.first = xData;
    ret.second = yData;
    return ret;
}
