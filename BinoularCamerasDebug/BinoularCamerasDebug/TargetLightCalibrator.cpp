#include "TargetLightCalibrator.h"
using namespace cv;
using std::vector;

cv::Rect2f TargetLightCalibrator::scaleAroundCenter(const cv::Rect2f& r, float s) {
	cv::Point2f c(r.x + r.width*0.5f, r.y + r.height*0.5f);
	cv::Size2f  sz(r.width * s, r.height * s);
	return cv::Rect2f(c.x - sz.width*0.5f, c.y - sz.height*0.5f, sz.width, sz.height);
}

void TargetLightCalibrator::orderQuad(vector<Point2f>& q) {
	std::sort(q.begin(), q.end(), [](const Point2f&a, const Point2f&b) {
		return (a.y < b.y) || (a.y == b.y && a.x < b.x);
		});
	Point2f tl = q[0].x < q[1].x ? q[0] : q[1];
	Point2f tr = q[0].x < q[1].x ? q[1] : q[0];
	Point2f bl = q[2].x < q[3].x ? q[2] : q[3];
	Point2f br = q[2].x < q[3].x ? q[3] : q[2];
	q = { tl,tr,br,bl };
}

cv::Rect2f TargetLightCalibrator::expectedRectPx(const cv::Size& sz, bool chess) const {
	double fx = sz.width / (2.0 * std::tan(deg2rad(hfov*0.5)));
	double fy = sz.height / (2.0 * std::tan(deg2rad(vfov*0.5)));
	double Sw = chess ? 0.20 : boardW; // m
	double Sh = chess ? 0.80 : boardH; // m
	double wpx = fx * (Sw / Z);
	double hpx = fy * (Sh / Z);
	cv::Point2f c(sz.width*0.5f, sz.height*0.5f);
	return cv::Rect2f(c.x - float(wpx*0.5), c.y - float(hpx*0.5), float(wpx), float(hpx));
}

float TargetLightCalibrator::roiScaleFor(const cv::Rect2f& expR, const cv::Size& imsz) {
	double cov = (expR.width * expR.height) / double(std::max(1, imsz.width*imsz.height)); // 0~1
	double s = 0.45 / std::sqrt(std::max(1e-6, cov));               // 更紧凑的系数
	return (float)std::min(3.2, std::max(1.8, s));                  // 夹到 [1.8, 3.2]
}

