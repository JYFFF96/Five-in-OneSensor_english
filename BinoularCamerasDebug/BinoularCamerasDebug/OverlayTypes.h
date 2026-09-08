
#pragma once
#include <QRectF>
#include <QString>
#include <QColor>
struct ObstacleBox {
    QRectF bbox;
    QString label;
    double score = -1.0;
    QColor color = QColor(255, 200, 0);
};
