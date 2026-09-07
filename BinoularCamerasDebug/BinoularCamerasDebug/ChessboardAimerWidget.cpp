// ChessboardAimerWidget.cpp
#include "ChessboardAimerWidget.h"
#include <QPainter>

ChessboardAimerWidget::ChessboardAimerWidget(QWidget *parent) : QWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMinimumSize(480, 270);
}

// ---------- public 接口 ----------
void ChessboardAimerWidget::setImage(const QImage &img) { currentImg_ = img; update(); }

QImage ChessboardAimerWidget::matToQImageCompat_(const cv::Mat &m) {
	if (m.empty()) return QImage();
	if (m.type() == CV_8UC3) {
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
		QImage img(m.data, m.cols, m.rows, m.step, QImage::Format_BGR888);
		return img.copy();
#else
		cv::Mat rgb; cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
		QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
		return img.copy();
#endif
	}
	else if (m.type() == CV_8UC1) {
		QImage img(m.data, m.cols, m.rows, m.step, QImage::Format_Grayscale8);
		return img.copy();
	}
	else {
		cv::Mat tmp; m.convertTo(tmp, CV_8U);
		QImage img(tmp.data, tmp.cols, tmp.rows, tmp.step, QImage::Format_Grayscale8);
		return img.copy();
	}
}
void ChessboardAimerWidget::setImage(const cv::Mat &mat) { setImage(matToQImageCompat_(mat)); }

void ChessboardAimerWidget::setFovDegrees(double hfovDeg, double vfovDeg) { HFOV_ = qDegreesToRadians(hfovDeg); VFOV_ = qDegreesToRadians(vfovDeg); update(); }
void ChessboardAimerWidget::setBoardSizeMeters(double w, double h) { boardWm_ = w; boardHm_ = h; update(); }
void ChessboardAimerWidget::setTargetDistances(const QVector<double> &zs) { targetsZ_ = zs.isEmpty() ? QVector<double>{8, 12, 16, 20} : zs; update(); }
void ChessboardAimerWidget::setActiveTargetIndex(int i) { autoTarget_ = false; activeTargetIndex_ = qBound(0, i, targetsZ_.size() - 1); update(); }
void ChessboardAimerWidget::setAutoTargeting(bool on) { autoTarget_ = on; update(); }
void ChessboardAimerWidget::setDrawAllTargets(bool on) { drawAllTargets_ = on; update(); }
void ChessboardAimerWidget::setTolerances(double sizeRelTol, double centerTolRatio) { sizeTol_ = sizeRelTol; centerTolRatio_ = centerTolRatio; update(); }
void ChessboardAimerWidget::setChessboardBoundingRect(const QRectF &r) { chessRect_ = r; haveChess_ = r.isValid(); update(); }
void ChessboardAimerWidget::setChessboardCorners(const QVector<QPointF> &pts) { chessRect_ = pts.isEmpty() ? QRectF() : QPolygonF(pts).boundingRect(); haveChess_ = !pts.isEmpty(); update(); }

void ChessboardAimerWidget::setTargetDistance(double zMeters)
{
	// 只保留这一个目标；不画其它距离；关闭自动匹配
	targetsZ_.clear();
	targetsZ_.push_back(zMeters);
	autoTarget_ = false;
	activeTargetIndex_ = 0;
	drawAllTargets_ = false;
	update();
}

bool ChessboardAimerWidget::recognizeAtDistance(double zMeters, double * sizeErrOut, double * centerErrPxOut)
{
	// 设置为“单目标”并计算判定
	setTargetDistance(zMeters);

	// 若没有图像，直接返回不通过
	if (currentImg_.isNull()) {
		if (sizeErrOut) *sizeErrOut = 1e9;
		if (centerErrPxOut) *centerErrPxOut = 1e9;
		emit targetMatched(zMeters, 0, false, 1e9);
		return false;
	}

	const QSizeF imgSize(currentImg_.width(), currentImg_.height());
	const QPointF imgCenter(imgSize.width() / 2.0, imgSize.height() / 2.0);

	// 期望像素尺寸与目标矩形（图像坐标）
	const QSizeF tp = expectedSizePx_(zMeters);
	QRectF tr(imgCenter.x() - tp.width() / 2.0, imgCenter.y() - tp.height() / 2.0,
		tp.width(), tp.height());

	// 计算误差
	double sizeErr = 1e9;
	double centerErr = 1e9;
	bool ok = false;

	if (haveChess_ && chessRect_.isValid() && tp.isValid()) {
		const double wErr = std::abs(chessRect_.width() - tp.width()) / std::max(1.0, tp.width());
		const double hErr = std::abs(chessRect_.height() - tp.height()) / std::max(1.0, tp.height());
		sizeErr = std::max(wErr, hErr);

		const double centerTolPx = centerTolRatio_ * std::min(currentImg_.width(), currentImg_.height());
		centerErr = std::hypot(chessRect_.center().x() - imgCenter.x(),
			chessRect_.center().y() - imgCenter.y());

		ok = (sizeErr <= sizeTol_) && (centerErr <= centerTolPx);
	}

	// 触发重绘（十字/方框会按结果红/绿显示）
	update();

	if (sizeErrOut) *sizeErrOut = sizeErr;
	if (centerErrPxOut) *centerErrPxOut = centerErr;
	emit targetMatched(zMeters, 0, ok, sizeErr);
	return ok;
}

