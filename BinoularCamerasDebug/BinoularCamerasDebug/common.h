#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <cmath>
#include <opencv2/video/tracking.hpp>
#include "stereocamera.h"
#include "calibrationparams.h"
#include "rotationmatrix.h"
#include <QToolButton>
#include <QAbstractButton>
#include <QPixmap>
#include <QEvent>
#include <QSizePolicy>
#include "obstacleData.h"

enum class ObstacleType { Pedestrian, Cyclist, Vehicle, Other };

struct ObstacleDet {
	int id;
	ObstacleType  type;
	QRectF bbox;          // 像素方框(基于 sourceSize)
	float x_m;            // 横向距离
	float z_m;            // 纵向距离
	float vx_mps;         // X 方向相对速度 (m/s)
	float vz_mps;         // Z 方向相对速度 (m/s)
	float hmw_s;          // 车头时距 (s)
	float ttc_s;          // TTC (s)
};

// RecognitionType -> ObstacleType
static ObstacleType toObstacleType(RecognitionType r) {
	switch (r) {
	case RecognitionType::PEDESTRIAN: return ObstacleType::Pedestrian;
	case RecognitionType::BICYCLE:    return ObstacleType::Cyclist;
	case RecognitionType::VEHICLE:
	case RecognitionType::TRUCK:
	case RecognitionType::BUS:
	case RecognitionType::MOTO:       return ObstacleType::Vehicle;
	default:                          return ObstacleType::Other;
	}
}
static inline QColor colorForType(ObstacleType t) {
	switch (t) {
	case ObstacleType::Pedestrian: return QColor(255, 0, 255); // 行人：洋红
	case ObstacleType::Cyclist:    return QColor(255, 165, 0); // 骑行者：橙色
	case ObstacleType::Vehicle:    return QColor(0, 255, 255); // 机动车辆：青蓝
	case ObstacleType::Other:      return QColor(0, 0, 255); // 其他类：蓝色（按你图例）
	}
	return QColor(0, 255, 0); // never
}
enum ShowCamrea {
	LEFT,
	RIGHT,
};

struct CameraInfo {
	float cameraHeight;       //(m) 相机安装高度（离地）
	float cameraLeftDistance; //(m) 相机中心到车辆左侧的横向距离
	float cameraRightDistance;//(m) 相机中心到车辆右侧的横向距离
	float cameraDepth;        //(m) 相机中心到车头的纵向距离
	float carWidth;           //(m) 车辆宽度
	float carHeadHeight;      //(m) 车头高度
};

extern CameraInfo g_cameraInfoParams;
extern StereoCalibrationParameters g_cameraBiaodingParams; // 相机内参 fov啥的
extern RotationMatrix g_rotationMatrix;

class FillIconSize : public QObject {
public:
	explicit FillIconSize(QAbstractButton* b) : QObject(b), btn(b) {
		btn->installEventFilter(this);
		adjust();
	}
private:
	QAbstractButton* btn;
	void adjust() {
		const QSize s = btn->contentsRect().size();
		if (!s.isEmpty()) btn->setIconSize(s); // 关键：图标尺寸 = 按钮内容区
	}
	bool eventFilter(QObject* o, QEvent* e) override {
		if (o == btn && (e->type() == QEvent::Resize || e->type() == QEvent::Show))
			adjust();
		return QObject::eventFilter(o, e);
	}
};

class common
{
};

void drawGroundRegion(cv::Mat& frame,
	const RotationMatrix& rotation,
	const CameraInfo& camInfo,
	float Znear_m, float Zfar_m,
	float Wnear_m, float Wfar_m,
	int worldYMode = 0, bool drawOutline = true, double alpha = 0.35);

void drawGroundRegionByRows(
	cv::Mat& frame, const RotationMatrix& rotation, const CameraInfo& camInfo,
	int y_top, int y_bottom,  // 目标像素行，例如 y_top=411（你工具里显示的值），y_bottom=frame.rows-20
	float Wtop_m, float Wbottom_m,
	bool drawOutline = true, double alpha = 0.35);

