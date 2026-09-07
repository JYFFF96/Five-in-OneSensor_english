#include "common.h"
#include <random>
#include <QRandomGenerator>

double getRandomValue(double min, double max) {
    quint64 range = 1000000; // 放大倍数，例如 1,000,000
    quint64 randomInt = QRandomGenerator::global()->bounded(range); // 生成 0 ~ range-1 之间的随机整数
    return min + (randomInt / static_cast<double>(range)) * (max - min);
}

// 计算 NMEA 校验和
QString calculateChecksum(const QString &nmea) {
    int checksum = 0;
    for (int i = 1; i < nmea.length(); ++i) {
        checksum ^= nmea[i].toLatin1();
    }
    return QString("*%1").arg(checksum, 2, 16, QChar('0')).toUpper();
}

// 生成随机整数
int getRandomInt(int min, int max) {
    static std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> distrib(min, max);
    return distrib(gen);
}

// 生成 RMC 语句
QString generateRMC(const QString& time, double latitude, double longitude, double speed, double heading, QString date) {
    QString sentence = QString("$GPRMC,%1,A,%2,%3,%4,%5,%6,%7,%8,,,A")
                           .arg(time)
                           .arg(latitude, 0, 'f', 4)
                           .arg(latitude > 0 ? "N" : "S")
                           .arg(longitude, 0, 'f', 4)
                           .arg(latitude > 0 ? "E" : "W")
                           .arg(speed * 1.94384, 0, 'f', 2)  // 转换为节
                           .arg(heading, 0, 'f', 2)
                           .arg(date);
    return sentence + calculateChecksum(sentence);
}

// 生成 VTG 语句
QString generateVTG(double speed, double heading) {
    QString sentence = QString("$GPVTG,%1,T,,M,%2,N,%3,K,A")
    .arg(heading, 0, 'f', 2)
        .arg(speed * 1.94384, 0, 'f', 2)  // 节
        .arg(speed, 0, 'f', 2);           // km/h
    return sentence + calculateChecksum(sentence);
}

// 生成 GGA 语句
QString generateGGA(const QString& time, double latitude, double longitude, double altitude) {
    QString sentence = QString("$GPGGA,%1,%2,N,%3,E,1,12,0.8,%4,M,0.0,M,,")
    .arg(time)
        .arg(latitude, 0, 'f', 4)
        .arg(longitude, 0, 'f', 4)
        .arg(altitude, 0, 'f', 1);
    return sentence + calculateChecksum(sentence);
}

// 生成 GSA 语句
QString generateGSA() {
    char mode = 'A'; // 自动模式
    int fixType = QRandomGenerator::global()->bounded(1, 4); // 1=无定位，2=2D，3=3D

    // 随机生成 3~12 颗卫星
    QList<int> satellites;
    int numSatellites = QRandomGenerator::global()->bounded(3, 13); // 3 到 12 颗卫星
    for (int i = 0; i < numSatellites; ++i) {
        int prn = QRandomGenerator::global()->bounded(1, 33); // PRN 范围: 1-32
        satellites.append(prn);
    }

    // 生成 PDOP, HDOP, VDOP（随机 0.5 ~ 5.0）
    double pdop = 0.5 + QRandomGenerator::global()->generateDouble() * 4.5;
    double hdop = 0.5 + QRandomGenerator::global()->generateDouble() * 4.5;
    double vdop = 0.5 + QRandomGenerator::global()->generateDouble() * 4.5;

    // 组装 NMEA 语句
    QStringList gsaList;
    gsaList << "$GPGSA";
    gsaList << QString(mode);
    gsaList << QString::number(fixType);

    // 添加最多 12 颗卫星 PRN
    for (int i = 0; i < 12; ++i) {
        if (i < satellites.size()) {
            gsaList << QString::number(satellites[i]);
        } else {
            gsaList << ""; // 填充空位
        }
    }

    // 添加 PDOP、HDOP、VDOP
    gsaList << QString::number(pdop, 'f', 1);
    gsaList << QString::number(hdop, 'f', 1);
    gsaList << QString::number(vdop, 'f', 1);

    // 生成 GSA 语句
    QString gsaSentence = gsaList.join(",");

    return gsaSentence + calculateChecksum(gsaSentence);
}

