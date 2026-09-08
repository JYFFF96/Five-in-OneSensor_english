#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QDateTime>
#include <QtMath>
#include <QDebug>
#include <QList>
#include <QTime>
#include <QtGlobal>
#include <QColor>
// 结构体表示坐标点
struct GPSPoint {
    double latitude;  // 纬度
    double longitude; // 经度
    double time;      // 该点的时间戳（秒）
};

struct carPostionInfo {
    QString date;   // UTC 时间 (ddmmyy)
    QString time;   // UTC 时间 (hhmmss)
    double latitude; // 纬度
    double longitude; // 经度
    double altitude; // 高度 (米)
    double speed; // 速度 (m/s)
    double heading; // 航向角
};

struct TleData {
    std::string name;
    std::string line1;
    std::string line2;
};

struct Postion {
    double latitude;  // 纬度
    double longitude; // 经度
    double height; // 高 单位：km
    QDateTime time;      // 时间
};

struct SatelliteData {
    QString name;
    double latitude;  // 纬度
    double longitude; // 经度
    double height; // 高 单位：km
    double azimuth; // 方位角
    double elevation; // 俯仰角
};

struct SatelliteGsvData {
    QString name;    // 卫星类型+编号
    double azimuth;  // 方位角 (0°-360°)
    double elevation;// 俯仰角 (0°-90°)
    int snr;         // 信噪比 (0-99, 若无则为 -1)
};

struct GSTData {
    QString utcTime;
    double rangeRMS;
    double stdMajor;
    double stdMinor;
    double orient;
    double stdLat;
    double stdLon;
    double stdAlt;
};

double getRandomValue(double min, double max);
QString calculateChecksum(const QString &nmea);
int getRandomInt(int min, int max);
QString generateRMC(const QString& time, double latitude, double longitude, double speed, double heading, QString date);
QString generateVTG(double speed, double heading);
QString generateGGA(const QString& time, double latitude, double longitude, double altitude);
QString generateGSA();
QString generateGSV();
QString generateGLL(const QString& time, double latitude, double longitude);
QString generateGST(const QString &time);
QColor getSatelliteColor(const QString &name);

#endif // COMMON_H
