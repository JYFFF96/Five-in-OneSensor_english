#ifndef MYCUSTOMPLOT_H
#define MYCUSTOMPLOT_H

#include "qcustomplot.h"
#include "common.h"

class myCustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    explicit myCustomPlot(QWidget *parent = nullptr);
    void UpdateObstaclesDistance(double distance1, double distance2, double distance3, double distance4);
    void startBiaoding(int index, int distance); // index:几号雷达 distance:标定距离
    void setArcColor(QColor color);
    void deleteArc();
    void deltePCurve();
    // void startTimer(bool flag);

private:
    void initObstacles();
    void onMouseMove(QMouseEvent *event);
    void drawRedArc(double centerX, double centerY, double radius, double startAngle, double endAngle);
    QPair<QVector<double>, QVector<double>> CalculatePostion(double ox, double oy, double angle, double distance);

protected:
    void wheelEvent(QWheelEvent *event) override;  // 处理滚轮事件
    void resizeEvent(QResizeEvent *event) override;  // 监听窗口大小变化
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void arcFlashing();

signals:
    void sendRange(double xRange, double yRange);

private:
    // 四条线用来画三角形
    QCPCurve *LeftTop;
    double distance1 = 0;
    QCPCurve *RightTop;
    double distance2 = 0;
    QCPCurve *LeftBottom;
    double distance3 = 0;
    QCPCurve *RightBottom;
    double distance4 = 0;
    // QCPItemText *coordText;
    QCPCurve *arc = nullptr; // 标定用的圆弧
    QTimer *timer; // 绿色弧线闪动用的
};

#endif // MYCUSTOMPLOT_H
