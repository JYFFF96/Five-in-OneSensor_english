#ifndef MAPBRIDGE_H
#define MAPBRIDGE_H

#include <QObject>
#include <QDebug>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "common.h"
class MapBridge : public QObject
{
    Q_OBJECT
public:
    MapBridge();
public slots:
    void receiveMessage(const QString &msg) {
        qDebug() << "Received message from HTML:" << msg;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(msg.toUtf8());
        if (!jsonDoc.isArray()) {
            qDebug() << "JSON Format error";
            return;
        }

        QJsonArray jsonArray = jsonDoc.array();
        QVector<GPSPoint> points;
        for (const QJsonValue &value : jsonArray) {
            GPSPoint pos;
            QJsonObject obj = value.toObject();
            pos.longitude = obj["lng"].toDouble();
            pos.latitude = obj["lat"].toDouble();
            qDebug() << "Longitude:" << pos.longitude << "Latitude:" << pos.latitude;
            points.append(pos);
        }
        emit sendPoints(points);
    }
public:
    void sendMessageToJs(const QString &message) {
        emit messageFromCpp(message);
    }

signals:
    void messageFromCpp(const QString &message);
    void sendPoints(QVector<GPSPoint> points);

private:
//    QVector<Position> points;
};

#endif // MAPBRIDGE_H
