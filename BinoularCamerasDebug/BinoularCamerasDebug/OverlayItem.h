
#pragma once
#include <QGraphicsItem>
#include <QPen>
#include <QBrush>
#include <QPolygonF>
#include <QVector>
#include <QColor>
#include "OverlayTypes.h"

class OverlayItem : public QGraphicsItem {
public:
    explicit OverlayItem();
    void setImageRect(const QRectF &r);
    void setCrosshairEnabled(bool on);
    void setTargetBox(const QSizeF &szPx);
    void setChessboardRect(const QRectF &r);
    void setChessboardCorners(const QVector<QPointF> &pts);
    void setLanes(const QVector<QPolygonF> &lanes);
    void setObstacles(const QVector<ObstacleBox> &obs);
    void setFitOk(bool ok);

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *opt, QWidget*) override;
private:
    QRectF imgRect_;
    bool crosshair_ = true;
    QSizeF targetSize_;
    QRectF chessRect_;
    bool haveChess_ = false;
    bool fitOk_ = false;
    QVector<QPolygonF> lanes_;
    QVector<ObstacleBox> obstacles_;
};
