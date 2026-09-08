// Exporter.cpp
#include "exporter.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QtEndian>
#include<QFileInfo>
// ---------- PCAP 文件头结构 ----------
struct PcapGlobalHeader {
    quint32 magic_number = 0xa1b2c3d4; // PCAP 魔数
    quint16 version_major = 2;         // 主版本号
    quint16 version_minor = 4;         // 副版本号
    qint32  thiszone = 0;              // 时区补偿，默认为 0
    quint32 sigfigs = 0;               // 精度（忽略）
    quint32 snaplen = 65535;           // 最大捕获包长度
    quint32 network = 1;               // 网络层类型，1 = Ethernet
};

// ---------- 每个数据包的头结构 ----------
struct PcapPacketHeader {
    quint32 ts_sec;     // 秒级时间戳
    quint32 ts_usec;    // 微秒级时间戳
    quint32 incl_len;   // 实际捕获长度
    quint32 orig_len;   // 原始包长度
};

ExporterWorker::ExporterWorker(QObject *parent) : QObject(parent) {}

void ExporterWorker::stop() {
    m_stopped = true;
}

void ExporterWorker::process(FrameTask task) {
    if (m_stopped) return;

    try {
        const ExportParams &params = task.params;
        const QString &path = params.filePath;
        bool append = (params.frameType != currentFrame && params.fileType == singleFile);

        switch (params.exportType) {
        case CSV:
            saveAsCSV(path, task.pointData, append);
            break;
        case LAS:
            saveAsLAS(path, task.pointData);
            break;
        case PCD:
            saveAsPCD(path, task.pointCloud, append);
            break;
        case PCAP:
            saveAsPCAP(path, task.pcapPackets, append);
            break;
        default:
            emit exportError("Unsupported export type");
            return;
        }

        emit frameExported(QDateTime::currentMSecsSinceEpoch());
    } catch (const std::exception &e) {
        emit exportError(QString("Export error: %1").arg(e.what()));
    }
}

void ExporterWorker::saveAsCSV(const QString &path, const QList<PointData> &pts, bool append) {
    QFile file(path);
    if (!file.open(append ? QFile::Append | QFile::Text : QFile::WriteOnly | QFile::Text)) {
        emit exportError("Cannot open CSV file");
        return;
    }
    QTextStream out(&file);
    for (const auto &pt : pts) {
        out << pt.x << "," << pt.y << "," << pt.z << "," << pt.intensity << "\n";
    }
    file.flush();
    file.close();
}

void ExporterWorker::saveAsLAS(const QString &path, const QList<PointData> &pts) {
    // Placeholder: Replace with real libLAS logic
    QFile file(path);
    if (!file.open(QFile::WriteOnly)) {
        emit exportError("Cannot open LAS file");
        return;
    }
    file.write("LAS HEADER\n");
    for (const auto &pt : pts) {
        QByteArray line = QByteArray::number(pt.x) + "," +
                          QByteArray::number(pt.y) + "," +
                          QByteArray::number(pt.z) + "," +
                          QByteArray::number(pt.intensity) + "\n";
        file.write(line);
    }
    file.close();
}

void ExporterWorker::saveAsPCD(const QString &path, const QList<PointXYZI> &pts, bool append) {
    QFile file(path);
    if (!file.open(append ? QFile::Append : QFile::WriteOnly)) {
        emit exportError("Cannot open PCD file");
        return;
    }
    for (const auto &pt : pts) {
        QByteArray line = QByteArray::number(pt.x) + " " +
                          QByteArray::number(pt.y) + " " +
                          QByteArray::number(pt.z) + " " +
                          QByteArray::number(pt.intensity) + "\n";
        file.write(line);
    }
    file.close();
}

