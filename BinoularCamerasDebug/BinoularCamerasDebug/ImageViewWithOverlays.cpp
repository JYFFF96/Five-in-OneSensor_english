
#include "ImageViewWithOverlays.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtMath>
#include <cmath>

ImageViewWithOverlays::ImageViewWithOverlays(QWidget *parent)
    : QGraphicsView(parent), scene_(new QGraphicsScene(this)), pixItem_(new QGraphicsPixmapItem), overlay_(new OverlayItem) {
    setScene(scene_);
    setBackgroundBrush(Qt::black);
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scene_->addItem(pixItem_);
    scene_->addItem(overlay_);
    updateSceneRect();
}

void ImageViewWithOverlays::setFitToWindow(bool on){ fitToWindow_ = on; if (on) fitInViewImage(); }
void ImageViewWithOverlays::setFovDegrees(double hfovDeg, double vfovDeg){ HFOV_ = qDegreesToRadians(hfovDeg); VFOV_ = qDegreesToRadians(vfovDeg); updateTargetBox_(); }
void ImageViewWithOverlays::setBoardSizeMeters(double w, double h){ boardWm_ = w; boardHm_ = h; updateTargetBox_(); }
void ImageViewWithOverlays::setTargetDistance(double zMeters){ targetZ_ = zMeters; updateTargetBox_(); }
void ImageViewWithOverlays::setTolerances(double sizeRelTol, double centerTolRatio){ sizeTol_ = sizeRelTol; centerTolRatio_ = centerTolRatio; updateFitState_(); }

void ImageViewWithOverlays::setImage(const QImage &img){
    if (img.isNull()) return;
    currentImg_ = img;
    pixItem_->setPixmap(QPixmap::fromImage(currentImg_));
    updateSceneRect(); updateTargetBox_();
    if (fitToWindow_) fitInViewImage();
}
void ImageViewWithOverlays::setImage(const cv::Mat &mat){ setImage(ImageUtils::matToQImage(mat)); }

void ImageViewWithOverlays::setLanePolylines(const QVector<QPolygonF> &lanes){ overlay_->setLanes(lanes); }
void ImageViewWithOverlays::setObstacles(const QVector<ObstacleBox> &obs){ overlay_->setObstacles(obs); }
void ImageViewWithOverlays::setChessboardBoundingRect(const QRectF &r){ chessRect_ = r; overlay_->setChessboardRect(r); updateFitState_(); }
void ImageViewWithOverlays::setChessboardCorners(const QVector<QPointF> &pts){ chessRect_ = pts.isEmpty()? QRectF() : QPolygonF(pts).boundingRect(); overlay_->setChessboardCorners(pts); updateFitState_(); }

QPointF ImageViewWithOverlays::mapToImage(const QPoint &viewPt) const { return mapToScene(viewPt); }

void ImageViewWithOverlays::resizeEvent(QResizeEvent *e){
    QGraphicsView::resizeEvent(e);
    if (fitToWindow_) fitInViewImage();
}

