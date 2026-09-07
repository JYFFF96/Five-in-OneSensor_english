#ifndef UDPRECEIVER_H
#define UDPRECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <memory>
#include <fstream>

#include "common.h"
#include "exportframedialog.h"
#include "exporter.h"

#include <liblas/liblas.hpp>
#include <QByteArray>

class UdpReceiver : public QObject
{
    Q_OBJECT

public:
    explicit UdpReceiver(QObject *parent = nullptr);
    ~UdpReceiver();

    void start(quint16 dataPort, quint16 devInfoPort);
    void stop();
    void closeConnection();

signals:
    void dataUpdated(const QList<PointXYZI>&);
    void pointDataUpdated(const QList<PointData>&);
    void exportFinished();
    void exportFrameReady(const ExportParams& params,
                          const QList<PointData>& pointData,
                          const QList<PointXYZI>& pointCloud,
                          const QList<QByteArray>& pcapPackets);

public slots:
    void onExportRequested(const ExportParams& params);
    void stopExport();

private slots:
    void onReadyRead1();
    void onReadyRead2();

private:
    void parseMSOP(const QByteArray& data);
    void parseHeader(const QByteArray& header);
    void parseDataBlock(const QByteArray& dataBlock);
    void parseChannelData(const QByteArray& channelData, float azimuth, int index, double R, double timestamp);
    bool parseVerticalAndHorizontalAngles(const QByteArray& datagram);
    float parseSignedAngle3Bytes(uint8_t signByte, uint8_t highByte, uint8_t lowByte);

    void handleExportFrame();
    void finalizeExport();
    void appendToSingleFile();
    void parseDIFOPPacket(const QByteArray &data);

private:
    QUdpSocket *socket1 = nullptr;
    QUdpSocket *socket2 = nullptr;
    QMutex mutex;

    bool running = false;
    bool collectingFrame = false;
    float accumulatedAngle = 0.0f;
    float lastAzimuth = 0.0f;
    double currentTimestamp = 0.0;

    QList<PointXYZI> pointList;
    QList<PointData> pointDataList;
    QList<QByteArray> pcapPacketsBuffer;

    // QVector<float> COR_VERT_ANG;
    QVector<float> COR_HOR_ANG;

    bool exportActive = false;
    int exportFrameCounter = 0;
    ExportParams currentExportParams;
    QString dirPath;

    QFile csvFile;
    QTextStream* csvStream = nullptr;

    std::ofstream lasOfs;
    liblas::Header lasHeader;
    std::unique_ptr<liblas::Writer> lasWriter;
    Exporter *exporter;
};

#endif // UDPRECEIVER_H
