#include "udpreceiver.h"
#include <QDebug>
#include <QDateTime>
#include <QtEndian>
#include <QDir>
#include <cmath>

/******************  UdpReceiver.cpp (修正版)  *******************/

namespace {
constexpr int FAIRY_PACKET_SIZE = 1248;
constexpr int FAIRY_HEADER_SIZE = 42;
constexpr int FAIRY_BLOCK_COUNT = 8;
constexpr int FAIRY_BLOCK_SIZE = 148;
constexpr int FAIRY_CHANNELS = 48;
constexpr int FAIRY_DATA_BLOCK_SIZE = FAIRY_BLOCK_COUNT * FAIRY_BLOCK_SIZE;
constexpr double FAIRY_DISTANCE_RESOLUTION_M = 0.005; // 0.5 cm
constexpr int FAIRY_DIFOP_VERT_ANG_OFFSET = 468;
constexpr int FAIRY_DIFOP_HOR_ANG_OFFSET = 756;
constexpr int FAIRY_DIFOP_ANGLE_BYTES = 288;
}

UdpReceiver::UdpReceiver(QObject *parent) : QObject(parent) {
    exporter = new Exporter(this);
}

UdpReceiver::~UdpReceiver()
{
    delete exporter;
    if (running) stop();
}

/* -------------------------- 启停 -------------------------- */
void UdpReceiver::start(quint16 dataPort, quint16 devInfoPort)
{
    if (running) return;

    socket1 = new QUdpSocket(this);
    socket2 = new QUdpSocket(this);

    bool ok1 = socket1->bind(QHostAddress::AnyIPv4, dataPort, QUdpSocket::ShareAddress);
    bool ok2 = socket2->bind(QHostAddress::AnyIPv4, devInfoPort, QUdpSocket::ShareAddress);

    if (!ok1 || !ok2) {
        qWarning() << "UDP bind failed:"
                   << "dataPort=" << dataPort << socket1->errorString()
                   << "devInfoPort=" << devInfoPort << socket2->errorString();

        stop();
        return;
    }

    connect(socket1, &QUdpSocket::readyRead, this, &UdpReceiver::onReadyRead1);
    connect(socket2, &QUdpSocket::readyRead, this, &UdpReceiver::onReadyRead2);

    running = true;
}

void UdpReceiver::stop()
{
    if (!running) return;
    if (socket1) { socket1->close(); socket1->deleteLater(); socket1=nullptr; }
    if (socket2) { socket2->close(); socket2->deleteLater(); socket2=nullptr; }
    running=false; collectingFrame=false;
}

void UdpReceiver::closeConnection() { stop(); }

/* --------------------- 导出控制 ---------------------- */
void UdpReceiver::onExportRequested(const ExportParams &p)
{
    currentExportParams=p; exportFrameCounter=0; exportActive=true;
    dirPath = p.filePath;
    if (p.fileType==singleFile && p.exportType==CSV) {
        csvFile.setFileName(p.filePath);
        if (csvFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
            csvStream=new QTextStream(&csvFile);
            *csvStream<<"time,x,y,z,azimuth,distance,intensity,channel\n";
        }
    } else if (p.fileType==singleFile && p.exportType==LAS) {
        lasOfs.open(p.filePath.toStdString(),std::ios::binary);
        if (lasOfs.is_open()) {
            lasHeader.SetDataFormatId(liblas::ePointFormat1);
            lasHeader.SetScale(0.001,0.001,0.001);
            lasWriter=std::make_unique<liblas::Writer>(lasOfs,lasHeader);
        }
    }
}

void UdpReceiver::stopExport()
{
    exportActive=false;
    if (csvStream){ delete csvStream; csvStream=nullptr; }
    if (csvFile.isOpen()) csvFile.close();
    if (lasWriter) lasWriter.reset();
    if (lasOfs.is_open()) lasOfs.close();
    emit exportFinished();
}

/* ------------------- 数据接收 --------------------- */
void UdpReceiver::onReadyRead1()
{
    while(socket1 && socket1->hasPendingDatagrams()){
        QByteArray d; d.resize(socket1->pendingDatagramSize());
        socket1->readDatagram(d.data(),d.size());
        QMutexLocker lock(&mutex);
        if(exportActive)
            pcapPacketsBuffer.append(d);
        parseMSOP(d);
    }
}

void UdpReceiver::onReadyRead2()
{
    while(socket2 && socket2->hasPendingDatagrams()){
        QByteArray d; d.resize(socket2->pendingDatagramSize());
        socket2->readDatagram(d.data(),d.size());
        parseVerticalAndHorizontalAngles(d);
        parseDIFOPPacket(d);
    }
}

/* ------------------ 解析与帧组装 ------------------ */
void UdpReceiver::parseMSOP(const QByteArray &data)
{
    if (data.size() != FAIRY_PACKET_SIZE) return;
    parseHeader(data.left(FAIRY_HEADER_SIZE));
    parseDataBlock(data.mid(FAIRY_HEADER_SIZE, FAIRY_DATA_BLOCK_SIZE));
}

