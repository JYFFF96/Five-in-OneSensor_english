
#pragma once
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPoint>
#include <QImage>
#include <QRectF>
#include <QVector>
#include <QPolygonF>
    #include <QtMath>
#include <opencv2/opencv.hpp>
#include "OverlayItem.h"
#include "ImageUtils.h"

class ImageViewWithOverlays : public QGraphicsView {
    Q_OBJECT
public:
    explicit ImageViewWithOverlays(QWidget *parent=nullptr);

    void setFitToWindow(bool on);
    void setFovDegrees(double hfovDeg, double vfovDeg);
    void setBoardSizeMeters(double w, double h);
    void setTargetDistance(double zMeters);
    void setTolerances(double sizeRelTol, double centerTolRatio);

    void setImage(const QImage &img);
    void setImage(const cv::Mat &mat);

    void setLanePolylines(const QVector<QPolygonF> &lanes);
    void setObstacles(const QVector<ObstacleBox> &obs);
    void setChessboardBoundingRect(const QRectF &r);
    void setChessboardCorners(const QVector<QPointF> &pts);

    QPointF mapToImage(const QPoint &viewPt) const;

signals:
    void zoomChanged(double scale);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

private:
    void updateSceneRect();
    void fitInViewImage();
    void resetView_();
    void setZoom_(double z);
    void zoomBy_(double factor);
    QSizeF expectedSizePx_(double Z) const;
    void updateTargetBox_();
    void updateFitState_();

private:
    QGraphicsScene *scene_ = nullptr;
    QGraphicsPixmapItem *pixItem_ = nullptr;
    OverlayItem *overlay_ = nullptr;

    QImage currentImg_;
    bool fitToWindow_ = true;
    bool panning_ = false;
    bool panWithSpace_ = false;
    QPoint lastPan_;

    double HFOV_ = qDegreesToRadians(82.0);
    double VFOV_ = qDegreesToRadians(44.0);
    double boardWm_ = 0.90, boardHm_ = 0.40;
    double targetZ_ = 8.0;
    double sizeTol_ = 0.15;
    double centerTolRatio_ = 0.03;
    QRectF chessRect_;
};
