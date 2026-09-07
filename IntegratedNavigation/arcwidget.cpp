#include "arcwidget.h"

ArcWidget::ArcWidget(QWidget *parent)
    : QWidget(parent)
{

}

void ArcWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    int side = qMin(width(), height());
    painter.translate(width() / 2, height() / 2);
    painter.scale(side / 200.0, side / 200.0);  // 归一化到 200x200
    // 设置画笔
    QPen pen(Qt::white, 2);
    painter.setPen(pen);

    // 定义圆弧的外接矩形
    QRectF rect(-100, -100, 200, 200);

    // 起始角度和跨度角度（以1/16度为单位）
    int startAngle = 0 * 16; // 起始角度为0度
    int spanAngle = 40 * 16; // 跨度为40度

    // 绘制圆弧
    painter.drawArc(rect, startAngle, spanAngle);// 绘制刻度
    drawTicks(painter, rect, startAngle, spanAngle);
    painter.setPen(pen);
    startAngle = 140 * 16; // 起始角度为0度
    spanAngle = 40 * 16; // 跨度为40度
    painter.drawArc(rect, startAngle, spanAngle);// 绘制刻度
    drawTicks(painter, rect, startAngle, spanAngle);
    painter.setPen(pen);
    startAngle = 180 * 16; // 起始角度为0度
    spanAngle = 40 * 16; // 跨度为40度
    painter.drawArc(rect, startAngle, spanAngle);// 绘制刻度
    drawTicks(painter, rect, startAngle, spanAngle);
    painter.setPen(pen);
    startAngle = 320 * 16; // 起始角度为0度
    spanAngle = 40 * 16; // 跨度为40度
    painter.drawArc(rect, startAngle, spanAngle);

    // 绘制刻度
    drawTicks(painter, rect, startAngle, spanAngle);

    // 绘制三角形
    drawRollTriangle(painter, rect); // 滚转角
    drawPitchTriangle(painter, rect);  // 俯仰角
    // 绘制图片
    drawPitchPicture(painter, rect);
    painter.resetTransform();
    painter.translate(width() / 2, height() / 2);
    painter.scale(side / 200.0, side / 200.0);  // 归一化到 200x200
    drawRollPicture(painter, rect);
}

void ArcWidget::drawTicks(QPainter &painter, const QRectF &rect, int startAngle, int spanAngle) {
    // 圆心和半径
    QPointF center = rect.center();
    qreal radius = rect.width() / 2;

    // 刻度长度
    qreal smallTickLength = 10; // 普通刻度长度
    qreal largeTickLength = 20; // 加长刻度长度

    // 每10度绘制一个刻度
    for (int angle = 0; angle <= spanAngle / 16; angle += 5) {
        // 计算当前角度（以度为单位）
        qreal currentAngle = startAngle / 16 + angle;

        // 将角度转换为弧度
        qreal radian = qDegreesToRadians(currentAngle);

        // 判断是否为加长刻度（10°、20°、30°、40°）
        bool isLargeTick = true; // 每10°都是加长刻度



        bool b_needMark = false, b_isPositive;
        if (currentAngle == 0) {
            b_needMark = true;
            b_isPositive = true;
        } else if (currentAngle == 180) {
            b_needMark = true;
            b_isPositive = false;
        }
        // 绘制刻度值
        if (currentAngle <= 90) {

        } else if (currentAngle > 90 && currentAngle <= 180) {
            currentAngle = 180 - currentAngle;
        } else if (currentAngle > 180 && currentAngle <= 270) {
            currentAngle = currentAngle - 180;
        } else if (currentAngle > 270 && currentAngle <= 360) {
            currentAngle = 360 - currentAngle;
        }

        // 计算刻度线的长度
        qreal tickLength = ((int)currentAngle%10 == 0) ? largeTickLength : smallTickLength;

        // 计算刻度的起点和终点
        QPointF startPoint = center + QPointF(radius * qCos(radian), -radius * qSin(radian));
        QPointF endPoint = center + QPointF((radius + tickLength) * qCos(radian), -(radius + tickLength) * qSin(radian));

        // 设置刻度线颜色
//            QColor tickColor = Qt::black;
        QColor tickColor = (currentAngle >= 30) ? Qt::red : Qt::white;
        QPen tickPen(tickColor, 1);
        painter.setPen(tickPen);

        // 绘制刻度线
        painter.drawLine(startPoint, endPoint);

        // 如果角度不为0°，绘制刻度值
        if (currentAngle != 0) {
            // 设置刻度值颜色
//                QColor textColor = (currentAngle >= 30) ? Qt::red : Qt::black;
//                painter.setPen(textColor);

            // 计算刻度值的位置
            QPointF textPoint = center + QPointF((radius + tickLength + 15) * qCos(radian), -(radius + tickLength + 15) * qSin(radian));

            // 绘制刻度值
            if ((int)currentAngle%10 == 0)
            painter.drawText(textPoint, QString::number(currentAngle));
        } else {
            if (b_needMark) {
                QPointF textPoint = center + QPointF((radius + tickLength + 15) * qCos(radian), -(radius + tickLength + 15) * qSin(radian));
                if (b_isPositive) {
                    painter.drawText(textPoint, "+");
                } else {
                    painter.drawText(textPoint, "-");
                }
            }
        }
    }
}