void UdpReceiver::parseHeader(const QByteArray &h)
{
    uint64_t sec=0; uint32_t us=0;
    for(int i=0;i<6;++i) sec=(sec<<8)|quint8(h[20+i]);
    for(int i=6;i<10;++i)us=(us<<8)|quint8(h[20+i]);
    currentTimestamp=sec+us/1e6;
}

void UdpReceiver::parseDataBlock(const QByteArray &blk)
{
    if (blk.size() < FAIRY_DATA_BLOCK_SIZE) return;
    if (COR_VERT_ANG.size() < FAIRY_CHANNELS) return;

    for (int i = 0; i < FAIRY_BLOCK_COUNT; ++i) {
        const QByteArray b = blk.mid(i * FAIRY_BLOCK_SIZE, FAIRY_BLOCK_SIZE);
        if (b.size() < FAIRY_BLOCK_SIZE) continue;

        // Fairy Data Block: 0~1 为 0xFFEE，2~3 为方位角，4 开始为 48 路通道数据。
        if (quint8(b[0]) != 0xFF || quint8(b[1]) != 0xEE) {
            continue;
        }

        float az = ((quint8(b[2]) << 8) | quint8(b[3])) / 100.0f;
        if (!collectingFrame) {
            collectingFrame = true;
            accumulatedAngle = 0;
        } else {
            float d = az - lastAzimuth;
            if (d < 0)
                d += 360;
            accumulatedAngle += d;
        }

        for (int j = 0; j < FAIRY_CHANNELS; ++j) {
            parseChannelData(b.mid(4 + j * 3, 3), az, j, 0.0, currentTimestamp);
        }

        lastAzimuth = az;
        if (accumulatedAngle >= 360.0f) {
            emit dataUpdated(pointList);
            emit pointDataUpdated(pointDataList);
            if (exportActive) handleExportFrame();
            pointList.clear();
            pointDataList.clear();
            pcapPacketsBuffer.clear();
            collectingFrame = false;
            accumulatedAngle = 0;
        }
    }
}

void UdpReceiver::parseChannelData(const QByteArray &d,float az,int idx,double R,double t)
{
    Q_UNUSED(R);
    if (d.size() < 3 || idx < 0 || idx >= COR_VERT_ANG.size()) return;

    // Fairy: Distance 取低 15 bits，分辨率 0.5 cm。
    quint16 rawDist = (quint8(d[0]) << 8) | quint8(d[1]);
    rawDist &= 0x7FFF;
    quint8 inten = quint8(d[2]);

    double dd = rawDist * FAIRY_DISTANCE_RESOLUTION_M;
    double om = az * M_PI / 180.0;
    double al = COR_VERT_ANG[idx] * M_PI / 180.0;
    double x = dd * cos(al) * cos(om); // 前
    double y = dd * cos(al) * sin(om); // 左
    double z = dd * sin(al);           // 上
    pointList.append({static_cast<float>(x),static_cast<float>(y),static_cast<float>(z),static_cast<float>(inten)});
    pointDataList.append({t,x,y,z,az,dd,static_cast<float>(inten),idx});
}

bool UdpReceiver::parseVerticalAndHorizontalAngles(const QByteArray &d)
{
    // Fairy DIFOP: 垂直角校准 offset 468，长度 288；水平角校准 offset 756，长度 288。
    if (d.size() < FAIRY_DIFOP_HOR_ANG_OFFSET + FAIRY_DIFOP_ANGLE_BYTES) return false;

    QVector<float> v(FAIRY_DIFOP_ANGLE_BYTES / 3);
    QVector<float> h(FAIRY_DIFOP_ANGLE_BYTES / 3);
    const uint8_t* r = reinterpret_cast<const uint8_t*>(d.constData());

    for (int i = 0; i < v.size(); ++i) {
        const int vo = FAIRY_DIFOP_VERT_ANG_OFFSET + i * 3;
        const int ho = FAIRY_DIFOP_HOR_ANG_OFFSET + i * 3;
        v[i] = parseSignedAngle3Bytes(r[vo], r[vo + 1], r[vo + 2]);
        h[i] = parseSignedAngle3Bytes(r[ho], r[ho + 1], r[ho + 2]);
    }

    // Fairy 手册里的校准表是 96 路，当前 MSOP 只用前 48 路通道。
    COR_VERT_ANG = v.mid(0, FAIRY_CHANNELS);
    COR_HOR_ANG  = h.mid(0, FAIRY_CHANNELS);
    return true;
}

float UdpReceiver::parseSignedAngle3Bytes(uint8_t s,uint8_t h,uint8_t l)
{ int v=(h<<8)|l; return s==0x01?-v*0.01f:v*0.01f; }

