#ifndef SIMPLEPOINTCLOUDVIEW_H
#define SIMPLEPOINTCLOUDVIEW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QList>
#include <QPoint>
#include <QMatrix4x4>
#include <QPainterPath>
#include <QFont>
#include <QVector3D>
#include <vector>
#include "common.h"
#include "colorbarwidget.h"
#include <Eigen/Dense>
struct OBB {
    QVector3D center;      // 盒子中心
    QMatrix3x3 R;          // 3x3 旋转矩阵（列为主轴）
    QVector3D half;        // 半尺寸 (hx,hy,hz)
    int num_points = 0;
};

struct OBBParams {
    float voxel = 0.1f;   // 滤波设置（体素下采样，=0 则不下采样）
    float eps   = 0.30f;   // 欧氏距离 (m)
    int   minPts= 20;       // 最少点云数
    int   maxPts= 500;     // 最大点云数
};
class SimplePointCloudView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit SimplePointCloudView(QWidget *parent = nullptr);
    void updatePointCloud(QList<PointXYZI> pointList);
    void resetView();
    void setColorBarWidget(ColorBarWidget *colorBar);
    void clearPointCloud();
    void setGridVisible(bool visible);
    float pointSize = 2.0f;
    void setOBBParams(const OBBParams& p) { m_obbParam = p; recomputeOBB(); }

    bool drawObb = false;
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QList<PointXYZI> points;
    float zoom = 1.0f;
    float xRot = 20.0f, yRot = -30.0f;
    QPoint lastMousePos;
    bool gridVisible = true;
    ColorBarWidget *colorBarWidget = nullptr;


    void drawGrid();
    QColor getColorFromIntensity(float intensity);
    void drawTextStroke3D(const QVector3D &pos, const QString &text, const QColor &color);
    std::vector<QVector<QVector3D>> generateTextPath3D(const QString &text, const QFont &font);
    void drawHudCoordinateAxes();
    void drawHudBillboardText(const QVector3D &screenPos, const QString &text, const QColor &color);

    void drawPointCloud();
    void drawOBBs();
    QList<Obstacle> obstacles;

    void recomputeOBB();          // 计算 OBB
    void drawOBB(const OBB& b);   // 画 OBB
    OBBParams m_obbParam;
    std::vector<OBB> m_boxes;

};

#endif // SIMPLEPOINTCLOUDVIEW_H