void ExporterWorker::saveAsPCAP(const QString &path, const QList<QByteArray> &pkts, bool append) {
    QFile file(path);
    bool writeHeader = true;

    if (append && QFile::exists(path)) {
        // 如果是追加模式，且文件存在且大小 >= 24，则已包含标准头
        QFileInfo info(file);
        if (info.size() >= 24) {
            writeHeader = false;
        }
    }

    if (!file.open(append ? QFile::Append : QFile::WriteOnly)) {
        qWarning("Unable to open file for writing!");
        return;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // ✅ 写入标准 PCAP 全局头（如果需要）
    if (writeHeader) {
        PcapGlobalHeader gh;
        out.writeRawData(reinterpret_cast<const char*>(&gh), sizeof(gh));
    }

    // 时间起点
    quint32 sec = QDateTime::currentSecsSinceEpoch();
    quint32 usec = 0;

    for (const QByteArray &raw : pkts) {
        QByteArray fullPacket = wrapUdpPacket(raw);

        PcapPacketHeader ph;
        ph.ts_sec = sec;
        ph.ts_usec = usec;
        ph.incl_len = ph.orig_len = fullPacket.size();

        out.writeRawData(reinterpret_cast<const char*>(&ph), sizeof(ph));
        out.writeRawData(fullPacket.constData(), fullPacket.size());

        usec += 556; // Fairy MSOP 发送间隔约 555.55us
        if (usec >= 1000000) {
            sec += 1;
            usec -= 1000000;
        }
    }
}

QByteArray ExporterWorker::wrapUdpPacket(const QByteArray &payload)
{
    QByteArray pkt;

    // Ethernet Header: 14 字节 (目的 MAC + 源 MAC + 类型)
    pkt.append(QByteArray::fromHex("00112233445566778899aabb0800"));

    // IP Header: 20 字节
    quint8 ipHeader[20] = {
        0x45, 0x00, 0x00, 0x00, // version + IHL + TOS + total_length (待填)
        0x00, 0x00, 0x40, 0x00, // ID, flags, frag_offset
        0x40, 0x11, 0x00, 0x00, // TTL=64, protocol=UDP, checksum
        192, 168, 1, 1,         // 源 IP
        192, 168, 1, 2          // 目的 IP
    };
    quint16 ipTotalLen = qToBigEndian<quint16>(20 + 8 + payload.size());
    memcpy(&ipHeader[2], &ipTotalLen, 2);  // 设置 total_length 字段
    pkt.append(reinterpret_cast<const char*>(ipHeader), 20);

    // UDP Header: 8 字节 (src port + dst port + len + checksum)
    quint8 udpHeader[8] = {
        0x1A, 0x0B, // 源端口（示例为 6667）
        0x1A, 0x0C, // 目的端口（示例为 6668）
        0x00, 0x00, // 长度字段（待填）
        0x00, 0x00  // 校验（通常为 0）
    };
    quint16 udpLen = qToBigEndian<quint16>(8 + payload.size());
    memcpy(&udpHeader[4], &udpLen, 2);  // 设置 UDP 长度
    pkt.append(reinterpret_cast<const char*>(udpHeader), 8);

    // 附加原始 Payload（如 1248 字节的激光数据）
    pkt.append(payload);

    return pkt;
}

Exporter::Exporter(QObject *parent) : QObject(parent), worker(new ExporterWorker) {
    worker->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Exporter::frameExported, this, [=](qint64 ts) {
        qDebug() << "Frame exported at" << ts;
    });
    connect(worker, &ExporterWorker::frameExported, this, &Exporter::frameExported);
    connect(worker, &ExporterWorker::exportError, this, &Exporter::exportError);
    workerThread.start();
}

Exporter::~Exporter() {
    worker->stop();
    workerThread.quit();
    workerThread.wait();
}

void Exporter::enqueueFrame(const ExportParams &params,
                            const QList<PointData> &pointData,
                            const QList<PointXYZI> &pointCloud,
                            const QList<QByteArray> &pcapPackets)
{
    FrameTask task{params, pointData, pointCloud, pcapPackets};
    QMetaObject::invokeMethod(worker, "process",
                              Qt::QueuedConnection,
                              Q_ARG(FrameTask, task));
}
