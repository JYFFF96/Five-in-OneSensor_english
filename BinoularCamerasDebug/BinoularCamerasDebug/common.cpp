#include "common.h"

StereoCalibrationParameters g_cameraBiaodingParams = { 0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0 };
CameraInfo g_cameraInfoParams = { 0.66f, 0.66f, 0.66f, 0.66f, 2.0f, 0.66f };
RotationMatrix g_rotationMatrix = {
	{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // real3DToImage
	{0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}                     // imageToReal3D
};

static inline float solveZForRow(const cv::Matx34f& M, float Yg, float y_pix) {
	// 行列索引：M(r,c)
	float m11 = M(1, 1), m12 = M(1, 2), m13 = M(1, 3);
	float m21 = M(2, 1), m22 = M(2, 2), m23 = M(2, 3);
	float denom = (m12 - y_pix * m22);
	if (fabs(denom) < 1e-9f) return std::numeric_limits<float>::quiet_NaN();
	float num = (y_pix * m21 - m11) * Yg + (y_pix * m23 - m13);
	return num / denom;
}


static inline cv::Matx34f makeProj34(const RotationMatrix& rot) {
	const float* m = rot.real3DToImage;
	return cv::Matx34f(
		m[0], m[1], m[2], m[3],
		m[4], m[5], m[6], m[7],
		m[8], m[9], m[10], m[11]
	);
}

static inline cv::Point projectWorldToPixel(const cv::Matx34f& M, const cv::Point3f& Pw) {
	cv::Vec4f X(Pw.x, Pw.y, Pw.z, 1.f);
	cv::Vec3f UVW = M * X;
	float w = UVW[2];
	if (fabs(w) < 1e-6f) return { -10000, -10000 };
	return cv::Point(cvRound(UVW[0] / w), cvRound(UVW[1] / w));
}

void drawGroundRegion(cv::Mat& frame,
	const RotationMatrix& rotation,
	const CameraInfo& camInfo,
	float Znear_m, float Zfar_m,
	float Wnear_m, float Wfar_m,
	int worldYMode, bool drawOutline, double alpha)
{
	CV_Assert(frame.data && Znear_m > 0.f && Zfar_m > Znear_m && Wnear_m > 0.f && Wfar_m > 0.f);

	cv::Matx34f M = makeProj34(rotation);
	float Yg = (worldYMode == 0) ? camInfo.cameraHeight : -camInfo.cameraHeight;

	std::vector<cv::Point> poly;
	poly.reserve(4);
	poly.push_back(projectWorldToPixel(M, { -Wnear_m * 0.5f, Yg, Znear_m }));
	poly.push_back(projectWorldToPixel(M, { +Wnear_m * 0.5f, Yg, Znear_m }));
	poly.push_back(projectWorldToPixel(M, { +Wfar_m * 0.5f, Yg, Zfar_m }));
	poly.push_back(projectWorldToPixel(M, { -Wfar_m * 0.5f, Yg, Zfar_m }));

	for (auto& p : poly) {
		if (p.x < -5000 || p.y < -5000) return; // 投影失败直接返回
	}

	if (drawOutline) {
		for (int i = 0; i < (int)poly.size(); ++i) {
			cv::line(frame, poly[i], poly[(i + 1) % poly.size()], cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
		}
	}
}

void drawGroundRegionByRows(cv::Mat & frame, const RotationMatrix & rotation, const CameraInfo & camInfo, int y_top, int y_bottom, float Wtop_m, float Wbottom_m, bool drawOutline, double alpha)
{
	CV_Assert(frame.data && y_top >= 0 && y_top < frame.rows && y_bottom > y_top && y_bottom < frame.rows);

	cv::Matx34f M = makeProj34(rotation);
	float Yg = camInfo.cameraHeight;   // ✅ 地面在 +H

	// 反解两个深度
	float Ztop = solveZForRow(M, Yg, (float)y_top);
	float Zbot = solveZForRow(M, Yg, (float)y_bottom);
	if (!std::isfinite(Ztop) || !std::isfinite(Zbot)) return;

	// 四角（世界→像素）
	std::vector<cv::Point> poly;
	poly.reserve(4);
	poly.push_back(projectWorldToPixel(M, { -Wbottom_m * 0.5f, Yg, Zbot })); // 左下
	poly.push_back(projectWorldToPixel(M, { +Wbottom_m * 0.5f, Yg, Zbot })); // 右下
	poly.push_back(projectWorldToPixel(M, { +Wtop_m * 0.5f, Yg, Ztop })); // 右上
	poly.push_back(projectWorldToPixel(M, { -Wtop_m * 0.5f, Yg, Ztop })); // 左上

	// 半透明填充 + 边框
	cv::Mat overlay = frame.clone();
	const cv::Point* pts[] = { poly.data() };
	int npts[] = { (int)poly.size() };
	cv::fillPoly(overlay, pts, npts, 1, cv::Scalar(0, 255, 0));
	cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0.0, frame);
	if (drawOutline) for (int i = 0; i < 4; ++i)
		cv::line(frame, poly[i], poly[(i + 1) % 4], cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
}

void drawLaneLines(cv::Mat& frame,
	const RotationMatrix& rotation,
	const CameraInfo& camInfo,
	float maxDist,  // 画多远
	bool useCarWidth) // true=用车辆宽度, false=用车道宽
{
	cv::Matx34f M = makeProj34(rotation);
	float Yg = camInfo.cameraHeight;

	// 车宽或车道宽
	float width = useCarWidth ? camInfo.carWidth : 3.5f;  // 默认3.5m车道

	for (int side = -1; side <= 1; side += 2) {
		std::vector<cv::Point> pts;
		for (float z = 1.0f; z <= maxDist; z += 1.0f) {
			cv::Point p = projectWorldToPixel(M,
				cv::Point3f(side * width * 0.5f, Yg, z));
			if (p.x >= 0 && p.x < frame.cols && p.y >= 0 && p.y < frame.rows)
				pts.push_back(p);
		}
		if (pts.size() > 1) {
			cv::polylines(frame, pts, false, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
		}
	}
}