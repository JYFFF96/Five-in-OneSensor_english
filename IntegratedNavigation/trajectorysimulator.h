#ifndef TRAJECTORYSIMULATOR_H
#define TRAJECTORYSIMULATOR_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QVector>
#include <QtMath>
#include "common.h"


class TrajectorySimulator : public QObject
{
    Q_OBJECT
public:
    TrajectorySimulator(QObject *parent = nullptr);
    void setTrajectory(const QVector<GPSPoint> &points, QDateTime startTime, double speed, double height);
    double haversine(double lat1, double lon1, double lat2, double lon2);

    void updateSegment();
    void stop();
private:
    GPSPoint interpolate(const GPSPoint& p1, const GPSPoint& p2, double ratio);
    double calculateBearing(double lat1, double lon1, double lat2, double lon2);
    carPostionInfo calculateCarPosInfo(GPSPoint currentPos);
signals:
    void sendCurrentPos(GPSPoint pos);
    void sendCurrentCarInfo(carPostionInfo info);

private slots:
    void updatePosition();
private:
    QVector<GPSPoint> waypoints;
    QDateTime startTime;
    QTimer* timer;
    double speed;
    double height;
    int currentSegment;
    double currentTime;
    double segmentTime;
    double angle; // 方向角
};

#endif // TRAJECTORYSIMULATOR_H
