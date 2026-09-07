
#include "PlaybackController.h"
#include <QDebug>
#include <QtEndian>

namespace {
constexpr int FAIRY_PACKET_SIZE = 1248;
constexpr int FAIRY_HEADER_SIZE = 42;
constexpr int FAIRY_BLOCK_COUNT = 8;
constexpr int FAIRY_BLOCK_SIZE = 148;
constexpr int FAIRY_CHANNELS = 48;
constexpr int FAIRY_DATA_BLOCK_SIZE = FAIRY_BLOCK_COUNT * FAIRY_BLOCK_SIZE;
constexpr double FAIRY_DISTANCE_RESOLUTION_M = 0.005; // 0.5 cm
}
PlaybackController::PlaybackController(QObject *parent)
    : QObject(parent) {}

bool PlaybackController::loadPCAP(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open PCAP file:" << filePath;
        return false;
    }

    frames.clear();
    currentFrame = 0;

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    file.seek(24);  // 跳过 PCAP 全局头

    accumulatedAngle = 0;
    lastAzimuth = 0;
    collectingFrame = false;
    QList<PointXYZI> framePoints;

    while (!stream.atEnd()) {
        // 读取 Packet Header（16 字节）
        if (file.bytesAvailable() < 16) break;
        char headerBuf[16];
        file.read(headerBuf, 16);
        quint32 incl_len = *reinterpret_cast<quint32*>(&headerBuf[8]);

        if (incl_len < 42 + FAIRY_PACKET_SIZE) {
            file.seek(file.pos() + incl_len);
            continue;
        }

        // 读取整个 UDP 封装包（含以太网/IP/UDP 头 + payload）
        QByteArray fullPacket = file.read(incl_len);
        if (fullPacket.size() < 42 + FAIRY_PACKET_SIZE) continue;

        // ✅ 提取雷达数据 Payload
        QByteArray payload = fullPacket.mid(42, FAIRY_PACKET_SIZE);
        const QByteArray blk = payload.mid(FAIRY_HEADER_SIZE, FAIRY_DATA_BLOCK_SIZE);

        for (int i = 0; i < FAIRY_BLOCK_COUNT; ++i) {
            QByteArray b = blk.mid(i * FAIRY_BLOCK_SIZE, FAIRY_BLOCK_SIZE);
            if (b.size() < FAIRY_BLOCK_SIZE) continue;
            if (quint8(b[0]) != 0xFF || quint8(b[1]) != 0xEE) continue;
            float azimuth = ((quint8(b[2]) << 8) | quint8(b[3])) / 100.f;

            if (!collectingFrame) {
                collectingFrame = true;
                accumulatedAngle = 0;
            } else {
                float delta = azimuth - lastAzimuth;
                if (delta < 0) delta += 360;
                accumulatedAngle += delta;
            }

            for (int j = 0; j < FAIRY_CHANNELS; ++j) {
                const QByteArray d = b.mid(4 + j * 3, 3);
                PointXYZI pt = parseChannel(d, azimuth, j);
                framePoints.append(pt);
            }

            lastAzimuth = azimuth;

            if (accumulatedAngle >= 360.0f) {
                frames.append(framePoints);
                framePoints.clear();
                collectingFrame = false;
                accumulatedAngle = 0;
            }
        }
    }
    if (!framePoints.isEmpty()) {
        frames.append(framePoints);
    }
    file.close();
    return !frames.isEmpty();
}

double PlaybackController::parseHeader(const QByteArray &h)
{
    uint64_t sec = 0; uint32_t us = 0;
    for (int i = 0; i < 6; ++i) sec = (sec << 8) | quint8(h[20 + i]);
    for (int i = 6; i < 10; ++i) us = (us << 8) | quint8(h[20 + i]);
    return sec + us / 1e6;
}

PointXYZI PlaybackController::parseChannel(const QByteArray &d, float azimuth, int channel)
{
    quint16 dist = (quint8(d[0]) << 8) | quint8(d[1]);
    dist &= 0x7FFF;
    quint8 inten = quint8(d[2]);

    double dd = dist * FAIRY_DISTANCE_RESOLUTION_M;
    double om = azimuth * M_PI / 180.0;
    double al = COR_VERT_ANG.value(channel, 0.0f) * M_PI / 180.0;

    PointXYZI pt;
    pt.x = dd * cos(al) * cos(om); // 前
    pt.y = dd * cos(al) * sin(om); // 左
    pt.z = dd * sin(al);           // 上
    pt.intensity = inten;

    return pt;
}

void PlaybackController::start()
{
    if (isPlaying || frames.isEmpty()) return;

    if (!playbackTimer) {
        playbackTimer = new QTimer(this);
        connect(playbackTimer, &QTimer::timeout, this, &PlaybackController::onPlaybackTick);
    }

    int intervalMs = static_cast<int>(1000.0 / (playbackSpeed * 10));
    playbackTimer->start(intervalMs);
    isPlaying = true;
}

void PlaybackController::pause()
{
    if (playbackTimer) playbackTimer->stop();
    isPlaying = false;
}

void PlaybackController::seekToFrame(int index)
{
    if (index >= 0 && index < frames.size()) {
        currentFrame = index;
        emit frameReady(frames[index], index);
    }
}

void PlaybackController::setLoop(bool enabled)
{
    loop = enabled;
}

void PlaybackController::setSpeed(double speed)
{
    playbackSpeed = speed;
    if (isPlaying && playbackTimer) {
        playbackTimer->setInterval(static_cast<int>(1000.0 / (playbackSpeed * 10)));
    }
}

int PlaybackController::frameCount() const
{
    return frames.size();
}

void PlaybackController::onPlaybackTick()
{
    if (currentFrame >= frames.size()) {
        if (loop) currentFrame = 0;
        else { pause(); return; }
    }

    emit frameReady( frames[currentFrame], currentFrame);
    ++currentFrame;
}
