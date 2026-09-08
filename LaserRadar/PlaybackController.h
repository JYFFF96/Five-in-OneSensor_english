
#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include <QObject>
#include <QFile>
#include <QTimer>
#include <QList>
#include <QByteArray>
#include "common.h"

class PlaybackController : public QObject
{
    Q_OBJECT
public:
    explicit PlaybackController(QObject *parent = nullptr);
    bool loadPCAP(const QString &filePath);
    void start();
    void pause();
    void seekToFrame(int index);
    void setLoop(bool loop);
    void setSpeed(double factor);
    int frameCount() const;

signals:
    void frameReady(const QList<PointXYZI> &cloud, int frameIdx);

private slots:
    void onPlaybackTick();

private:
    double parseHeader(const QByteArray &h);
    PointXYZI parseChannel(const QByteArray &d, float azimuth, int channel);

private:
    QVector<QList<PointXYZI>> frames;
    QTimer* playbackTimer = nullptr;
    int currentFrame = 0;
    double playbackSpeed = 1.0;
    bool loop = false;
    bool isPlaying = false;

    // 帧组装状态
    float accumulatedAngle = 0;
    float lastAzimuth = 0;
    bool collectingFrame = false;
};

#endif // PLAYBACKCONTROLLER_H
