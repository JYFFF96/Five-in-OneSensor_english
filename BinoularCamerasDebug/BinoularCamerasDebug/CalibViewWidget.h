#pragma once
#include <QWidget>
#include <QMutex>
#include "TargetLightCalibrator.h"

class CalibViewWidget : public QWidget {
	Q_OBJECT
public:
	explicit CalibViewWidget(QWidget* parent = nullptr);

	// Í¸´«
	void setFovDegrees(double a, double b) { calib_.setFovDegrees(a, b); }
	void setBoardSizeMeters(double w, double h) { calib_.setBoardSizeMeters(w, h); }
	void setDistanceMeters(double z) { calib_.setDistanceMeters(z); }
	void setTolerances(double cpx, double srel) { calib_.setTolerances(cpx, srel); }
	void setUseChessArea(bool on) { calib_.setUseChessArea(on); }
	void setUseChessboardArea(bool on) { calib_.setUseChessboardArea(on); }

public slots:
	void ingestFrame(const cv::Mat& frameBgr);

signals:
	void recognitionStateChangedUI(bool ok);

protected:
	void paintEvent(QPaintEvent*) override;
	QSize sizeHint() const override { return QSize(960, 540); }

private:
	static QImage matToQImage(const cv::Mat& m);

	TargetLightCalibrator calib_;
	QMutex mtx_;
	QImage lastImg_;
	QSize  lastSize_;
};
