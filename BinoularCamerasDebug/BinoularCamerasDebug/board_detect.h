#include <opencv2/opencv.hpp>

// === 参数：按你的实际情况修改/传入 ===
struct CameraSpec {
	int imgW = 1280, imgH = 720;
	double HFOV_deg = 38.0, VFOV_deg = 21.0;
};

struct BoardSpec {
	// 仅检测棋盘本体（黑白格）20cm×80cm；两侧 3cm 灰边这里不参与匹配
	double physW_m = 0.20; // 宽 0.20 m
	double physH_m = 0.80; // 高 0.80 m
	int cols = 2;          // 棋盘格列数
	int rows = 8;          // 棋盘格行数
};

static inline double deg2rad(double d) { return d * CV_PI / 180.0; }

// 计算 fx/fy（像素）
static inline void focalInPixels(const CameraSpec& cam, double& fx, double& fy) {
	fx = (cam.imgW * 0.5) / std::tan(deg2rad(cam.HFOV_deg * 0.5));
	fy = (cam.imgH * 0.5) / std::tan(deg2rad(cam.VFOV_deg * 0.5));
}

// 给定距离，计算期望像素尺寸
static inline cv::Size expectedSizePx(const CameraSpec& cam, const BoardSpec& bd, double Z_m) {
	double fx, fy; focalInPixels(cam, fx, fy);
	int w = std::max(4, (int)std::round(fx * bd.physW_m / Z_m));
	int h = std::max(8, (int)std::round(fy * bd.physH_m / Z_m));
	return { w, h };
}

// 生成 2×8 的棋盘模板（黑白相间），用于模板匹配
static cv::Mat makeBoardTemplate(const cv::Size& sz, const BoardSpec& bd) {
	cv::Mat templ(sz, CV_8UC1, cv::Scalar(255)); // 白底
	// 每个小格的像素尺寸
	const int cellW = std::max(1, sz.width / bd.cols);
	const int cellH = std::max(1, sz.height / bd.rows);

	// 以左上角黑色开始的标准棋盘
	for (int r = 0; r < bd.rows; ++r) {
		for (int c = 0; c < bd.cols; ++c) {
			bool black = ((r + c) % 2 == 0);
			if (black) {
				int x = c * cellW;
				int y = r * cellH;
				int w = (c == bd.cols - 1) ? (sz.width - x) : cellW;
				int h = (r == bd.rows - 1) ? (sz.height - y) : cellH;
				cv::rectangle(templ, cv::Rect(x, y, w, h), cv::Scalar(0), cv::FILLED);
			}
		}
	}
	// 略做平滑，容忍拍摄模糊
	cv::GaussianBlur(templ, templ, cv::Size(3, 3), 0);
	return templ;
}

/**
 * 在距离 Z_m 时，检测中心 1.2w×1.2h 搜索窗里的标定板。
 * 返回：是否在中心；并在 frame 上用黄色画出检测到的外接框 & 搜索窗。
 */
