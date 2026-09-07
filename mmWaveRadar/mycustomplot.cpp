#include "mycustomplot.h"
#include <algorithm>  // 必须包含此头文件以使用 std::max
#include <QColor>     // 必须包含此头文件以使用 QColor
#include <QPen>       // 必须包含此头文件以使用 QPen
double sideLength = 0.5;
myCustomPlot::myCustomPlot(QWidget *parent) : QCustomPlot(parent),
    pulseTimer(new QTimer(this))
{
    QPen axisPen(QColor(185,192,238)); // 红色
    axisPen.setWidth(2);
    this->xAxis->setBasePen(axisPen);
    this->yAxis->setBasePen(axisPen);

    // 设置 X 轴和 Y 轴的范围
    xAxis->setRange(-50, 50);
    yAxis->setRange(0, 50);

    // 使 X 轴 (0,0) 作为中心
    xAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker));
    xAxis->ticker()->setTickOrigin(0);
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


    initPlot();
    drawLines(currentAngle);
    // setMouseTracking(true);  // 启用鼠标追踪
    // connect(this, &QCustomPlot::mouseMove, this, &myCustomPlot::onMouseMove);

    // // 创建坐标显示文本
    // coordText = new QCPItemText(this);
    // coordText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    // coordText->setColor(Qt::red);
    // coordText->setFont(QFont("Arial", 10, QFont::Bold));
    // coordText->setPadding(QMargins(5, 2, 5, 2));
    // // coordText->setBackgroundColor(Qt::white); // 让背景更清晰
    // coordText->setText(""); // 初始为空

    initObstaclePool();

    timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &myCustomPlot::arcFlashing);
    timer->setInterval(1000);

    connect(pulseTimer, &QTimer::timeout, this, &myCustomPlot::updatePulse);

    // 加载图片
    QPixmap pixmap(":/images/carhead.png");  // 替换为你的图片路径
    QCPItemPixmap *pixmapItem = new QCPItemPixmap(this);
    pixmapItem->setPixmap(pixmap); // 设置图片
    pixmapItem->setScaled(true, Qt::IgnoreAspectRatio);
    // pixmapItem->
    // pixmapItem->setTopLeft(customPlot->xAxis, customPlot->yAxis); // 或者使用其他方法设置位置
    pixmapItem->topLeft->setCoords(-0.8, 0.3); // 设置具体坐标
    pixmapItem->bottomRight->setCoords(0.8, 0); // 设置具体坐标

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
    double factor = (event->angleDelta().y() > 0) ? 0.9 : 1.1; // 滚轮上：缩小，滚轮下：放大
    double newXRange = (xAxis->range().size() * factor);
    double newYRange = (yAxis->range().size() * factor);

    // 限制最大范围不超过 999
    if (newYRange > 300 || newYRange < 0.1) {
        return;
    }
    if (newYRange >= 150) {
        pulseStep = 5;
        pulseDis = 30;
    } else if ((newYRange < 150) && (newYRange >= 75)) {
        pulseStep = 2.5;
        pulseDis = 20;
    } else if ((newYRange < 75) && (newYRange >= 25)){
        pulseStep = 1;
        pulseDis = 5;
    } else {
        pulseStep = 0.5;
        pulseDis = 2;
    }
    xAxis->scaleRange(factor, 0);  // 以 X 轴的 0 为中心缩放
    yAxis->scaleRange(factor, 0);  // 以 Y 轴的 0 为中心缩放

    emit sendRange(xAxis->range().upper, yAxis->range().upper);
    replot(QCustomPlot::rpQueuedReplot);
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

