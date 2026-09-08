
#include "OverlayItem.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

OverlayItem::OverlayItem() {
    setZValue(10.0);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
}
void OverlayItem::setImageRect(const QRectF &r){ imgRect_ = r; prepareGeometryChange(); }
void OverlayItem::setCrosshairEnabled(bool on){ crosshair_ = on; update(); }
void OverlayItem::setTargetBox(const QSizeF &szPx){ targetSize_ = szPx; update(); }
void OverlayItem::setChessboardRect(const QRectF &r){ chessRect_ = r; haveChess_ = r.isValid(); update(); }
void OverlayItem::setChessboardCorners(const QVector<QPointF> &pts){
    chessRect_ = pts.isEmpty()? QRectF() : QPolygonF(pts).boundingRect();
    haveChess_ = !pts.isEmpty(); update();
}
void OverlayItem::setLanes(const QVector<QPolygonF> &lanes){ lanes_ = lanes; update(); }
void OverlayItem::setObstacles(const QVector<ObstacleBox> &obs){ obstacles_ = obs; update(); }
void OverlayItem::setFitOk(bool ok){ fitOk_ = ok; update(); }

QRectF OverlayItem::boundingRect() const { return imgRect_; }

void OverlayItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget*) {
    if (imgRect_.isEmpty()) return;
    p->setRenderHint(QPainter::Antialiasing, true);

    QColor main = fitOk_ ? QColor(0,200,0) : QColor(220,0,0);
    QPen mainPen(main, 2.0); mainPen.setCosmetic(true);

    if (crosshair_) {
        const QPointF c = imgRect_.center();
        p->setPen(mainPen);
        p->drawLine(QPointF(imgRect_.left(), c.y()), QPointF(imgRect_.right(), c.y()));
        p->drawLine(QPointF(c.x(), imgRect_.top()), QPointF(c.x(), imgRect_.bottom()));
    }

    if (targetSize_.isValid()) {
        QRectF r(imgRect_.center().x() - targetSize_.width()/2.0,
                imgRect_.center().y() - targetSize_.height()/2.0,
                targetSize_.width(), targetSize_.height());
        p->setPen(mainPen);
        p->drawRect(r);
    }

    if (haveChess_ && chessRect_.isValid()) {
        QPen det(Qt::yellow, 1.0, Qt::DashLine); det.setCosmetic(true);
        p->setPen(det); p->drawRect(chessRect_);
    }

    QPen lanePen(Qt::white, 2.0); lanePen.setCosmetic(true);
    p->setPen(lanePen);
    for (const auto &poly : lanes_) if (poly.size() >= 2) p->drawPolyline(poly);

    for (const auto &ob : obstacles_) {
        QPen bp(ob.color, 2.0); bp.setCosmetic(true);
        p->setPen(bp); p->setBrush(Qt::NoBrush);
        p->drawRect(ob.bbox);

        const QString txt = ob.score >= 0 ? QString("%1 %.0f%%").arg(ob.label).arg(ob.score*100.0)
                                          : ob.label;
        if (!txt.isEmpty()) {
            QFont f = p->font(); f.setPointSizeF(f.pointSizeF()+1); p->setFont(f);
            QRectF tbr = QFontMetricsF(f).boundingRect(txt).adjusted(-4,-2,4,2);
            tbr.moveTopLeft(ob.bbox.topLeft() + QPointF(1,1));
            p->setPen(Qt::NoPen); p->setBrush(QColor(0,0,0,160)); p->drawRect(tbr);
            p->setPen(Qt::white); p->drawText(tbr, Qt::AlignLeft|Qt::AlignVCenter, txt);
        }
    }
}
