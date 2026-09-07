#include "satellitecounter.h"
#include <QCoreApplication>
#include <QFile>
#include "SGP4.h"
#include <QDebug>
#include "Observer.h"
#include <QProcess>

using namespace libsgp4;
using namespace std;

#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

// **地理坐标（经纬度 -> ECEF）**
void GeodeticToECEF(double lat, double lon, double alt, double& x, double& y, double& z) {
    double a = 6378.137;  // 地球赤道半径 (km)
    double e2 = 0.00669437999014; // 偏心率平方

    double sinLat = sin(lat * DEG2RAD);
    double cosLat = cos(lat * DEG2RAD);
    double sinLon = sin(lon * DEG2RAD);
    double cosLon = cos(lon * DEG2RAD);

    double N = a / sqrt(1 - e2 * sinLat * sinLat);

    x = (N + alt) * cosLat * cosLon;
    y = (N + alt) * cosLat * sinLon;
    z = (N * (1 - e2) + alt) * sinLat;
}

// **将 ECEF 坐标转换为 ENU（东-北-天）**
void ECEFtoENU(double obsX, double obsY, double obsZ, double satX, double satY, double satZ,
               double lat, double lon, double& east, double& north, double& up) {
    double sinLat = sin(lat * DEG2RAD);
    double cosLat = cos(lat * DEG2RAD);
    double sinLon = sin(lon * DEG2RAD);
    double cosLon = cos(lon * DEG2RAD);

    double dx = satX - obsX;
    double dy = satY - obsY;
    double dz = satZ - obsZ;

    east = -sinLon * dx + cosLon * dy;
    north = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz;
    up = cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;
}

// **计算方位角（Azimuth）和俯仰角（Elevation）**
void CalculateAzEl(double east, double north, double up, double& azimuth, double& elevation) {
    azimuth = atan2(east, north) * RAD2DEG;
    if (azimuth < 0) azimuth += 360.0;

    double range = sqrt(east * east + north * north + up * up);
    elevation = asin(up / range) * RAD2DEG;
}

SatelliteCounter::SatelliteCounter(QObject *parent)
    : QObject(parent)
{
    // 先更新TLE文件

    QString tleFilePath = QCoreApplication::applicationDirPath() + "/config/TLE.txt";
    QString logFilePath = QCoreApplication::applicationDirPath() + "/config/Tle_log.txt";

    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 记录日志
    QFile logfile(logFilePath);
    if (logfile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logfile);
        stream << "[" << timestamp << "] Retrieving TLE Data...\n";
        logfile.close();
    }

    // 执行 curl 下载 TLE 数据
    QStringList arguments;
    arguments << "-s" << "https://celestrak.org/NORAD/elements/gp.php?GROUP=gnss&FORMAT=tle" << "-o" << tleFilePath;

    QProcess *process = new QProcess(this);
    process->start("curl", arguments);

    if (!process->waitForFinished()) {
        qDebug() << "Execution failed：" << process->errorString();
    } else {
        qDebug() << "Execution succeeded";
        qDebug() << "Output：" << process->readAllStandardOutput();
        qDebug() << "Error：" << process->readAllStandardError();
    }

    // 再次写入日志
    if (logfile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logfile);
        if (QFile::exists(tleFilePath)) {
            stream << "[" << timestamp << "] TLE Data retrieved successfully，saved to " << tleFilePath << "\n";
        }
        logfile.close();
    }





    QFile file(tleFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Unable to open TLE File：" << tleFilePath;
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString name = in.readLine().trimmed();
        QString line1 = in.readLine().trimmed();
        QString line2 = in.readLine().trimmed();

        if (!name.isEmpty() && line1.startsWith("1 ") && line2.startsWith("2 ")) {
            TleData tle = { name.toStdString(), line1.toStdString(), line2.toStdString() };
            tleDataList.append(tle);
        }
    }

    file.close();
}

void SatelliteCounter::StartParseData(Postion pos)
{
    QVector<SatelliteData> visualSatelliteList;
    foreach (TleData tleData, tleDataList) {
        Tle tle(tleData.name, tleData.line1, tleData.line2);
        SGP4 sgp4(tle);

        // **观测者的时间**
        DateTime now(pos.time.date().year(), pos.time.date().month(), pos.time.date().day(), pos.time.time().hour(), pos.time.time().minute(), pos.time.time().second());  // UTC 时间 2024年2月26日 12:00:00
        double jd = now.ToJulian();             // 转换为儒略日
        // **计算卫星位置**
        Eci eci = sgp4.FindPosition(jd);

        // **转换为地理坐标**
        CoordGeodetic geo = eci.ToGeodetic();

        // **观测者的地理坐标**
        double obsLat = pos.latitude;
        double obsLon = pos.longitude;
        double obsAlt = pos.height;

        // **计算 ECEF 坐标**
        double obsX, obsY, obsZ;
        GeodeticToECEF(obsLat, obsLon, obsAlt, obsX, obsY, obsZ);

        // **卫星的 ECEF 坐标**
        double satX = eci.Position().x;
        double satY = eci.Position().y;
        double satZ = eci.Position().z;

        // **转换 ECEF 到 ENU**
        double east, north, up;
        ECEFtoENU(obsX, obsY, obsZ, satX, satY, satZ, obsLat, obsLon, east, north, up);

        // **计算方位角和俯仰角**
        double azimuth, elevation;
        CalculateAzEl(east, north, up, azimuth, elevation);

        // 俯仰角大于0可见
        if (elevation > 0) {
            SatelliteData sate;
            sate.name = QString::fromStdString(tleData.name);
            sate.latitude = geo.latitude * 180 / M_PI;
            sate.longitude = geo.longitude * 180 / M_PI;
            sate.height = geo.altitude;
            sate.azimuth = azimuth;
            sate.elevation = elevation;
            visualSatelliteList.append(sate);
        }
    }
    if (!visualSatelliteList.isEmpty()) {
        emit sendVisualSatelliteList(visualSatelliteList);
    }
}
