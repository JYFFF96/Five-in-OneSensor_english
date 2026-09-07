#ifndef SATELLITECOUNTER_H
#define SATELLITECOUNTER_H

#include <QObject>
#include "common.h"

class SatelliteCounter : public QObject
{
    Q_OBJECT
public:
    SatelliteCounter(QObject *parent = nullptr);
    void StartParseData(Postion pos);

signals:
    void sendVisualSatelliteList(QVector<SatelliteData> list);

private:
    QList<TleData> tleDataList;
};

#endif // SATELLITECOUNTER_H
