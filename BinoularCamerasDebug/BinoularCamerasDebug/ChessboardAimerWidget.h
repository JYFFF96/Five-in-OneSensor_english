#pragma once
#include <QWidget>
#include <QImage>
#include <QRectF>
#include <QVector>
#include <QPointF>
#include <QtMath>      // for qDegreesToRadians
#include <cmath>       // for std::atan
#include <opencv2/opencv.hpp>

class ChessboardAimerWidget : public QWidget {
	Q_OBJECT
public:
	explicit ChessboardAimerWidget(QWidget *parent = nullptr);

	// 图像输入（支持 QImage 或 OpenCV Mat）
	void setImage(const QImage &img);
	void setImage(const cv::Mat &mat);

	// 相机/棋盘参数（用于按距离计算期望像素尺寸）
	void setFovDegrees(double hfovDeg, double vfovDeg);   // 例如 82, 44
	void setBoardSizeMeters(double w, double h);          // 例如 0.90, 0.40

	// 多距离目标与匹配策略
	void setTargetDistances(const QVector<double> &zs);   // 例如 {8,12,16,20}
	void setActiveTargetIndex(int i);                     // 关闭自动匹配，手动指定
	void setAutoTargeting(bool on);                       // 开启自动匹配（默认开）
	void setDrawAllTargets(bool on);                      // 是否把其他距离的框也画出来（灰色虚线）

	// 判定容差（默认：尺寸15%，中心偏差=短边的3%）
	void setTolerances(double sizeRelTol, double centerTolRatio);

	// 棋盘检测输入（图像坐标）。你后面把 findChessboardCorners 的包围框喂这里即可
	void setChessboardBoundingRect(const QRectF &r);
	void setChessboardCorners(const QVector<QPointF> &pts); // 也可直接给角点

	// 仅用一个距离做一次识别/显示（会关闭自动匹配与多框显示）
	void setTargetDistance(double zMeters);

	// 入参只有距离：返回是否贴合；可选返回尺寸相对误差与中心像素偏差
	bool recognizeAtDistance(double zMeters, double* sizeErr = nullptr, double* centerErrPx = nullptr);

signals:
	// 每次绘制时发一次：当前使用的目标距离、索引、是否贴合、尺寸误差
	void targetMatched(double zMeters, int index, bool ok, double sizeErr);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	// 计算给定距离 Z 时棋盘在图像中的期望像素尺寸（基于 FOV）
	QSizeF expectedSizePx_(double Z) const;

	struct MatchResult {
		int index = -1; double z = 0; double sizeErr = 1e9; bool ok = false; QRectF targetRectImg;
	};
	// 计算当前“活动目标”（自动或手动）；并判定与检测框是否贴合
	MatchResult computeMatch_() const;

	// OpenCV Mat -> QImage（Qt 5.12 兼容）
	static QImage matToQImageCompat_(const cv::Mat &m);

private:
	// 数据
	QImage currentImg_;
	QRectF chessRect_;  bool haveChess_ = false;

	// 相机/棋盘参数
	double HFOV_ = qDegreesToRadians(82.0);
	double VFOV_ = qDegreesToRadians(44.0);
	double boardWm_ = 0.90, boardHm_ = 0.40;

	// 目标距离与绘制
	QVector<double> targetsZ_{ 10.0, 12.0, 16.0, 20.0 };
	int  activeTargetIndex_ = 0;
	bool autoTarget_ = true;
	bool drawAllTargets_ = true;

	// 判定容差
	double sizeTol_ = 0.15;        // 15%
	double centerTolRatio_ = 0.03; // 图像短边的 3%
};
