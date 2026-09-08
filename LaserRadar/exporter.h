// Exporter.h
#ifndef EXPORTER_H
#define EXPORTER_H

#include <QObject>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "common.h"

struct FrameTask {
    ExportParams params;
    QList<PointData> pointData;
    QList<PointXYZI> pointCloud;
    QList<QByteArray> pcapPackets;
};

class ExporterWorker : public QObject {
    Q_OBJECT
public:
    explicit ExporterWorker(QObject *parent = nullptr);
    void stop();

public slots:
    void process(FrameTask task);

signals:
    void frameExported(qint64 timestamp);
    void exportError(const QString &msg);

private:
    void saveAsCSV(const QString &path, const QList<PointData> &pts, bool append);
    void saveAsLAS(const QString &path, const QList<PointData> &pts);
    void saveAsPCD(const QString &path, const QList<PointXYZI> &pts, bool append);
    void saveAsPCAP(const QString &path, const QList<QByteArray> &pkts, bool append);
    QByteArray wrapUdpPacket(const QByteArray &payload);
    bool m_stopped = false;
};

class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject *parent = nullptr);
    ~Exporter();

    void enqueueFrame(const ExportParams &params,
                      const QList<PointData> &pointData,
                      const QList<PointXYZI> &pointCloud,
                      const QList<QByteArray> &pcapPackets);

signals:
    void frameExported(qint64 timestamp);
    void exportError(const QString &msg);

private:
    QThread workerThread;
    ExporterWorker *worker;

    QMutex m_mutex;
    QQueue<FrameTask> m_queue;
};

#endif // EXPORTER_H