bool detectBoardAtDistance(cv::Mat& frameBGR,
	double Z_m,
	const CameraSpec& cam,
	const BoardSpec& bd,
	double searchScale,
	double scoreThresh,
	cv::Rect* outDetRect,
	cv::Rect* outSearchRoi,
	double* outScore)
{
	// ---- 输出默认值
	if (outDetRect)   *outDetRect = cv::Rect();
	if (outSearchRoi) *outSearchRoi = cv::Rect();
	if (outScore)     *outScore = 0.0;

	// ---- 基础检查
	if (frameBGR.empty()) return false;
	CV_Assert(frameBGR.depth() == CV_8U);

	// ---- 用“实际图像尺寸”，不要用 cam.imgW/imgH
	const int W = frameBGR.cols;
	const int H = frameBGR.rows;

	// ---- 灰度图
	cv::Mat gray;
	if (frameBGR.channels() == 3)
		cv::cvtColor(frameBGR, gray, cv::COLOR_BGR2GRAY);
	else
		gray = frameBGR;

	auto deg2rad = [](double d) { return d * CV_PI / 180.0; };

	// ---- 焦距(像素) 用实际分辨率计算
	const double fx = (W * 0.5) / std::tan(deg2rad(cam.HFOV_deg * 0.5));
	const double fy = (H * 0.5) / std::tan(deg2rad(cam.VFOV_deg * 0.5));

	// ---- 期望像素尺寸（棋盘本体 0.20m x 0.80m）
	int expW = std::max(4, (int)std::round(fx * bd.physW_m / Z_m));
	int expH = std::max(8, (int)std::round(fy * bd.physH_m / Z_m));

	// 过远时太小，直接返回
	if (expW < 10 || expH < 40) return false;

	// ---- 以图像中心建立搜索 ROI，并与图像边界相交
	const int cx = W / 2, cy = H / 2;
	int roiW = std::min(W, std::max((int)std::round(expW * searchScale), 20));
	int roiH = std::min(H, std::max((int)std::round(expH * searchScale), 40));
	cv::Rect roi(cx - roiW / 2, cy - roiH / 2, roiW, roiH);
	roi &= cv::Rect(0, 0, W, H);                 // **关键：裁剪到图像内**

	if (roi.width <= 0 || roi.height <= 0) return false;

	if (outSearchRoi) *outSearchRoi = roi;

	// ---- 取 ROI （这句之前已确保不越界）
	cv::Mat roiImg = gray(roi).clone();
	cv::GaussianBlur(roiImg, roiImg, cv::Size(3, 3), 0);

	// ---- 多尺度模板匹配（±15%）
	const double scales[] = { 0.85, 0.93, 1.00, 1.07, 1.15 };
	double bestScore = -1.0; cv::Rect bestRect;

	for (double s : scales) {
		cv::Size templSz(std::max(8, (int)std::round(expW * s)),
			std::max(24, (int)std::round(expH * s)));
		templSz.width = std::min(templSz.width, roiImg.cols - 1);
		templSz.height = std::min(templSz.height, roiImg.rows - 1);
		if (templSz.width < 8 || templSz.height < 24) continue;

		cv::Mat templ = makeBoardTemplate(templSz, bd);

		cv::Mat result;
		cv::matchTemplate(roiImg, templ, result, cv::TM_CCOEFF_NORMED);

		double maxVal; cv::Point maxLoc;
		cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

		if (maxVal > bestScore) {
			bestScore = maxVal;
			bestRect = cv::Rect(roi.x + maxLoc.x, roi.y + maxLoc.y, templSz.width, templSz.height);
		}
	}

	// ---- 画框 & 判定
	cv::rectangle(frameBGR, roi, cv::Scalar(0, 255, 255), 1, cv::LINE_AA); // 搜索窗(黄)

	if (bestScore >= scoreThresh && bestRect.area() > 0) {
		cv::rectangle(frameBGR, bestRect, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
		cv::Point imgC(W / 2, H / 2);
		cv::Point detC(bestRect.x + bestRect.width / 2, bestRect.y + bestRect.height / 2);
		cv::drawMarker(frameBGR, imgC, cv::Scalar(0, 255, 255), cv::MARKER_CROSS, 16, 2, cv::LINE_AA);
		cv::drawMarker(frameBGR, detC, cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 16, 2, cv::LINE_AA);

		if (outDetRect) *outDetRect = bestRect;
		if (outScore)   *outScore = bestScore;

		// 偏心阈值：期望尺寸的 10%
		const double dx = std::abs(detC.x - imgC.x);
		const double dy = std::abs(detC.y - imgC.y);
		const bool centered = (dx <= 0.1 * expW) && (dy <= 0.1 * expH);

		char buf[128];
		std::snprintf(buf, sizeof(buf), "score=%.2f %s", bestScore, centered ? "[CENTERED]" : "");
		cv::putText(frameBGR, buf, cv::Point(bestRect.x, std::max(0, bestRect.y - 6)),
			cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

		return centered;
	}

	cv::putText(frameBGR, "board not found in ROI",
		cv::Point(roi.x, std::max(0, roi.y - 6)),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
	return false;
}

static inline cv::Rect clampRect(const cv::Rect& r, const cv::Size& imsz) {
	int x = std::max(0, r.x), y = std::max(0, r.y);
	int w = std::min(r.width, imsz.width - x);
	int h = std::min(r.height, imsz.height - y);
	return (w > 0 && h > 0) ? cv::Rect(x, y, w, h) : cv::Rect();
}

bool detectBoardAtDistanceHorizontal(cv::Mat& frameBGR,
	double Z_m,
	const CameraSpec& cam,
	const BoardSpec&  bd,
	double searchScale,
	double scoreThresh,
	cv::Rect* outDetRect,
	cv::Rect* outSearchRoi,
	double*  outScore)
{
	if (outDetRect)   *outDetRect = cv::Rect();
	if (outSearchRoi) *outSearchRoi = cv::Rect();
	if (outScore)     *outScore = 0.0;

	if (frameBGR.empty()) return false;
	CV_Assert(frameBGR.depth() == CV_8U);

	// —— 用实际图像尺寸推 FOV 像素焦距
	const int W = frameBGR.cols, H = frameBGR.rows;
	const double fx = (W*0.5) / std::tan(deg2rad(cam.HFOV_deg*0.5));
	const double fy = (H*0.5) / std::tan(deg2rad(cam.VFOV_deg*0.5));

	// 期望像素尺寸（横置：宽0.80×高0.20）
	const int expW = std::max(8, (int)std::round(fx * bd.physW_m / Z_m));
	const int expH = std::max(16, (int)std::round(fy * bd.physH_m / Z_m));
	if (expW < 40 || expH < 20) return false; // 太小就不搜

	// —— 构造【带状 ROI】：x方向放宽（左右可偏），y方向贴中部
	const int cx = W / 2, cy = H / 2;
	// 你可以调这两个系数：越大越宽容
	const double roiScaleX = 2.5;   // 让 ROI 仅比靶板宽 1.6 倍
	const double roiScaleY = 1.6;

	int bandW = std::min(W, std::max(20, (int)std::round(expW * roiScaleX)));
	int bandH = std::min(H, std::max(20, (int)std::round(expH * roiScaleY)));
	bandW = std::min(bandW, W);
	bandH = std::min(bandH, H);

	cv::Rect roi(cx - bandW / 2, cy - bandH / 2, bandW, bandH);
	roi = clampRect(roi, frameBGR.size());
	if (roi.empty()) return false;
	if (outSearchRoi) *outSearchRoi = roi;

	// —— 预处理
	cv::Mat gray, roiImg, result;
	if (frameBGR.channels() == 3) cv::cvtColor(frameBGR, gray, cv::COLOR_BGR2GRAY);
	else gray = frameBGR;
	roiImg = gray(roi).clone();
	cv::GaussianBlur(roiImg, roiImg, cv::Size(3, 3), 0);

	// —— 多尺度 NCC 模板匹配（±15%）
	const double scales[] = { 0.85, 0.93, 1.00, 1.07, 1.15 };
	double bestScore = -1.0; cv::Rect bestRect;

	for (double s : scales) {
		cv::Size templSz(std::max(16, (int)std::round(expW*s)),
			std::max(12, (int)std::round(expH*s)));
		templSz.width = std::min(templSz.width, roiImg.cols - 2);
		templSz.height = std::min(templSz.height, roiImg.rows - 2);
		if (templSz.width < 16 || templSz.height < 12) continue;

		cv::Mat templ = makeBoardTemplate(templSz, bd);

		cv::matchTemplate(roiImg, templ, result, cv::TM_CCOEFF_NORMED);
		double maxVal; cv::Point maxLoc;
		cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);

		// 反色再试一遍（现场亮/暗反转时有用）
		cv::Mat templInv = 255 - templ, resultInv;
		cv::matchTemplate(roiImg, templInv, resultInv, cv::TM_CCOEFF_NORMED);
		double maxValInv; cv::Point maxLocInv;
		cv::minMaxLoc(resultInv, nullptr, &maxValInv, nullptr, &maxLocInv);

		if (maxValInv > maxVal) { maxVal = maxValInv; maxLoc = maxLocInv; templ = templInv; }

		if (maxVal > bestScore) {
			bestScore = maxVal;
			bestRect = cv::Rect(roi.x + maxLoc.x, roi.y + maxLoc.y, templSz.width, templSz.height);
		}
	}

	// —— 可视化 ROI
	cv::rectangle(frameBGR, roi, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);

	if (bestScore >= scoreThresh && bestRect.area() > 0) {
		// 画检测框
		cv::rectangle(frameBGR, bestRect, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

		if (outDetRect) *outDetRect = bestRect;
		if (outScore)   *outScore = bestScore;

		// 判“近中心”：X 更宽松，Y 略严格（围绕棋盘中心线）
		const cv::Point imgC(W / 2, H / 2);
		const cv::Point detC(bestRect.x + bestRect.width / 2,
			bestRect.y + bestRect.height / 2);

		// 放宽 X：允许 ±0.35 * 预测宽，Y：±0.20 * 预测高（你可按需调）
		const double tolX = 0.35 * expW;
		const double tolY = 0.20 * expH;
		const bool centered = (std::abs(detC.x - imgC.x) <= tolX) &&
			(std::abs(detC.y - imgC.y) <= tolY);

		// 十字辅助
		cv::drawMarker(frameBGR, imgC, cv::Scalar(0, 255, 255), cv::MARKER_CROSS, 16, 1, cv::LINE_AA);
		cv::drawMarker(frameBGR, detC, cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 16, 1, cv::LINE_AA);

		char buf[128];
		std::snprintf(buf, sizeof(buf), "score=%.2f %s", bestScore, centered ? "[CENTERED]" : "");
		cv::putText(frameBGR, buf, cv::Point(bestRect.x, std::max(0, bestRect.y - 6)),
			cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
		return centered;
	}

	cv::putText(frameBGR, "board not found in band-ROI",
		cv::Point(roi.x, std::max(0, roi.y - 6)),
		cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
	return false;
}
