#include "CalibViewWidget.h"
#include <QPainter>

QImage CalibViewWidget::matToQImage(const cv::Mat& m) {
	if (m.empty()) return {};
	if (m.type() == CV_8UC1) {
		QImage img(m.data, m.cols, m.rows, m.step, QImage::Format_Grayscale8);
		return img.copy();
	}
	else if (m.type() == CV_8UC3) {
		cv::Mat rgb; cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
		QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
		return img.copy();
	}
	else {
		cv::Mat bgr8; m.convertTo(bgr8, CV_8UC3);
		cv::Mat rgb;  cv::cvtColor(bgr8, rgb, cv::COLOR_BGR2RGB);
		QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
		return img.copy();
	}
}

CalibViewWidget::CalibViewWidget(QWidget* parent) : QWidget(parent) {
	connect(&calib_, &TargetLightCalibrator::recognitionStateChanged,
		this, &CalibViewWidget::recognitionStateChangedUI);
}

void CalibViewWidget::ingestFrame(const cv::Mat& frameBgr) {
	if (frameBgr.empty()) return;
	cv::Mat vis = calib_.processFrame(frameBgr);
	QImage img = matToQImage(vis);
	{
		QMutexLocker lk(&mtx_);
		lastImg_ = std::move(img);
		lastSize_ = QSize(vis.cols, vis.rows);
	}
	update();
}

void CalibViewWidget::paintEvent(QPaintEvent*) {
	QPainter p(this); p.fillRect(rect(), Qt::black);
	QImage img; QSize sz;
	{ QMutexLocker lk(&mtx_); img = lastImg_; sz = lastSize_; }
	if (img.isNull()) return;

	QSize scaled = sz; scaled.scale(size(), Qt::KeepAspectRatio);
	QRect dst((width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
		scaled.width(), scaled.height());
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	p.drawImage(dst, img);
}
