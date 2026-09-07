#ifndef MYCUSTOMPLOT_H
#define MYCUSTOMPLOT_H

#include "qcustomplot.h"
#include "common.h"

struct ObstacleItem {
    QCPItemPixmap* trianglePixmap;
    QCPItemText *label;
};
class myCustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    explicit myCustomPlot(QWidget *parent = nullptr);
    void updatePlot(double x, double y, int angle);
    void drawObstaclesWithText(const QList<ObstacleInfo> &obsList);
    void clearObstacles();

    void setLabelDisplayOptions(const LabelDisplayOptions &options);  // 添加接口
    void updateTargetState(const showTargetState &state);  // 公有函数
    void updateTargetType(const showTargetType &state);  // 公有函数
    void startBiaoding(double distance, double angle); // index:几号雷达 distance:标定距离
    void stopBiaoding();
    void setBiaodingColor(const QColor &color);

    void startPulse();
    void stopPulse();

private:
    void drawLines(double angle);
    void onMouseMove(QMouseEvent *event);
    void drawTriangleWithText(const ObstacleInfo &obs);

    void initObstaclePool();
    void updateObstacleItem(int index, const ObstacleInfo &obs);
    QString getStateString(uint8_t dynProp);
    QString getClassString(uint8_t objClass);
    bool isTargetVisible(uint8_t dynProp) const;
    bool isTypeVisible(uint8_t objClass) const;
    void drawPulses();
    void initPlot();

protected:
    void wheelEvent(QWheelEvent *event) override;  // 处理滚轮事件
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private slots:
    void arcFlashing();
    void updatePulse();
signals:
    void sendRange(double xRange, double yRange);

private:
    int currentAngle = 120;
    QVector<ObstacleItem> obstaclePool;
    QPixmap trianglePixmap; // 用来缓存红色三角形图像
    LabelDisplayOptions labelOptions;  // ✅ 使用没问题
    showTargetState currentTargetState;
    showTargetType currentTargetType;

    QTimer *timer; // 绿色弧线闪动用的

    QCPItemLine *biaodingLine = nullptr;
    QCPItemLine *line1 = nullptr;
    QCPItemCurve *biaodingArc = nullptr;
    QCPItemLine *line2 = nullptr;


    QTimer* pulseTimer;
    QVector<double> pulseRadii;

    const double maxRadius = 200.0;
    double pulseStep = 1.0;
    double pulseDis = 5.0;
    QVector<QCPGraph*> pulseGraphs;
    QVector<QCPGraph*> angleGraphs;
};

#endif // MYCUSTOMPLOT_H
