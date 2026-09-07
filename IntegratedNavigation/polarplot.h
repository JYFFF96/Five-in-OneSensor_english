#ifndef POLARPLOT_H
#define POLARPLOT_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMap>
#include <QVector>
#include <QString>
#include "common.h"
// struct SatelliteData {
//     QString name;
//     double azimuth;   // 方位角 (0°-360°)
//     double elevation; // 俯仰角 (0°-90°)
//     double longitude; // 经度
//     double latitude;  // 纬度
//     double altitude;  // 高度 (km)
// };
class PolarPlot : public QWidget {
    Q_OBJECT

public:
    explicit PolarPlot(QWidget *parent = nullptr);
    void setSatelliteData(QVector<SatelliteData> data);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QVector<SatelliteData> satelliteData;
    QMap<QString, int> countryCounts;
    QVector<QString> fixedCountries = {"China", "United States", "Russia", "European Union", "Japan", "India", "Other"};

    void drawPolarGrid(QPainter &painter);
    void drawSatellites(QPainter &painter);
    void drawLegend(QPainter &painter);
    QPointF polarToCartesian(double azimuth, double elevation) const;
    QString getCountry(const QString &name) const;
    QColor getCountryColor(const QString &country) const;
};

#endif // POLARPLOT_H