/* ------------------- 导出核心 ------------------- */
void UdpReceiver::handleExportFrame()
{
    if (currentExportParams.fileType == singleFile) {
        appendToSingleFile();
    } else {
        currentExportParams.filePath = dirPath + "/" +QString::number(exportFrameCounter);
    }

    ++exportFrameCounter;

    bool finish = false;
    if (currentExportParams.frameType == currentFrame)
        finish = true;
    if (currentExportParams.frameType == rangeFrame &&
        exportFrameCounter > currentExportParams.endFrame - currentExportParams.startFrame)
        finish = true;
    // 发出异步写入信号（Exporter 线程处理）
    // emit exportFrameReady(currentExportParams, pointDataList, pointList, pcapPacketsBuffer);
    qDebug() << "exportFrameReady : " << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    exporter->enqueueFrame(currentExportParams, pointDataList, pointList, pcapPacketsBuffer);

    if (finish) stopExport();
    pointList.clear();
    pointDataList.clear();
    pcapPacketsBuffer.clear();
    collectingFrame = false;
}

void UdpReceiver::appendToSingleFile()
{
    if(currentExportParams.exportType==CSV && csvStream){
        for(auto &pt:pointDataList)
            *csvStream<<pt.time<<","<<pt.x<<","<<pt.y<<","<<pt.z<<","<<pt.azimuthValue<<","<<pt.distance<<","<<pt.intensity<<","<<pt.channel<<"\n";
    } else if(currentExportParams.exportType==LAS && lasWriter){
        for(auto &pd:pointDataList){
            liblas::Point p(&lasHeader); p.SetCoordinates(pd.x,pd.y,pd.z); p.SetIntensity(pd.intensity); lasWriter->WritePoint(p);
        }
    }
}

void UdpReceiver::parseDIFOPPacket(const QByteArray &data)
{
    if (data.size() < FAIRY_PACKET_SIZE) {
        qWarning() << "DIFOP incomplete";
        return;
    }
    if (!(quint8(data[0]) == 0xA5 &&
          quint8(data[1]) == 0xFF &&
          quint8(data[2]) == 0x00 &&
          quint8(data[3]) == 0x5A)) {
        qWarning() << "Invalid packet header";
        return;
    }

    DifopInfo info;
    // --------- 1. Fairy 电机实时转速 (offset 373, 2 bytes) ---------
    info.speed = static_cast<quint8>(data[373]) << 8 | static_cast<quint8>(data[374]);

    // --------- 2. Fairy 以太网信息 ---------
    // source IP:10, dest IP:14, MAC:18, MSOP:24, DIFOP:28
    int offset = 18;

    QString lidarIp = QString("%1.%2.%3.%4")
                          .arg(static_cast<quint8>(data[10]))
                          .arg(static_cast<quint8>(data[11]))
                          .arg(static_cast<quint8>(data[12]))
                          .arg(static_cast<quint8>(data[13]));

    QString destIp = QString("%1.%2.%3.%4")
                         .arg(static_cast<quint8>(data[14]))
                         .arg(static_cast<quint8>(data[15]))
                         .arg(static_cast<quint8>(data[16]))
                         .arg(static_cast<quint8>(data[17]));

    QString macAddr;
    for (int i = 0; i < 6; ++i) {
        macAddr += QString("%1").arg(static_cast<quint8>(data[offset + i]), 2, 16, QLatin1Char('0'));
        if (i != 5) macAddr += ":";
    }

    quint16 msopPort = static_cast<quint8>(data[24]) << 8 | static_cast<quint8>(data[25]);
    quint16 difopPort = static_cast<quint8>(data[28]) << 8 | static_cast<quint8>(data[29]);

    info.netInfo = InternetInfo {
        lidarIp,
        destIp,
        macAddr,
        msopPort,
        difopPort
    };

    // --------- 3. FOV 设置 (offset 32, 4 bytes) ---------
    info.fov_start = (static_cast<quint8>(data[32]) << 8 | static_cast<quint8>(data[33])) / 100;
    info.fov_end   = (static_cast<quint8>(data[34]) << 8 | static_cast<quint8>(data[35])) / 100;

    // --------- 4. 电机锁相相位 (offset 38, 2 bytes) ---------
    info.mot_phase = static_cast<quint8>(data[38]) << 8 | static_cast<quint8>(data[39]);

    // --------- 5. 回波（返回）模式 (offset 300, 1 byte) ---------
    info.returnMode = static_cast<quint8>(data[300]);

    // --------- 6. Fairy 无雨雾模式字段，这里置 0，避免沿用 Helios16 的 offset 302 ---------
    info.rainMode = 0;

    // --------- 7. 时间同步信息 (offset 301:方式，302:状态) ---------
    info.timeSyncInfo.type   = static_cast<quint8>(data[301]);
    info.timeSyncInfo.status = static_cast<quint8>(data[302]);

}