void ArcWidget::drawRollTriangle(QPainter &painter, const QRectF &rect) {
    // 圆心和半径
    QPointF center = rect.center();
    qreal radius = rect.width() / 2;

    // 三角形的顶点指向30度方向
    qreal triangleAngle = 0;
    if (rollAngle >= 0) {
        triangleAngle = rollAngle;
    } else {
        triangleAngle = (180 - std::abs(rollAngle));
    }
    // qreal triangleAngle = 60; // 30度
    qreal radian = qDegreesToRadians(triangleAngle);

    // 三角形的顶点坐标
    QPointF topPoint = center + QPointF(radius * qCos(radian), -radius * qSin(radian));

    // 等边三角形的边长
    qreal sideLength = 20;

    // 计算底边的两个端点坐标
    // 底边垂直于顶点到圆心的连线
    qreal perpendicularAngle1 = triangleAngle + 120; // 第一个底边端点方向
    qreal perpendicularAngle2 = triangleAngle - 120; // 第二个底边端点方向
    qreal radian1 = qDegreesToRadians(perpendicularAngle1);
    qreal radian2 = qDegreesToRadians(perpendicularAngle2);

    // 底边的两个端点
    QPointF bottomLeft = topPoint + QPointF(sideLength * qCos(radian1), -sideLength * qSin(radian1));
    QPointF bottomRight = topPoint + QPointF(sideLength * qCos(radian2), -sideLength * qSin(radian2));
    // 设置三角形颜色
    painter.setBrush(Qt::blue); // 填充颜色
    painter.setPen(Qt::blue);   // 边框颜色

    // 绘制三角形
    QPolygonF triangle;
    triangle << topPoint << bottomLeft << bottomRight;
    painter.drawPolygon(triangle);
}

void ArcWidget::drawPitchTriangle(QPainter &painter, const QRectF &rect)
{
    QPointF center = rect.center();
    qreal radius = rect.width() / 2;

    // 三角形的顶点指向30度方向
    qreal triangleAngle = 0;
    if (rollAngle >= 0) {
        triangleAngle = 360 - rollAngle;
    } else {
        triangleAngle = (180 + std::abs(rollAngle));
    }
    // qreal triangleAngle = 60; // 30度
    qreal radian = qDegreesToRadians(triangleAngle);

    // 三角形的顶点坐标
    QPointF topPoint = center + QPointF(radius * qCos(radian), -radius * qSin(radian));

    // 等边三角形的边长
    qreal sideLength = 20;

    // 计算底边的两个端点坐标
    // 底边垂直于顶点到圆心的连线
    qreal perpendicularAngle1 = triangleAngle + 120; // 第一个底边端点方向
    qreal perpendicularAngle2 = triangleAngle - 120; // 第二个底边端点方向
    qreal radian1 = qDegreesToRadians(perpendicularAngle1);
    qreal radian2 = qDegreesToRadians(perpendicularAngle2);

    // 底边的两个端点
    QPointF bottomLeft = topPoint + QPointF(sideLength * qCos(radian1), -sideLength * qSin(radian1));
    QPointF bottomRight = topPoint + QPointF(sideLength * qCos(radian2), -sideLength * qSin(radian2));
    // 设置三角形颜色
    painter.setBrush(Qt::blue); // 填充颜色
    painter.setPen(Qt::blue);   // 边框颜色

    // 绘制三角形
    QPolygonF triangle;
    triangle << topPoint << bottomLeft << bottomRight;
    painter.drawPolygon(triangle);
}

void ArcWidget::drawPitchPicture(QPainter &painter, const QRectF &rect)
{
    QPixmap pixmap(":/images/cemian.png"); // 替换为你的图片路径

    if (pixmap.isNull()) {
        return; // 避免加载失败时崩溃
    }

    // 调整图片大小
    QPixmap scaledPixmap = pixmap.scaled(74, 74, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // 计算图片中心点（以 (-25,30) 作为左上角）
    QPoint center(0, 10 + 37);  // 50x50 图片中心为 (25,25)，加到左上角坐标上

    // 旋转图片
    painter.translate(center);  // 将原点移动到图片中心
    painter.rotate(pitchAngle);         // 旋转 45 度
    painter.translate(-center); // 复原原点

    // 在 (-25,30) 处绘制图片
    painter.drawPixmap(-37, 10, scaledPixmap);
}

void ArcWidget::drawRollPicture(QPainter &painter, const QRectF &rect)
{
    QPixmap pixmap(":/images/weibu.png"); // 替换为你的图片路径

    if (pixmap.isNull()) {
        return; // 避免加载失败时崩溃
    }

    // 调整图片大小
    QPixmap scaledPixmap = pixmap.scaled(74, 74, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // // 计算图片中心点（以 (-25,30) 作为左上角）
    QPoint center(-37 + scaledPixmap.width() / 2, -100 + scaledPixmap.height() / 2);  // 50x50 图片中心为 (25,25)，加到左上角坐标上

    // // 旋转图片
    painter.translate(center);  // 将原点移动到图片中心
    painter.rotate(rollAngle);         // 旋转 45 度
    painter.translate(-center); // 复原原点

    // 在 (-25,30) 处绘制图片
    painter.drawPixmap(-37, -100, scaledPixmap);
}


void ArcWidget::updateRoll(double angle)
{
    rollAngle = angle;
    update();
}

void ArcWidget::updatePitch(double angle)
{
    pitchAngle = angle;
    update();
}
