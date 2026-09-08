#include "trajectorysimulator.h"

TrajectorySimulator::TrajectorySimulator(QObject *parent)
    : QObject(parent),
      speed(speed),
      currentSegment(0),
      currentTime(0)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TrajectorySimulator::updatePosition);
}

void TrajectorySimulator::setTrajectory(const QVector<GPSPoint> &points, QDateTime startTime, double speed, double height)
{
    waypoints = points;
    this->speed = speed;
    this->startTime = startTime;
    this->height = height;
}

double TrajectorySimulator::haversine(double lat1, double lon1, double lat2, double lon2)
{
    constexpr double R = 6371000; // 地球半径（米）
    double dLat = qDegreesToRadians(lat2 - lat1);
    double dLon = qDegreesToRadians(lon2 - lon1);

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(qDegreesToRadians(lat1)) * std::cos(qDegreesToRadians(lat2)) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);

    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
    return R * c;
}



void TrajectorySimulator::updateSegment()
{
    if (currentSegment >= waypoints.size() - 1) return;

    double distance = haversine(
        waypoints[currentSegment].latitude, waypoints[currentSegment].longitude,
        waypoints[currentSegment + 1].latitude, waypoints[currentSegment + 1].longitude
        );
    angle = calculateBearing(waypoints[currentSegment].latitude, waypoints[currentSegment].longitude,
                             waypoints[currentSegment + 1].latitude, waypoints[currentSegment + 1].longitude);
    segmentTime = distance / speed;
    waypoints[currentSegment + 1].time = waypoints[currentSegment].time + segmentTime;
    timer->start(1000);
}

void TrajectorySimulator::stop()
{
    timer->stop();
    currentTime = 0;
    currentSegment = 0;
}

GPSPoint TrajectorySimulator::interpolate(const GPSPoint &p1, const GPSPoint &p2, double ratio)
{
    GPSPoint interpolated;
    interpolated.latitude = p1.latitude + ratio * (p2.latitude - p1.latitude);
    interpolated.longitude = p1.longitude + ratio * (p2.longitude - p1.longitude);
    return interpolated;
}

double TrajectorySimulator::calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    // 转换为弧度
    double radLat1 = qDegreesToRadians(lat1);
    double radLat2 = qDegreesToRadians(lat2);
    double dLon = qDegreesToRadians(lon2 - lon1);

    // 计算方向角
    double y = qSin(dLon) * qCos(radLat2);
    double x = qCos(radLat1) * qSin(radLat2) - qSin(radLat1) * qCos(radLat2) * qCos(dLon);
    double bearing = qRadiansToDegrees(qAtan2(y, x));

    // 转换为 0° - 360°
    return fmod((bearing + 360.0), 360.0);
}

carPostionInfo TrajectorySimulator::calculateCarPosInfo(GPSPoint currentPos)
{
    carPostionInfo info;
    info.date = startTime.toString("ddMMyy");
    info.time = startTime.addSecs(currentTime).toString("hhmmss");
    info.latitude = currentPos.latitude;
    info.longitude = currentPos.longitude;
    info.altitude = height;
    info.speed = speed;
    info.heading = angle;
    return info;
}

void TrajectorySimulator::updatePosition()
{
    if (currentSegment >= waypoints.size() - 1) {
        qDebug() << "Vehicle reached the destination; simulation finished!";
        stop();
        return;
    }

    // 计算当前秒在路径上的比例
    double ratio = (currentTime - waypoints[currentSegment].time) / segmentTime;
    if (ratio > 1.0) ratio = 1.0;

    // 计算当前位置
    GPSPoint currentPos = interpolate(waypoints[currentSegment], waypoints[currentSegment + 1], ratio);
//    qDebug() << "Time:" << currentTime << "s -> Lat:" << currentPos.latitude << ", Lon:" << currentPos.longitude;
    // 把当前数据信息发到MainWindow， 然后生成NMEA语句
    carPostionInfo info = calculateCarPosInfo(currentPos);
    emit sendCurrentCarInfo(info);
    emit sendCurrentPos(currentPos);
    // 进入下一个点
    if (currentTime >= waypoints[currentSegment + 1].time) {
        currentSegment++;
        updateSegment();
    }

    currentTime++;
}