// 生成 GSV 语句（随机 3-5 种 GNSS）
QString generateGSV() {
    QList<QString> gsvSentences;
    QList<QString> systems = {"GP", "GL", "GB", "GA", "QZ"}; // GPS, GLONASS, BeiDou, Galileo, QZSS
    int numSystems = getRandomInt(3, 5); // 生成 3~5 种 GNSS
    std::shuffle(systems.begin(), systems.end(), std::mt19937(std::random_device()()));

    for (int sysIndex = 0; sysIndex < numSystems; ++sysIndex) {
        QString system = systems[sysIndex];
        int totalSatellites = getRandomInt(10, 15); // 10~15 颗卫星
        int totalMessages = (totalSatellites + 3) / 4;

        for (int msgNum = 1; msgNum <= totalMessages; ++msgNum) {
            QString sentence = QString("$%1GSV,%2,%3,%4")
            .arg(system)
                .arg(totalMessages)
                .arg(msgNum)
                .arg(totalSatellites);

            for (int i = 0; i < 4 && (msgNum - 1) * 4 + i < totalSatellites; ++i) {
                int prn = getRandomInt(1, 32);
                int elevation = getRandomInt(0, 90);
                int azimuth = getRandomInt(0, 360);
                int snr = getRandomInt(20, 50);
                sentence += QString(",%1,%2,%3,%4").arg(prn).arg(elevation).arg(azimuth).arg(snr);
            }

            gsvSentences.append(sentence + calculateChecksum(sentence));
        }
    }
    return gsvSentences.join("\n");
}

// 生成 GLL 语句
QString generateGLL(const QString& time, double latitude, double longitude) {
    QString sentence = QString("$GPGLL,%1,N,%2,E,%3,A")
    .arg(latitude, 0, 'f', 4)
        .arg(longitude, 0, 'f', 4)
        .arg(time);
    return sentence + calculateChecksum(sentence);
}

QString generateGST(const QString &time) {
    double range_rms = getRandomValue(0.5, 5.0);   // 伪距均方根误差 (m)
    double std_major = getRandomValue(0.1, 2.0);   // 主轴标准差 (m)
    double std_minor = getRandomValue(0.1, 2.0);   // 次轴标准差 (m)
    double orient = getRandomValue(0.0, 180.0);        // 椭圆方向角 (°)
    double std_lat = getRandomValue(0.1, 3.0);     // 纬度标准差 (m)
    double std_lon = getRandomValue(0.1, 3.0);     // 经度标准差 (m)
    double std_alt = getRandomValue(0.1, 5.0);     // 高度标准差 (m)

    // 组装 GST 语句
    QString sentence = QString("$GPGST,%1,%2,%3,%4,%5,%6,%7,%8")
                           .arg(time)
                           .arg(range_rms, 0, 'f', 2)
                           .arg(std_major, 0, 'f', 2)
                           .arg(std_minor, 0, 'f', 2)
                           .arg(orient, 0, 'f', 2)
                           .arg(std_lat, 0, 'f', 2)
                           .arg(std_lon, 0, 'f', 2)
                           .arg(std_alt, 0, 'f', 2);

    return sentence + calculateChecksum(sentence);
}

QColor getSatelliteColor(const QString &name) {
    if (name.contains("GPS")) return Qt::blue;
    if (name.contains("GLONASS")) return Qt::yellow;
    if (name.contains("BeiDou")) return Qt::red;
    if (name.contains("Galileo")) return QColor(0, 255, 255); // Cyan
    if (name.contains("QZS")) return Qt::magenta;
    return Qt::gray; // 其他类型默认灰色
}