bool TargetLightCalibrator::detectTargetBoard2x8(const cv::Mat& gray,
	cv::RotatedRect& rr,
	std::vector<cv::Point2f>& boardQuad) const
{
	CV_Assert(!gray.empty() && gray.channels() == 1);
	boardQuad.clear();

	const cv::Size imgSz = gray.size();

	// ---------- 1) 期望框 -> ROI（自适应放大） ----------
	cv::Rect2f expR = expectedRectPx(imgSz, /*chess=*/false);        // 以整板 40x90 估一个尺度与中心
	float scale = roiScaleFor(expR, imgSz);                          // 自适应放大倍数（你项目里已有）
	cv::Rect2f roiF = scaleAroundCenter(expR, scale) &
		cv::Rect2f(0.f, 0.f, (float)imgSz.width, (float)imgSz.height);
	cv::Rect roi(cvRound(roiF.x), cvRound(roiF.y), cvRound(roiF.width), cvRound(roiF.height));
	roi &= cv::Rect(0, 0, imgSz.width, imgSz.height);

	// 如需排查 ROI，解开下行：整幅图检测
	// roi = cv::Rect(0,0,imgSz.width,imgSz.height);

	cv::Mat patch = gray(roi).clone();

	// ---------- 2) 预处理：增强 + 平滑 + 边缘 + 形态学 ----------
	cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.6, cv::Size(8, 8));
	clahe->apply(patch, patch);
	cv::GaussianBlur(patch, patch, cv::Size(5, 5), 0.8);

	cv::Mat edges;
	cv::Canny(patch, edges, 30, 90); // 较松

	cv::morphologyEx(edges, edges, cv::MORPH_CLOSE,
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
	cv::erode(edges, edges,
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2)));

	// ---------- 3) 轮廓候选 ----------
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	// 期望像素尺寸（用于打分/约束）
	const double fx = imgSz.width / (2.0 * std::tan(deg2rad(hfov*0.5)));
	const double fy = imgSz.height / (2.0 * std::tan(deg2rad(vfov*0.5)));
	const double wExpPx = fx * (boardW / Z);
	const double hExpPx = fy * (boardH / Z);

	auto rectIoU = [](const cv::Rect2f& a, const cv::Rect2f& b) {
		cv::Rect2f inter = a & b;
		float ia = inter.area();
		return ia > 0 ? ia / (a.area() + b.area() - ia) : 0.f;
	};

	auto bboxF = [](const std::vector<cv::Point2f>& pts)->cv::Rect2f {
		float xmin = FLT_MAX, ymin = FLT_MAX, xmax = -FLT_MAX, ymax = -FLT_MAX;
		for (auto& p : pts) {
			xmin = std::min(xmin, p.x); ymin = std::min(ymin, p.y);
			xmax = std::max(xmax, p.x); ymax = std::max(ymax, p.y);
		}
		return cv::Rect2f(xmin, ymin, xmax - xmin, ymax - ymin);
	};

	double bestScore = -1.0;
	std::vector<cv::Point2f> bestQuad;

	for (auto& c : contours) {
		double areaC = cv::contourArea(c);
		if (areaC < 120) continue;                        // 面积门

		cv::RotatedRect r = cv::minAreaRect(c);
		float w = r.size.width, h = r.size.height;
		if (w < 8 || h < 8) continue;

		float L = std::max(w, h), S = std::min(w, h);
		double ar = L / std::max(1.f, S);
		if (ar < 1.1 || ar > 5.8) continue;               // 长宽比门

		// 填充率：过滤“外溢一圈”的矩形
		double fill = areaC / std::max(1.f, w*h);
		if (fill < 0.45) continue;

		// 与期望尺寸越接近分越高
		auto nearScore = [](double v, double e) {
			double d = std::abs(v - e);
			return std::exp(-d / (0.6*std::max(30.0, e)));
		};
		double score = nearScore(L, std::max(wExpPx, hExpPx)) * nearScore(S, std::min(wExpPx, hExpPx));

		// ROI 坐标下四角（排序）
		std::vector<cv::Point2f> box(4); r.points(box.data());
		orderQuad(box);

		// ---------- 4) 透视展开到标准平面（400×900） ----------
		const int Wc = 400, Hc = 900;
		std::vector<cv::Point2f> dst4 = { {0,0},{(float)Wc,0},{(float)Wc,(float)Hc},{0,(float)Hc} };
		cv::Mat Hm = cv::getPerspectiveTransform(box, dst4);
		if (!cv::checkRange(Hm) || std::abs(cv::determinant(Hm)) < 1e-8) continue;

		cv::Mat rect; cv::warpPerspective(patch, rect, Hm, cv::Size(Wc, Hc), cv::INTER_LINEAR);

		// ---------- 5) 宽松“2×8 结构 + 黑块占比”校验（硬门） ----------
		cv::Rect core((Wc - 200) / 2, (Hc - 800) / 2, 200, 800);
		core &= cv::Rect(0, 0, Wc, Hc);
		cv::Mat roiCore = rect(core).clone();

		// 5.1 水平分界峰
		cv::Mat gy; cv::Sobel(roiCore, gy, CV_32F, 0, 1, 3);
		cv::Mat profY; cv::reduce(cv::abs(gy), profY, 1, cv::REDUCE_AVG, CV_32F);
		std::vector<int> peaks; peaks.reserve(8);
		int Hroi = roiCore.rows, minGap = std::max(5, (int)std::round((Hroi / 8.0)*0.40));
		for (int y = 2; y < Hroi - 2; ++y) {
			float v = profY.at<float>(y, 0);
			if (v > profY.at<float>(y - 1, 0) && v >= profY.at<float>(y + 1, 0)) {
				if (peaks.empty() || (y - peaks.back() >= minGap)) peaks.push_back(y);
			}
		}
		bool structureOK = false;
		if ((int)peaks.size() >= 5) {
			if ((int)peaks.size() > 7) {
				std::vector<std::pair<float, int>> cand;
				for (int y : peaks) cand.push_back({ profY.at<float>(y,0), y });
				std::sort(cand.begin(), cand.end(), [](auto&a, auto&b) {return a.first > b.first; });
				peaks.clear(); for (int i = 0; i < 7; i++) peaks.push_back(cand[i].second);
				std::sort(peaks.begin(), peaks.end());
			}
			if (peaks.size() >= 2) {
				std::vector<float> gaps; gaps.reserve(peaks.size() - 1);
				for (size_t i = 0; i + 1 < peaks.size(); ++i) gaps.push_back((float)(peaks[i + 1] - peaks[i]));
				float meanGap = (float)cv::mean(gaps)[0];
				float sd = 0.f; for (float g : gaps) { sd += (g - meanGap)*(g - meanGap); }
				sd = std::sqrt(sd / std::max(1, (int)gaps.size() - 1));
				structureOK = (meanGap > 0 && (sd / meanGap <= 0.35f));
			}
		}

		// 5.2 黑块占比
		bool darkOK = false;
		{
			cv::Mat eq; cv::equalizeHist(roiCore, eq);
			cv::Mat bw; cv::threshold(eq, bw, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
			double darkRatio = (double)cv::countNonZero(bw) / (bw.total() + 1e-9);
			darkOK = (darkRatio > 0.18 && darkRatio < 0.60);
		}

		// —— 结构&占比必须成立（硬门），并略加分 —— //
		if (!structureOK || !darkOK) continue;
		if (structureOK) score *= 1.5;
		if (darkOK)      score *= 1.2;

		// ---------- 6) 外框精化（梯度投影） ----------
		cv::Mat gx2, gy2; cv::Sobel(rect, gx2, CV_32F, 1, 0, 3); cv::Sobel(rect, gy2, CV_32F, 0, 1, 3);
		cv::Mat projX, projY; cv::reduce(cv::abs(gx2), projX, 0, cv::REDUCE_SUM, CV_32F);
		cv::reduce(cv::abs(gy2), projY, 1, cv::REDUCE_SUM, CV_32F);

		auto argmax = [](const cv::Mat& v, int a, int b)->int {
			a = std::max(a, 0); b = std::min(b, (int)v.total() - 1);
			int idx = a; float best = -1.f;
			for (int i = a; i <= b; ++i) {
				float t = (v.rows == 1) ? v.at<float>(0, i) : v.at<float>(i, 0);
				if (t > best) { best = t; idx = i; }
			}
			return idx;
		};
		int xL = argmax(projX, int(Wc*0.05), int(Wc*0.35));
		int xR = argmax(projX, int(Wc*0.65), int(Wc*0.95));
		int yT = argmax(projY, int(Hc*0.05), int(Hc*0.25));
		int yB = argmax(projY, int(Hc*0.75), int(Hc*0.95));

		std::vector<cv::Point2f> rectQuad = {
			{(float)xL,(float)yT}, {(float)xR,(float)yT},
			{(float)xR,(float)yB}, {(float)xL,(float)yB}
		};

		cv::Mat Hinv = Hm.inv();
		std::vector<cv::Point2f> refined;
		cv::perspectiveTransform(rectQuad, refined, Hinv);

		// 加回 ROI 偏移
		for (auto& p : refined) p += cv::Point2f((float)roi.x, (float)roi.y);

		// refined 合理才用；否则回退到未精化的 box（也要加回 ROI 偏移）
		auto areaQ = [](const std::vector<cv::Point2f>& q) {
			double a = 0; for (int i = 0; i < 4; i++) { auto p = q[i], qn = q[(i + 1) & 3]; a += p.x*qn.y - p.y*qn.x; }
			return std::abs(a)*0.5;
		};
		std::vector<cv::Point2f> boxFull = {
			box[0] + cv::Point2f((float)roi.x,(float)roi.y),
			box[1] + cv::Point2f((float)roi.x,(float)roi.y),
			box[2] + cv::Point2f((float)roi.x,(float)roi.y),
			box[3] + cv::Point2f((float)roi.x,(float)roi.y)
		};
		std::vector<cv::Point2f> chosen = (areaQ(refined) > 200.0) ? refined : boxFull;

		// ---------- 7) 硬约束：姿态/IoU/中心 ----------
		cv::RotatedRect rCand = cv::minAreaRect(chosen);
		double angle = rCand.angle;                                 // [-90,0)
		if (rCand.size.width >= rCand.size.height) angle += 90.0;   // 取“长边相对水平”的角度
		double tiltFromVertical = std::abs(90.0 - std::abs(angle)); // 0=竖直
		const double maxTiltDeg = 25.0;
		if (tiltFromVertical > maxTiltDeg) continue;

		cv::Rect2f detAABB = bboxF(chosen);
		cv::Rect2f expAABB = expR;
		double iou = rectIoU(detAABB, expAABB);
		const double minIoU = 0.06;
		if (iou < minIoU) continue;

		cv::Point2f expC(expR.x + expR.width*0.5f, expR.y + expR.height*0.5f);
		double centerDist = cv::norm(rCand.center - expC);
		if (centerDist > 2.0 * centerTolPx) continue;

		// —— 合格候选，用 score 选最好 —— //
		if (score > bestScore) {
			bestScore = score;
			bestQuad = chosen;
		}
	}

	// ---------- 8) 返回最佳；若无则兜底 ----------
	if (!bestQuad.empty()) {
		boardQuad = bestQuad;
		orderQuad(boardQuad);
		rr = cv::minAreaRect(boardQuad);
		return true;
	}

	// ===== 兜底：按黑格质心估计外框 =====
	auto runFallback = [&](const cv::Rect& roiLocal)->bool {
		cv::Mat work = gray(roiLocal).clone();
		cv::Ptr<cv::CLAHE> clahe2 = cv::createCLAHE(3.0, cv::Size(8, 8));
		clahe2->apply(work, work);

		cv::Mat bw; cv::threshold(work, bw, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
		cv::morphologyEx(bw, bw, cv::MORPH_OPEN,
			cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));

		std::vector<std::vector<cv::Point>> cs;
		cv::findContours(bw, cs, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		std::vector<cv::Point2f> centers; centers.reserve(cs.size());
		for (auto &cc : cs) {
			cv::Rect bb = cv::boundingRect(cc);
			if (bb.area() < 60) continue;
			float ar = (float)std::max(bb.width, bb.height) / std::max(1, std::min(bb.width, bb.height));
			if (ar > 1.6) continue; // 近似正方（棋盘格）
			centers.push_back(cv::Point2f(bb.x + bb.width*0.5f, bb.y + bb.height*0.5f));
		}

		if (centers.size() >= 4) {
			cv::RotatedRect r2 = cv::minAreaRect(centers);
			cv::Point2f p4[4]; r2.points(p4);
			boardQuad = {
				p4[0] + cv::Point2f((float)roiLocal.x,(float)roiLocal.y),
				p4[1] + cv::Point2f((float)roiLocal.x,(float)roiLocal.y),
				p4[2] + cv::Point2f((float)roiLocal.x,(float)roiLocal.y),
				p4[3] + cv::Point2f((float)roiLocal.x,(float)roiLocal.y)
			};
			orderQuad(boardQuad);
			rr = cv::RotatedRect(r2.center + cv::Point2f((float)roiLocal.x, (float)roiLocal.y),
				r2.size, r2.angle);
			return true;
		}
		return false;
	};

	return runFallback(roi);
}


cv::Mat TargetLightCalibrator::processFrame(const cv::Mat& src)
{
	CV_Assert(!src.empty());

	// --- 准备 BGR/GRAY ---
	cv::Mat out;
	if (src.channels() == 3) out = src.clone();
	else cv::cvtColor(src, out, cv::COLOR_GRAY2BGR);

	cv::Mat gray;
	if (src.channels() == 3) cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
	else gray = src;

	const int W = out.cols, H = out.rows;
	const cv::Point center(W / 2, H / 2);

	// --- 先做检测（拿到旋转矩形 rr 与四角 quad） ---
	cv::RotatedRect rr; std::vector<cv::Point2f> quad;
	bool detected = detectTargetBoard2x8(gray, rr, quad);

	// --- 由 FOV 与分辨率计算像素焦距 ---
	const double fx = W / (2.0 * std::tan(deg2rad(hfov*0.5)));
	const double fy = H / (2.0 * std::tan(deg2rad(vfov*0.5)));

	// 工具：以中心和给定宽高构造矩形（仅用于画框）
	auto rectFromWH = [&](double w, double h)->cv::Rect2f {
		return cv::Rect2f(center.x - float(w*0.5), center.y - float(h*0.5),
			float(w), float(h));
	};

	// 两种参照：整板(40×90) 与 中间棋盘(20×80) 的期望宽高（像素）
	auto expDims = [&](bool chess)->std::pair<double, double> {
		double Sw = chess ? 0.20 : boardW;   // m
		double Sh = chess ? 0.80 : boardH;   // m
		return { fx*(Sw / Z), fy*(Sh / Z) };
	};
	auto whFull = expDims(false);  // 40×90
	auto whChess = expDims(true);   // 20×80

	// —— 红框固定画整板 40×90 —— //
	cv::Rect2f rExpDraw = rectFromWH(whFull.first, whFull.second);

	// --- 判定：用旋转矩形的短/长边与期望宽/高比较 ---
	bool ok = false;

	if (detected) {
		// 旋转矩形的短/长边：短边≈物理“宽”，长边≈物理“高”
		const double detShort = std::min(rr.size.width, rr.size.height);
		const double detLong = std::max(rr.size.width, rr.size.height);

		// 两种参照的尺寸误差（相对误差）
		auto relErr = [&](double wExp, double hExp) {
			double eW = std::abs(detShort - wExp) / (wExp + 1e-6);
			double eH = std::abs(detLong - hExp) / (hExp + 1e-6);
			return std::make_pair(eW, eH);
		};
		auto eFull = relErr(whFull.first, whFull.second);
		auto eChess = relErr(whChess.first, whChess.second);

		// 中心误差用红框中心（图像中心）
		const cv::Point2f cDet = rr.center;
		const cv::Point2f cExp(center.x, center.y);
		const double centerErr = std::hypot(double(cDet.x - cExp.x), double(cDet.y - cExp.y));

		const double centerTol = centerTolPx;   // 像素
		const double sizeTol = sizeTolRel;    // 相对

		// —— 判定：FULL 或 CHESS 任一满足就通过 —— //
		bool okFull = (centerErr <= centerTol) && (eFull.first <= sizeTol) && (eFull.second <= sizeTol);
		bool okChess = (centerErr <= centerTol) && (eChess.first <= sizeTol) && (eChess.second <= sizeTol);
		ok = okFull || okChess;

		// —— 调试叠字（可注释）——
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"FULL eW=%.2f eH=%.2f | CHESS eW=%.2f eH=%.2f | C=%.1f<=%.1f  Z=%.2f  %s",
			eFull.first, eFull.second, eChess.first, eChess.second,
			centerErr, centerTol, Z, ok ? (okFull ? "[PASS FULL]" : "[PASS CHESS]") : "[FAIL]");
		cv::putText(out, buf, cv::Point(20, 32),
			cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(50, 255, 50), 2, cv::LINE_AA);

		// —— 调试：画检测到的外轮廓（黄，使用 int 点防异常）——
		if (quad.size() == 4) {
			std::vector<cv::Point> poly(4);
			for (int i = 0; i < 4; ++i) {
				int xi = std::max(0, std::min(W - 1, cvRound(quad[i].x)));
				int yi = std::max(0, std::min(H - 1, cvRound(quad[i].y)));
				poly[i] = cv::Point(xi, yi);
			}
			const cv::Point* pts[1] = { poly.data() };
			int npts[1] = { 4 };
			cv::polylines(out, pts, npts, 1, /*closed=*/true, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
		}
	}
	else {
		cv::putText(out, "not detected", cv::Point(20, 32),
			cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
	}

	// --- 画贯穿十字 + 红/绿期望框（红框固定 40×90） ---
	cv::Scalar col = ok ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
	cv::line(out, cv::Point(0, center.y), cv::Point(W - 1, center.y), col, 2, cv::LINE_AA);
	cv::line(out, cv::Point(center.x, 0), cv::Point(center.x, H - 1), col, 2, cv::LINE_AA);
	cv::rectangle(out, rExpDraw, col, 2, cv::LINE_AA);

	// --- 状态变化才发信号 ---
	if (ok != lastOk) { lastOk = ok; emit recognitionStateChanged(ok); }

	return out;
}
