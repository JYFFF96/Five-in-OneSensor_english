#pragma once
#include <QObject>
#include <opencv2/opencv.hpp>

class TargetLightCalibrator : public QObject {
	Q_OBJECT
public:
	explicit TargetLightCalibrator(QObject* parent = nullptr) : QObject(parent) {}

	// 参数配置
	void setFovDegrees(double hfov_deg, double vfov_deg) { hfov = hfov_deg; vfov = vfov_deg; }
	void setBoardSizeMeters(double w, double h) { boardW = w; boardH = h; }  // 默认 0.40 x 0.90
	void setDistanceMeters(double z) { Z = z; }                              // 任意距离（米）
	void setTolerances(double center_px, double size_rel) { centerTolPx = center_px; sizeTolRel = size_rel; }
	void setUseChessArea(bool on) { useChessArea = on; }                     // 仅影响初始红框选择
	void setUseChessboardArea(bool on) { setUseChessArea(on); }              // 别名

	// 主入口：输入一帧（BGR/GRAY），返回叠加可视化后的 BGR
	cv::Mat processFrame(const cv::Mat& bgrOrGray);

signals:
	void recognitionStateChanged(bool ok); // 仅在状态改变时触发

private:
	static inline double deg2rad(double d) { return d * CV_PI / 180.0; }
	static cv::Rect2f scaleAroundCenter(const cv::Rect2f& r, float s);
	static void orderQuad(std::vector<cv::Point2f>& q);

	// 根据 FOV/Z 计算“以图像中心为中心”的期望矩形（像素）
	cv::Rect2f expectedRectPx(const cv::Size& sz, bool chess) const;

	// ROI 放大倍数自适应（期望框覆盖率越小 → 放大越多）
	static float roiScaleFor(const cv::Rect2f& expR, const cv::Size& imsz);

	// 检测 2×8 棋盘靶（含“外框精化”），成功返回 true，并给出 rr/quad
	bool detectTargetBoard2x8(const cv::Mat& gray, cv::RotatedRect& rr,
		std::vector<cv::Point2f>& quad) const;

private:
	// 运行参数
	double hfov = 38.0, vfov = 21.0;    // 视场（度）
	double boardW = 0.40, boardH = 0.90;// 整板尺寸（米）
	double Z = 8.0;                     // 距离（米）
	bool   useChessArea = false;        // 初始红框用整板 or 棋盘
	double centerTolPx = 40.0;          // 中心容差（像素）
	double sizeTolRel = 0.30;          // 尺寸相对容差（±比例）

	// 运行状态
	bool lastOk = false;
};