void myCustomPlot::updatePlot(double x, double y, int angle)
{
    xAxis->setRange(-x, x);
    yAxis->setRange(0, y);

    // drawLines(angle);
    if (angle != currentAngle) {
        currentAngle = angle;
        drawLines(currentAngle);
    }


    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::drawLines(double angle)
{
    for (QCPGraph* g : angleGraphs) {
        removeGraph(g);
    }
    angleGraphs.clear();

    // 角度转换为弧度
    double angles = angle / 2.0;
    double angle1 = angles * M_PI / 180.0;  // 转换为弧度
    double angle2 = -angles * M_PI / 180.0;

    // 终点坐标计算
    double length = 500; // 取 Y 轴范围作为射线长度
    double x1 = length * sin(angle1);
    double y1 = length * cos(angle1);
    double x2 = length * sin(angle2);
    double y2 = length * cos(angle2);

    // 画第一条射线
    QCPGraph *graph1 = addGraph();
    graph1->setPen(QPen(QColor(120,161,234), 2));
    graph1->addData(0, 0);
    graph1->addData(x1, y1);
    angleGraphs.append(graph1);

    // 画第二条射线
    QCPGraph *graph2 = addGraph();
    graph2->setPen(QPen(QColor(120,161,234), 2));
    graph2->addData(0, 0);
    graph2->addData(x2, y2);
    angleGraphs.append(graph2);
}

void myCustomPlot::drawObstaclesWithText(const QList<ObstacleInfo> &obsList)
{
    // 隐藏所有图元
    for (int i = 0; i < obstaclePool.size(); ++i) {
        obstaclePool[i].trianglePixmap->setVisible(false);
        obstaclePool[i].label->setVisible(false);
    }

    int maxDraw = qMin(obsList.size(), obstaclePool.size());

    for (int i = 0; i < maxDraw; ++i)
    {
        const ObstacleInfo &obs = obsList[i];

        // 根据 dynProp 和 targetState 过滤
        if (!isTargetVisible(obs.dynProp) || !isTypeVisible(obs.objClass)) continue;

        if (qAbs(obs.angle) > currentAngle/2) continue;


        ObstacleItem &item = obstaclePool[i];

        // 坐标转换：经纬度坐标 -> 像素坐标
        double x = xAxis->coordToPixel(obs.distLat);
        double y = yAxis->coordToPixel(obs.distLong);

        // 设置图元位置（左上角放置 pixmap）
        item.trianglePixmap->topLeft->setCoords(x - 10, y - 10); // 偏移居中
        item.trianglePixmap->setVisible(true);

        // 设置文字标签内容
        QString text = QString("ID:%1").arg(obs.id);

        if (labelOptions.showAngle)
            text += QString(" Angle:%1°").arg(obs.angle, 0, 'f', 2);
        if (labelOptions.showRange)
            text += QString(" Distance:%1m").arg(obs.distance, 0, 'f', 2);
        if (labelOptions.showSpeed)
            text += QString(" Speed:%1m/s").arg(obs.speed, 0, 'f', 2);
        if (labelOptions.showRcs)
            text += QString(" RCS:%1").arg(obs.rcs, 0, 'f', 2);
        if (labelOptions.showState)
            text += QString(" Target State:%1").arg(getStateString(obs.dynProp));
        if (labelOptions.showClass)
            text += QString(" Type:%1").arg(getClassString(obs.objClass));

        item.label->setText(text);
        item.label->position->setCoords(x, y - 15); // 顶部偏移
        item.label->setVisible(true);
    }

    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::clearObstacles()
{
    for (int i = 0; i < obstaclePool.size(); ++i) {
        obstaclePool[i].trianglePixmap->setVisible(false);
        obstaclePool[i].label->setVisible(false);
    }
    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::setLabelDisplayOptions(const LabelDisplayOptions &options)
{
    labelOptions = options;
}

void myCustomPlot::updateTargetState(const showTargetState &state)
{
    currentTargetState = state;
}

void myCustomPlot::updateTargetType(const showTargetType &state)
{
    currentTargetType = state;
}

void myCustomPlot::startBiaoding(double distance, double angle)
{
    if (biaodingLine) {
        removeItem(biaodingLine);
        biaodingLine = nullptr;
    }
    if (line1) {
        removeItem(line1);
        line1 = nullptr;
    }
    if (biaodingArc) {
        removeItem(biaodingArc);
        biaodingArc = nullptr;
    }
    if (line2) {
        removeItem(line2);
        line2 = nullptr;
    }

    if (distance <= 0)
        return;

    QPen redPen(Qt::red);
    redPen.setWidthF(2);

    if (angle == 0.0) {
        // --- 水平直线段 ---
        biaodingLine = new QCPItemLine(this);

        double halfLength = sideLength/2.0;
        double centerX = 0;
        double centerY = distance;

        biaodingLine->start->setCoords(centerX - halfLength, centerY);
        biaodingLine->end->setCoords(centerX + halfLength, centerY);
        biaodingLine->setPen(redPen);

        line1 = new QCPItemLine(this);
        line1->setPen(QPen(Qt::red, 2, Qt::DashLine));
        line1->start->setCoords(0, 0);
        line1->end->setCoords(0, 300);
    }
    else {
        // --- 圆弧绘制 ---
        const double centerX = 0;
        const double centerY = 0;

        // 修改角度计算方式：angle 是相对 Y 轴的角度
        double centerDeg = 90.0 - angle;  // 将角度计算修正为相对于 Y 轴
        double arcLength = sideLength;  // 圆弧长度为 0.5

        // 计算圆弧所需的角度，弧度 = 圆弧长度 / 半径
        double arcAngle = arcLength / distance;  // 得到弧度值

        // 将弧度转换为角度
        double startDeg = centerDeg - qRadiansToDegrees(arcAngle / 2.0); // 起始角度
        double endDeg = centerDeg + qRadiansToDegrees(arcAngle / 2.0);   // 结束角度

        // 转换为弧度
        double startRad = qDegreesToRadians(startDeg);
        double endRad = qDegreesToRadians(endDeg);

        // 计算圆弧的起点、终点
        QPointF p1(centerX + distance * cos(startRad),
                   centerY + distance * sin(startRad));
        QPointF p2(centerX + distance * cos(endRad),
                   centerY + distance * sin(endRad));

        // 计算控制点（贝塞尔曲线逼近圆弧）
        double controlOffset = distance * 0.5519 * (arcAngle / (M_PI / 2.0));
        QPointF ctrl1 = p1 + QPointF(-controlOffset * sin(startRad), controlOffset * cos(startRad));
        QPointF ctrl2 = p2 + QPointF(controlOffset * sin(endRad), -controlOffset * cos(endRad));

        // 创建弧线
        biaodingArc = new QCPItemCurve(this);
        biaodingArc->start->setCoords(p1);
        biaodingArc->startDir->setCoords(ctrl1);
        biaodingArc->endDir->setCoords(ctrl2);
        biaodingArc->end->setCoords(p2);
        biaodingArc->setPen(redPen);

        line2 = new QCPItemLine(this);
        line2->setPen(QPen(Qt::red, 2, Qt::DashLine));
        double a = qDegreesToRadians(centerDeg);
        line2->start->setCoords(0, 0);
        line2->end->setCoords(300* qCos(a), 300* qSin(a));
    }

    replot(QCustomPlot::rpQueuedReplot);
    timer->start();
}

void myCustomPlot::stopBiaoding()
{
    timer->stop();
    if (biaodingLine) {
        removeItem(biaodingLine);
        biaodingLine = nullptr;
    }
    if (line1) {
        removeItem(line1);
        line1 = nullptr;
    }
    if (biaodingArc) {
        removeItem(biaodingArc);
        biaodingArc = nullptr;
    }
    if (line2) {
        removeItem(line2);
        line2 = nullptr;
    }
    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::setBiaodingColor(const QColor &color)
{
    QPen pen(color);
    pen.setWidthF(2);

    if (biaodingLine)
        biaodingLine->setPen(pen);
    if (line1)
        line1->setPen(pen);
    if (biaodingArc)
        biaodingArc->setPen(pen);
    if (line2)
        line2->setPen(pen);
    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::startPulse()
{
    pulseRadii.clear();
    pulseTimer->start(100); // 控制动画速度
}

void myCustomPlot::stopPulse()
{
    pulseTimer->stop();
    pulseRadii.clear();

    // 只移除脉冲波图
    for (QCPGraph* g : pulseGraphs) {
        removeGraph(g);
    }
    pulseGraphs.clear();


    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::initObstaclePool() {
    QPixmap triangle(20, 20); // 20x20 固定尺寸
    triangle.fill(Qt::transparent);
    QPainter painter(&triangle);
    painter.setRenderHint(QPainter::Antialiasing);
    QPolygon polygon;
    polygon << QPoint(10, 0) << QPoint(0, 20) << QPoint(20, 20);
    painter.setBrush(QColor(214,178,107));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(polygon);
    painter.end();

    for (int i = 0; i < 255; ++i)
    {
        QCPItemPixmap* pix = new QCPItemPixmap(this);
        pix->setPixmap(triangle);
        pix->setScaled(false); // 保证不随缩放变化
        pix->topLeft->setType(QCPItemPosition::ptAbsolute);  // 用像素坐标
        pix->setVisible(false);

        QCPItemText* label = new QCPItemText(this);
        label->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        label->position->setType(QCPItemPosition::ptAbsolute);
        label->setFont(QFont("Consolas", 10));
        label->setColor(Qt::white);
        label->setVisible(false);

        obstaclePool.append({pix, label});
    }
}

void myCustomPlot::updateObstacleItem(int index, const ObstacleInfo &obs)
{

}

QString myCustomPlot::getStateString(uint8_t dynProp) {
    switch (dynProp) {
    case 0x0: return "Moving";
    case 0x1: return "Stationary";
    case 0x2: return "Approaching";
    case 0x3: return "Turning Left";
    case 0x4: return "Turning Right";
    case 0x5: return "Unknown";
    case 0x6: return "Stopped";
    default: return "Unknown";
    }
}

QString myCustomPlot::getClassString(uint8_t objClass) {
    switch (objClass) {
    case 0x0: return "Point Type";
    case 0x1: return "Vehicle Type";
    default: return "Unknown Type";
    }
}

bool myCustomPlot::isTargetVisible(uint8_t dynProp) const
{
    // 假设你的 dynProp 定义如下（你可以根据实际修改）：
    switch (dynProp) {
    case 0: return currentTargetState.showMovingTarget;          // moving
    case 1: return currentTargetState.showStationaryTarget;      // stationary
    case 2: return currentTargetState.showOncomingTarget;        // oncoming
    case 3: return currentTargetState.showCrossingLeftTarget;    // crossing left
    case 4: return currentTargetState.showCrossingRightTarget;   // crossing right
    case 5: return currentTargetState.showUnknownTarget;         // stopped
    case 6: return currentTargetState.showStoppedTarget;         // stopped
    default: return currentTargetState.showUnknownTarget;        // unknown
    }
}

bool myCustomPlot::isTypeVisible(uint8_t objClass) const
{
    switch (objClass) {
    case 0: return currentTargetType.showPointType;
    case 1: return currentTargetType.showCarType;
    default: return false;  // 或者加 showUnknownType 也行
    }
}

void myCustomPlot::drawPulses()
{
    // 只清除脉冲图，不动其他图
    for (QCPGraph* g : pulseGraphs) {
        removeGraph(g);
    }
    pulseGraphs.clear();

    for (double radius : pulseRadii) {
        QVector<double> xData, yData;
        int pointCount = 100;
        double startAngle = M_PI / 4;       // 45°
        double endAngle = 3 * M_PI / 4;     // 135°

        for (int j = 0; j <= pointCount; ++j) {
            double angle = startAngle + (endAngle - startAngle) * j / pointCount;
            double x = radius * qCos(angle);
            double y = radius * qSin(angle);
            xData << x;
            yData << y;
        }

        QCPGraph* graph = addGraph();
        QColor color = QColor(0, 170, 255, 150 - (radius / maxRadius) * 120);
        graph->setPen(QPen(color, 2));
        graph->setData(xData, yData);

        pulseGraphs.append(graph); // 记录下来
    }


    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::initPlot()
{
    setBackground(QBrush(Qt::black));


    replot(QCustomPlot::rpQueuedReplot);
}

void myCustomPlot::arcFlashing()
{
    if (biaodingLine) {
        biaodingLine->setVisible(!biaodingLine->visible());
        line1->setVisible(!line1->visible());
        biaodingLine->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }
    if (biaodingArc) {
        biaodingArc->setVisible(!biaodingArc->visible());
        line2->setVisible(!line2->visible());
        biaodingArc->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
    }
}

void myCustomPlot::updatePulse()
{
    // 添加新波纹
    if (pulseRadii.isEmpty() || pulseRadii.last() > pulseDis) {
        pulseRadii.append(0.0);
    }

    // 所有波纹扩散
    for (int i = 0; i < pulseRadii.size(); ++i)
        pulseRadii[i] += pulseStep;

    // 移除超过最大半径的波纹
    while (!pulseRadii.isEmpty() && pulseRadii.first() > maxRadius)
        pulseRadii.removeFirst();

    drawPulses();
}