void ImageViewWithOverlays::wheelEvent(QWheelEvent *e){
    if (fitToWindow_) { fitToWindow_ = false; }
    const double steps = e->angleDelta().y() / 120.0;
    const double factor = std::pow(1.15, (e->modifiers() & Qt::ControlModifier) ? steps*0.5 : steps);
    zoomBy_(factor);
    e->accept();
}
void ImageViewWithOverlays::mousePressEvent(QMouseEvent *e){
    if (e->button() == Qt::MiddleButton || (e->button()==Qt::LeftButton && (e->modifiers() & Qt::ControlModifier || panWithSpace_))) {
        lastPan_ = e->pos(); panning_ = true; setCursor(Qt::ClosedHandCursor); e->accept(); return; }
    QGraphicsView::mousePressEvent(e);
}
void ImageViewWithOverlays::mouseMoveEvent(QMouseEvent *e){
    if (panning_) { QPointF delta = mapToScene(lastPan_) - mapToScene(e->pos()); translate(delta.x(), delta.y()); lastPan_ = e->pos(); e->accept(); return; }
    QGraphicsView::mouseMoveEvent(e);
}
void ImageViewWithOverlays::mouseReleaseEvent(QMouseEvent *e){
    if (panning_ && (e->button()==Qt::MiddleButton || e->button()==Qt::LeftButton)) { panning_ = false; setCursor(Qt::ArrowCursor); e->accept(); return; }
    QGraphicsView::mouseReleaseEvent(e);
}
void ImageViewWithOverlays::mouseDoubleClickEvent(QMouseEvent *e){ Q_UNUSED(e); fitToWindow_ = true; fitInViewImage(); }
void ImageViewWithOverlays::keyPressEvent(QKeyEvent *e){
    switch (e->key()) {
    case Qt::Key_Plus: case Qt::Key_Equal: zoomBy_(1.15); break;
    case Qt::Key_Minus: zoomBy_(1.0/1.15); break;
    case Qt::Key_1: setZoom_(1.0); centerOn(pixItem_); break;
    case Qt::Key_0: resetView_(); break;
    case Qt::Key_F: fitToWindow_ = true; fitInViewImage(); break;
    case Qt::Key_Space: panWithSpace_ = true; setCursor(Qt::OpenHandCursor); break;
    default: QGraphicsView::keyPressEvent(e); break;
    }
}
void ImageViewWithOverlays::keyReleaseEvent(QKeyEvent *e){
    if (e->key() == Qt::Key_Space) { panWithSpace_ = false; setCursor(Qt::ArrowCursor); }
    QGraphicsView::keyReleaseEvent(e);
}

void ImageViewWithOverlays::updateSceneRect(){
    const QRectF r(0,0,currentImg_.width(), currentImg_.height());
    scene_->setSceneRect(r);
    overlay_->setImageRect(r);
}
void ImageViewWithOverlays::fitInViewImage(){
    if (currentImg_.isNull()) return;
    const QRectF r = pixItem_->boundingRect();
    fitInView(r, Qt::KeepAspectRatio);
    emit zoomChanged(transform().m11());
}
void ImageViewWithOverlays::resetView_(){ setTransform(QTransform()); centerOn(pixItem_); emit zoomChanged(1.0); }
void ImageViewWithOverlays::setZoom_(double z){ QTransform t; t.scale(z, z); setTransform(t); emit zoomChanged(z); }
void ImageViewWithOverlays::zoomBy_(double factor){ const double cur = transform().m11(); double z = qBound(0.05, cur*factor, 40.0); setZoom_(z); }

QSizeF ImageViewWithOverlays::expectedSizePx_(double Z) const {
    if (HFOV_<=0 || VFOV_<=0 || currentImg_.isNull()) return {};
    const double angW = 2.0 * std::atan(boardWm_/(2.0*Z));
    const double angH = 2.0 * std::atan(boardHm_/(2.0*Z));
    const double wpx = currentImg_.width()  * (angW / HFOV_);
    const double hpx = currentImg_.height() * (angH / VFOV_);
    return QSizeF(wpx, hpx);
}
void ImageViewWithOverlays::updateTargetBox_(){
    const QSizeF sz = expectedSizePx_(targetZ_);
    overlay_->setTargetBox(sz);
    updateFitState_();
}
void ImageViewWithOverlays::updateFitState_(){
    bool ok = false;
    if (chessRect_.isValid() && !currentImg_.isNull()) {
        const QSizeF target = expectedSizePx_(targetZ_);
        if (target.isValid()) {
            const double wErr = std::abs(chessRect_.width()  - target.width())  / qMax(1.0, target.width());
            const double hErr = std::abs(chessRect_.height() - target.height()) / qMax(1.0, target.height());
            const QPointF c1 = chessRect_.center();
            const QPointF c2 = QRectF(QPointF(0,0), currentImg_.size()).center();
            const double centerTol = centerTolRatio_ * qMin(currentImg_.width(), currentImg_.height());
            const double cdist = std::hypot(c1.x()-c2.x(), c1.y()-c2.y());
            ok = (qMax(wErr, hErr) <= sizeTol_) && (cdist <= centerTol);
        }
    }
    overlay_->setFitOk(ok);
}
