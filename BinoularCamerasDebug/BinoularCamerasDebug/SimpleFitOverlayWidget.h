
#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>
#include <QPolygonF>
#include <QRectF>
#include <QtMath>
#include "ImageUtils.h"
#include "OverlayTypes.h"

class SimpleFitOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit SimpleFitOverlayWidget(QWidget *parent=nullptr);
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

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QSizeF expectedSizePx_(double Z) const;

private:
    QImage currentImg_;
    QVector<QPolygonF> lanes_;
    QVector<ObstacleBox> obstacles_;
    QRectF chessRect_;
    bool haveChess_ = false;

    double HFOV_ = qDegreesToRadians(82.0);
    double VFOV_ = qDegreesToRadians(44.0);
    double boardWm_ = 0.90, boardHm_ = 0.40;
    double targetZ_ = 8.0;
    double sizeTol_ = 0.15;
    double centerTolRatio_ = 0.03;
};
