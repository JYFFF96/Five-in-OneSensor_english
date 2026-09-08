#ifndef POSTIONINGVIEW_H
#define POSTIONINGVIEW_H

#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QWheelEvent>

class PostioningView : public QWidget
{
    Q_OBJECT
public:
    explicit PostioningView(QWidget *parent = nullptr);
    void addCoordinate(double latitude, double longitude);
    void clearData();
protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
signals:

private:
    QList<int> scaleList = {10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000, 250000, 500000};
    double currentIndex = 6;
    double centerLat, centerLon;
    QVector<QPointF> coordinates;
    bool hasCenterPoint = false;
};

#endif // POSTIONINGVIEW_H