// ---------- 绘制 ----------
void ChessboardAimerWidget::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.fillRect(rect(), Qt::black);
	if (currentImg_.isNull()) return;

	// 1) 让图像按窗口尺寸等比铺放
	const QSizeF imgSize(currentImg_.width(), currentImg_.height());
	const QSizeF wndSize(width(), height());
	const double s = qMin(wndSize.width() / imgSize.width(), wndSize.height() / imgSize.height());
	const QSizeF drawSize(imgSize.width()*s, imgSize.height()*s);
	const QPointF topLeft((wndSize.width() - drawSize.width()) / 2.0, (wndSize.height() - drawSize.height()) / 2.0);
	const QRectF imgRectOnWidget(topLeft, drawSize);
	p.drawImage(imgRectOnWidget, currentImg_);

	// 2) 选择活动目标（自动或手动），并给出期望矩形
	const MatchResult mr = computeMatch_();

	// 3) 坐标映射：图像像素 -> 窗口坐标
	auto mapPt = [&](const QPointF &pt) { return QPointF(topLeft.x() + pt.x()*s, topLeft.y() + pt.y()*s); };
	auto mapRect = [&](const QRectF &r) { return QRectF(mapPt(r.topLeft()), QSizeF(r.width()*s, r.height()*s)); };

	// 4) 画十字线（穿过图像中心），与目标方框
	const QPointF imgCenter(imgSize.width() / 2.0, imgSize.height() / 2.0);
	const QColor main = mr.ok ? QColor(0, 200, 0) : QColor(220, 0, 0);
	QPen mainPen(main, 2.0); mainPen.setCosmetic(true); p.setPen(mainPen);

	const QPointF cW = mapPt(imgCenter);
	p.drawLine(QPointF(imgRectOnWidget.left(), cW.y()), QPointF(imgRectOnWidget.right(), cW.y()));
	p.drawLine(QPointF(cW.x(), imgRectOnWidget.top()), QPointF(cW.x(), imgRectOnWidget.bottom()));
	p.drawRect(mapRect(mr.targetRectImg));

	// 5)（可选）画其他距离的参照框（灰色虚线）
	if (drawAllTargets_ && targetsZ_.size() > 1) {
		QPen gp(QColor(180, 180, 180), 1.0, Qt::DashLine); gp.setCosmetic(true);
		p.setPen(gp);
		for (int i = 0; i < targetsZ_.size(); ++i) if (i != mr.index) {
			const QSizeF sz = expectedSizePx_(targetsZ_[i]);
			QRectF r(imgCenter.x() - sz.width() / 2.0, imgCenter.y() - sz.height() / 2.0, sz.width(), sz.height());
			p.drawRect(mapRect(r));
		}
		p.setPen(mainPen);
	}

	// 6) 调试：画检测到的棋盘包围框（黄虚线）
	if (haveChess_) {
		QPen det(Qt::yellow, 1.0, Qt::DashLine); det.setCosmetic(true);
		p.setPen(det); p.drawRect(mapRect(chessRect_));
		p.setPen(mainPen);
	}

	emit targetMatched(mr.z, mr.index, mr.ok, mr.sizeErr);
}

// ---------- 私有：计算 ----------
QSizeF ChessboardAimerWidget::expectedSizePx_(double Z) const {
	if (HFOV_ <= 0 || VFOV_ <= 0 || currentImg_.isNull()) return {};
	const double angW = 2.0 * std::atan(boardWm_ / (2.0*Z));
	const double angH = 2.0 * std::atan(boardHm_ / (2.0*Z));
	const double wpx = currentImg_.width()  * (angW / HFOV_);
	const double hpx = currentImg_.height() * (angH / VFOV_);
	return QSizeF(wpx, hpx);
}

ChessboardAimerWidget::MatchResult ChessboardAimerWidget::computeMatch_() const {
	MatchResult mr; if (currentImg_.isNull()) return mr;

	const QSizeF imgSize(currentImg_.width(), currentImg_.height());
	const QPointF imgCenter(imgSize.width() / 2.0, imgSize.height() / 2.0);

	auto evalIdx = [&](int i) {
		const double Z = targetsZ_.value(i, 8.0);
		const QSizeF tp = expectedSizePx_(Z);
		QRectF tr(imgCenter.x() - tp.width() / 2.0, imgCenter.y() - tp.height() / 2.0, tp.width(), tp.height());
		double err = 1e9; bool ok = false;
		if (haveChess_ && chessRect_.isValid() && tp.isValid()) {
			const double wErr = std::abs(chessRect_.width() - tp.width()) / qMax(1.0, tp.width());
			const double hErr = std::abs(chessRect_.height() - tp.height()) / qMax(1.0, tp.height());
			const double centerTol = centerTolRatio_ * qMin(currentImg_.width(), currentImg_.height());
			const double cdist = std::hypot(chessRect_.center().x() - imgCenter.x(), chessRect_.center().y() - imgCenter.y());
			err = std::max(wErr, hErr);
			ok = (err <= sizeTol_) && (cdist <= centerTol);
		}
		MatchResult r; r.index = i; r.z = Z; r.sizeErr = err; r.ok = ok; r.targetRectImg = tr; return r;
	};

	if (!autoTarget_) return evalIdx(qBound(0, activeTargetIndex_, targetsZ_.size() - 1));

	// 自动模式：找“尺寸误差”最小的那一个目标距离
	MatchResult best; double bestErr = 1e9; int bestIdx = 0;
	for (int i = 0; i < targetsZ_.size(); ++i) {
		MatchResult r = evalIdx(i);
		if (r.sizeErr < bestErr) { bestErr = r.sizeErr; bestIdx = i; best = r; }
	}
	return (best.index == -1) ? evalIdx(qBound(0, activeTargetIndex_, targetsZ_.size() - 1)) : best;
}