void drawLaneLines(cv::Mat& frame,
	const RotationMatrix& rotation,
	const CameraInfo& camInfo,
	float maxDist = 30.0f,   // 画多远
	bool useCarWidth = true); // true=用车辆宽度, false=用车道宽

static cv::Mat makeTemplate(int W = 400, int H = 900) {
	cv::Mat templ(H, W, CV_8UC1, cv::Scalar(255));
	int bw = std::round(W * 3.0 / 40.0); // 3cm/40cm
	cv::rectangle(templ, cv::Rect(0, 0, bw, H), cv::Scalar(180), CV_FILLED);
	cv::rectangle(templ, cv::Rect(W - bw, 0, bw, H), cv::Scalar(180), CV_FILLED);

	int cellW = std::round(W * 10.0 / 40.0);
	int cellH = std::round(H * 10.0 / 90.0);
	int usedH = 8 * cellH, topPad = (H - usedH) / 2;
	int usedW = 2 * cellW;
	int sidePad = std::round((W - 2 * bw - usedW) / 3.0);
	int x0 = bw + sidePad;
	int x1 = x0 + cellW + sidePad;

	for (int r = 0; r < 8; ++r) {
		int y = topPad + r * cellH;
		cv::rectangle(templ, cv::Rect(x0, y, cellW, cellH), cv::Scalar(0), CV_FILLED);
		cv::rectangle(templ, cv::Rect(x1, y, cellW, cellH), cv::Scalar(0), CV_FILLED);
	}
	return templ;
}

struct FitReport {
	std::vector<cv::Point2f> quad; // 检测到的靶标四角
	double angleDiffDeg = 180.0, scaleRatio = 0, centerNorm = 1e9, iou = 0, eccScore = -1;
	bool ok = false;
};

static double polygonArea(const std::vector<cv::Point2f>& p) {
	double a = 0; int n = (int)p.size();
	for (int i = 0; i < n; i++) { const auto& A = p[i]; const auto& B = p[(i + 1) % n]; a += (double)A.x*B.y - (double)A.y*B.x; }
	return std::abs(a)*0.5;
}
static double iouQuadVsRect(const std::vector<cv::Point2f>& quad, const cv::Rect& R) {
	std::vector<cv::Point2f> rect = {
		{ (float)R.x, (float)R.y },
		{ (float)(R.x + R.width), (float)R.y },
		{ (float)(R.x + R.width), (float)(R.y + R.height) },
		{ (float)R.x, (float)(R.y + R.height) }
	};
	std::vector<cv::Point2f> inter;
	if (cv::intersectConvexConvex(quad, rect, inter) == 0) return 0.0;
	double aI = polygonArea(inter);
	double aU = polygonArea(quad) + polygonArea(rect) - aI;
	return (aU > 0) ? aI / aU : 0.0;
}
static double angleOfQuadLongAxis(const std::vector<cv::Point2f>& q) {
	auto len2 = [&](cv::Point2f a, cv::Point2f b) {cv::Point2f d = b - a; return d.dot(d); };
	double l01 = len2(q[0], q[1]), l12 = len2(q[1], q[2]), l23 = len2(q[2], q[3]), l30 = len2(q[3], q[0]);
	cv::Point2f dir;
	if (std::max(l01, l23) >= std::max(l12, l30)) dir = q[1] - q[0];
	else dir = q[2] - q[1];
	return std::atan2(dir.y, dir.x) * 180.0 / CV_PI;
}

