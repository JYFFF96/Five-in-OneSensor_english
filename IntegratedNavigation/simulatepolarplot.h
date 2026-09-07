#ifndef SIMULATEPOLARPLOT_H
#define SIMULATEPOLARPLOT_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QMap>
#include <QVector>
#include <QString>
#include "common.h"

class SimulatePolarPlot : public QWidget {
    Q_OBJECT

public:
    explicit SimulatePolarPlot(QWidget *parent = nullptr);
    void setSatelliteData(const QVector<SatelliteGsvData> &data);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QVector<SatelliteGsvData> satelliteData;
    QMap<QString, QColor> countryColors;
    QMap<QString, int> countryCounts;

    void drawPolarGrid(QPainter &painter);
    void drawSatellites(QPainter &painter);
    void drawLegend(QPainter &painter);
    QPointF polarToCartesian(double azimuth, double elevation) const;
    QString getSatelliteType(const QString &name) const;
    QColor getCountryColor(const QString &country) const;
};

#endif // SIMULATEPOLARPLOT_H
