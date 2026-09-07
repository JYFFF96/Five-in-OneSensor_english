#ifndef ARCWIDGET_H
#define ARCWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QtMath>

class ArcWidget : public QWidget
{
    Q_OBJECT
public:
    ArcWidget(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *e) override;
private:
    void drawTicks(QPainter &painter, const QRectF &rect, int startAngle, int spanAngle);
    void drawRollTriangle(QPainter &painter, const QRectF &rect);
    void drawPitchTriangle(QPainter &painter, const QRectF &rect);
    void drawRollPicture(QPainter &painter, const QRectF &rect);
    void drawPitchPicture(QPainter &painter, const QRectF &rect);
public slots:
    void updateRoll(double angle);
    void updatePitch(double angle);

private:
    double rollAngle = 0;
    double pitchAngle = 0;
};

#endif // ARCWIDGET_H
