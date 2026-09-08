
#include "SimpleFitOverlayWidget.h"
#include <QPainter>

SimpleFitOverlayWidget::SimpleFitOverlayWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(480, 270);
}
void SimpleFitOverlayWidget::setFovDegrees(double hfovDeg, double vfovDeg){ HFOV_ = qDegreesToRadians(hfovDeg); VFOV_ = qDegreesToRadians(vfovDeg); update(); }
void SimpleFitOverlayWidget::setBoardSizeMeters(double w, double h){ boardWm_ = w; boardHm_ = h; update(); }
void SimpleFitOverlayWidget::setTargetDistance(double zMeters){ targetZ_ = zMeters; update(); }
void SimpleFitOverlayWidget::setTolerances(double sizeRelTol, double centerTolRatio){ sizeTol_ = sizeRelTol; centerTolRatio_ = centerTolRatio; update(); }

void SimpleFitOverlayWidget::setImage(const QImage &img){ currentImg_ = img; update(); }
void SimpleFitOverlayWidget::setImage(const cv::Mat &mat){ setImage(ImageUtils::matToQImage(mat)); }
void SimpleFitOverlayWidget::setLanePolylines(const QVector<QPolygonF> &lanes){ lanes_ = lanes; update(); }
void SimpleFitOverlayWidget::setObstacles(const QVector<ObstacleBox> &obs){ obstacles_ = obs; update(); }
void SimpleFitOverlayWidget::setChessboardBoundingRect(const QRectF &r){ chessRect_ = r; haveChess_ = r.isValid(); update(); }
void SimpleFitOverlayWidget::setChessboardCorners(const QVector<QPointF> &pts){ chessRect_ = pts.isEmpty()? QRectF() : QPolygonF(pts).boundingRect(); haveChess_ = !pts.isEmpty(); update(); }

QSizeF SimpleFitOverlayWidget::expectedSizePx_(double Z) const {
    if (HFOV_<=0 || VFOV_<=0 || currentImg_.isNull()) return {};
    const double angW = 2.0 * std::atan(boardWm_/(2.0*Z));
    const double angH = 2.0 * std::atan(boardHm_/(2.0*Z));
    const double wpx = currentImg_.width()  * (angW / HFOV_);
    const double hpx = currentImg_.height() * (angH / VFOV_);
    return QSizeF(wpx, hpx);
}

void SimpleFitOverlayWidget::paintEvent(QPaintEvent *){
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::black);
    if (currentImg_.isNull()) return;

    const QSizeF imgSize(currentImg_.width(), currentImg_.height());
    const QSizeF wndSize(width(), height());
    const double s = qMin(wndSize.width()/imgSize.width(), wndSize.height()/imgSize.height());
    const QSizeF drawSize(imgSize.width()*s, imgSize.height()*s);
    const QPointF topLeft((wndSize.width()-drawSize.width())/2.0, (wndSize.height()-drawSize.height())/2.0);
    const QRectF imgRectOnWidget(topLeft, drawSize);
    p.drawImage(imgRectOnWidget, currentImg_);

    const QSizeF targetPx = expectedSizePx_(targetZ_);
    const QPointF imgCenter(imgSize.width()/2.0, imgSize.height()/2.0);
    const QRectF targetRectImg(imgCenter.x()-targetPx.width()/2.0,
                               imgCenter.y()-targetPx.height()/2.0,
                               targetPx.width(), targetPx.height());

    auto mapPt = [&](const QPointF &pt){ return QPointF(topLeft.x()+pt.x()*s, topLeft.y()+pt.y()*s); };
    auto mapRect = [&](const QRectF &r){ return QRectF(mapPt(r.topLeft()), QSizeF(r.width()*s, r.height()*s)); };
    auto mapPoly = [&](const QPolygonF &poly){ QPolygonF out; out.reserve(poly.size()); for(const auto &pt: poly) out << mapPt(pt); return out; };

    bool fitOk = false;
    if (haveChess_ && chessRect_.isValid() && targetPx.isValid()) {
        const double wErr = std::abs(chessRect_.width()  - targetPx.width())  / qMax(1.0, targetPx.width());
        const double hErr = std::abs(chessRect_.height() - targetPx.height()) / qMax(1.0, targetPx.height());
        const QPointF c1 = chessRect_.center();
        const double centerTol = centerTolRatio_ * qMin(currentImg_.width(), currentImg_.height());
        const double cdist = std::hypot(c1.x()-imgCenter.x(), c1.y()-imgCenter.y());
        fitOk = (qMax(wErr, hErr) <= sizeTol_) && (cdist <= centerTol);
    }
    const QColor main = fitOk ? QColor(0,200,0) : QColor(220,0,0);
    QPen mainPen(main, 2.0); mainPen.setCosmetic(true); // cosmetic pen
    // correct boolean

    p.setPen(mainPen);

    const QPointF cW = mapPt(imgCenter);
    p.drawLine(QPointF(imgRectOnWidget.left(), cW.y()), QPointF(imgRectOnWidget.right(), cW.y()));
    p.drawLine(QPointF(cW.x(), imgRectOnWidget.top()), QPointF(cW.x(), imgRectOnWidget.bottom()));

    p.drawRect(mapRect(targetRectImg));

    if (haveChess_) {
        QPen det(Qt::yellow, 1.0, Qt::DashLine); det.setCosmetic(true);
        p.setPen(det); p.drawRect(mapRect(chessRect_));
    }

    QPen lanePen(Qt::white, 2.0); lanePen.setCosmetic(true);
    p.setPen(lanePen);
    for (const auto &poly : lanes_) if (poly.size()>=2) p.drawPolyline(mapPoly(poly));

    for (const auto &ob : obstacles_) {
        QPen bp(ob.color, 2.0); bp.setCosmetic(true); p.setPen(bp); p.setBrush(Qt::NoBrush);
        p.drawRect(mapRect(ob.bbox));
        const QString txt = ob.score>=0 ? QString("%1 %.0f%%").arg(ob.label).arg(ob.score*100.0) : ob.label;
        if (!txt.isEmpty()) {
            QFont f = p.font(); f.setPointSizeF(f.pointSizeF()+1); p.setFont(f);
            QRectF tbr = QFontMetricsF(f).boundingRect(txt).adjusted(-4,-2,4,2);
            QRectF topLeftR = mapRect(ob.bbox);
            tbr.moveTopLeft(topLeftR.topLeft() + QPointF(1,1));
            p.setPen(Qt::NoPen); p.setBrush(QColor(0,0,0,160)); p.drawRect(tbr);
            p.setPen(Qt::white); p.drawText(tbr, Qt::AlignLeft|Qt::AlignVCenter, txt);
        }
    }
}