// 传入 BGR 图和红框，返回检测报告
inline FitReport checkBoardInBox_ECC(const cv::Mat& imgBGR, const cv::Rect& redBox) {
	FitReport rep;
	if (imgBGR.empty() || redBox.width <= 0 || redBox.height <= 0) return rep;

	cv::Mat gray; cv::cvtColor(imgBGR, gray, cv::COLOR_BGR2GRAY);
	cv::Mat templ = makeTemplate(400, 900);

	// ROI 扩 10%
	cv::Rect roi = redBox;
	int dx = std::max(1, (int)std::round(roi.width*0.10));
	int dy = std::max(1, (int)std::round(roi.height*0.10));
	roi.x = std::max(0, roi.x - dx);
	roi.y = std::max(0, roi.y - dy);
	roi.width = std::min(imgBGR.cols - roi.x, redBox.width + 2 * dx);
	roi.height = std::min(imgBGR.rows - roi.y, redBox.height + 2 * dy);

	cv::Mat patch = gray(roi).clone();
	cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
	clahe->apply(patch, patch);

	// ECC 同尺寸
	cv::Mat patchResized; cv::resize(patch, patchResized, templ.size());
	cv::Mat warp = cv::Mat::eye(3, 3, CV_32F);
	double ecc = -1;
	try {
		ecc = cv::findTransformECC(
			templ,                 // 模板（灰度）
			patchResized,          // ROI（灰度，已 resize 到和模板同尺寸）
			warp,                  // 3x3
			cv::MOTION_HOMOGRAPHY,
			cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 100, 1e-5)
		);
	}
	catch (...) { ecc = -1; }

	rep.eccScore = ecc;
	if (ecc < 0.30) { rep.ok = false; return rep; } // 配准质量太差

	// 投影模板四角 → 回到整幅图坐标
	std::vector<cv::Point2f> templQuad = { {0,0},{(float)templ.cols,0},{(float)templ.cols,(float)templ.rows},{0,(float)templ.rows} };
	std::vector<cv::Point2f> quadROI;
	cv::perspectiveTransform(templQuad, quadROI, warp);
	float sx = (float)roi.width / templ.cols;
	float sy = (float)roi.height / templ.rows;
	rep.quad.reserve(4);
	for (auto p : quadROI) {
		p.x = p.x * sx + roi.x;
		p.y = p.y * sy + roi.y;
		rep.quad.push_back(p);
	}

	// 几何比对（阈值可按现场调）
	double angQuad = angleOfQuadLongAxis(rep.quad);
	// 若红框是竖直的，参照角设 90°；如果你的红框是横向，设 0°
	double angBox = 90.0;
	double dAng = std::fabs(std::remainder(angQuad - angBox, 180.0));
	rep.angleDiffDeg = std::min(dAng, 180.0 - dAng);

	// 高度比做尺度
	double hQuad = std::hypot(rep.quad[2].x - rep.quad[1].x, rep.quad[2].y - rep.quad[1].y);
	double hBox = redBox.height;
	rep.scaleRatio = (hBox > 1 ? hQuad / hBox : 0);

	auto center = [](const std::vector<cv::Point2f>& q) {
		cv::Point2f c(0, 0); for (auto&p : q) c += p; c.x /= q.size(); c.y /= q.size(); return c; };
	cv::Point2f cq = center(rep.quad);
	cv::Point2f cb(redBox.x + redBox.width / 2.f, redBox.y + redBox.height / 2.f);
	double diag = std::hypot(redBox.width, redBox.height);
	rep.centerNorm = (diag > 1 ? cv::norm(cq - cb) / diag : 1e9);

	rep.iou = iouQuadVsRect(rep.quad, redBox);

	rep.ok = (rep.angleDiffDeg <= 8.0) &&
		(rep.scaleRatio >= 0.80 && rep.scaleRatio <= 1.25) &&
		(rep.centerNorm <= 0.15) &&
		(rep.iou >= 0.60);
	return rep;
}
// FitIconFilter.h
#include <QObject>
#include <QAbstractButton>
#include <QEvent>

class FitIconFilter : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
protected:
	bool eventFilter(QObject* obj, QEvent* ev) override {
		if (auto b = qobject_cast<QAbstractButton*>(obj)) {
			switch (ev->type()) {
			case QEvent::Show:
			case QEvent::Resize:
			case QEvent::ScreenChangeInternal:
			case QEvent::StyleChange: {
				// 用内容区域大小作为 iconSize，QIcon 会按比例等比缩放并居中
				const QSize s = b->contentsRect().size();
				b->setIconSize(s);
				break;
			}
			default: break;
			}
		}
		return QObject::eventFilter(obj, ev);
	}
};
