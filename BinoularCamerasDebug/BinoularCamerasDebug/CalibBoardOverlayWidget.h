#pragma once
// CalibBoardOverlayWidget.hpp
// Qt 5.12 + OpenCV 3.4.1
// - Displays cv::Mat in a QWidget with aspect-preserving auto-resize
// - Draws a crosshair and a center-aligned rectangle sized to a real 40cm x 90cm board at a given distance
// - Detects the board-like quadrilateral and toggles overlay color (green when matched, red otherwise)
// - Emits recognitionChanged(bool matched) only on state changes
//
// Assumptions (due to spec ambiguity):
//   * Board total size: 0.40 m (width) x 0.90 m (height)
//   * Chessboard region: 0.20 m (width) x 0.80 m (height), vertical orientation (2x8 squares => 1x7 inner corners)
//   * Camera: HFOV=38бу, VFOV=21бу, resolution=1280x720
//   * Image input is BGR8 cv::Mat (CV_8UC3) or gray (CV_8UC1)
//
// How it works:
//   - expectedBoardSizePx() uses pinhole FOV to convert physical meters to pixels at distance Z.
//   - A simple contour-based detector finds the most plausible board quadrilateral near the expected area & aspect.
//   - If within tolerances (size, aspect, angle, center offset) => matched.
//
// Usage:
//   auto w = new CalibBoardOverlayWidget;  
//   w->setDistanceMeters(1.2); // e.g., 1.2 m  
//   connect(source, &YourFrameSource::newFrame, w, &CalibBoardOverlayWidget::setFrameBGR);
//   connect(w, &CalibBoardOverlayWidget::recognitionChanged, [](bool ok){ qDebug() << "recognition" << ok; });
//   setCentralWidget(w);
//
// Tuning: call setTolerances() as needed.

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtCore/QPointer>
#include <QtCore/QDebug>
#include <cmath>
#include <algorithm>
#include <qmath.h>

#include <opencv2/opencv.hpp>

class CalibBoardDetector
{
public:
	struct Params {
		double hfov_deg = 38.0;
		double vfov_deg = 21.0;
		int img_w = 1280;
		int img_h = 720;
		// Physical board dimensions in meters
		double board_w_m = 0.40; // 40 cm
		double board_h_m = 0.90; // 90 cm

		// Tolerances
		double aspect_rel_tol = 0.18; // б└18% on aspect ratio
		double area_rel_tol = 0.35; // б└35% on area
		double angle_tol_deg = 12.0; // within 12бу of axis-aligned
		double center_tol_rel = 0.08; // center offset <= 8% of min(img_w, img_h)

		// Preprocessing / contours
		int canny_low = 50;
		int canny_high = 150;
		double approx_eps_rel = 0.02; // approxPolyDP epsilon as fraction of perimeter
		double min_area_rel = 0.02;   // reject quads < 2% of expected board area
	};

	explicit CalibBoardDetector(const Params& p = Params()) : P(p) {}

	void setParams(const Params& p) { P = p; }
	const Params& params() const { return P; }

	// Compute expected board size in pixels at distance Z (meters) using FOV model.
	QSizeF expectedBoardSizePx(double Zm) const {
		Zm = std::max(0.001, Zm);
		const double hfov = P.hfov_deg * M_PI / 180.0;
		const double vfov = P.vfov_deg * M_PI / 180.0;
		const double view_w_m = 2.0 * Zm * std::tan(hfov / 2.0);
		const double view_h_m = 2.0 * Zm * std::tan(vfov / 2.0);
		const double px_per_m_h = static_cast<double>(P.img_w) / view_w_m;
		const double px_per_m_v = static_cast<double>(P.img_h) / view_h_m;
		const double w_px = P.board_w_m * px_per_m_h;
		const double h_px = P.board_h_m * px_per_m_v;
		return QSizeF(w_px, h_px);
	}

	struct Detection {
		bool matched = false;
		cv::RotatedRect bestRect;  // in input image coordinates
		std::vector<cv::Point> bestContour;
		double score = 1e9;        // lower is better
		// diagnostics
		double aspect_err = 0, area_err = 0, angle_err = 0, center_err = 0;
		QSizeF expected_px; // expected board size at given Z
	};

	Detection detect(const cv::Mat& bgr_or_gray, double Zm) const {
		Detection D;
		D.expected_px = expectedBoardSizePx(Zm);
		const double expArea = D.expected_px.width() * D.expected_px.height();
		const double expAspect = std::min(P.board_w_m, P.board_h_m) / std::max(P.board_w_m, P.board_h_m); // 0.40/0.90 б╓ 0.444

		if (bgr_or_gray.empty()) return D;

		cv::Mat gray;
		if (bgr_or_gray.type() == CV_8UC1) gray = bgr_or_gray;
		else cv::cvtColor(bgr_or_gray, gray, cv::COLOR_BGR2GRAY);

		cv::Mat blur;
		cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);

		cv::Mat edges;
		cv::Canny(blur, edges, P.canny_low, P.canny_high);

		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		const cv::Point2f imgCenter(P.img_w * 0.5f, P.img_h * 0.5f);
		const double diag = std::sqrt(P.img_w * 1.0 * P.img_w + P.img_h * 1.0 * P.img_h);

		for (auto& c : contours) {
			const double peri = cv::arcLength(c, true);
			if (peri <= 1.0) continue;
			std::vector<cv::Point> approx;
			cv::approxPolyDP(c, approx, P.approx_eps_rel * peri, true);
			if (approx.size() != 4) continue;
			if (!cv::isContourConvex(approx)) continue;

			cv::RotatedRect rr = cv::minAreaRect(approx);
			double w = rr.size.width;
			double h = rr.size.height;
			if (w <= 1 || h <= 1) continue;

			// Size & area check
			const double area = w * h;
			if (area < P.min_area_rel * std::max(1.0, expArea)) continue;

			// Aspect ratio (min/max vs expected)
			const double aspect = std::min(w, h) / std::max(w, h);
			const double aspect_err = std::abs(aspect - expAspect) / expAspect;
			if (aspect_err > (P.aspect_rel_tol * 2.0)) continue; // keep loose in search; final check later

			// Angle (prefer axis-aligned due to camera facing)
			// Compute edge orientation using the longest edge of approx
			auto edgeAngleDeg = [](const std::vector<cv::Point>& poly) {
				double bestLen = 0; double bestAng = 0;
				for (int i = 0; i < 4; ++i) {
					cv::Point2f a = poly[i];
					cv::Point2f b = poly[(i + 1) % 4];
					cv::Point2f v = b - a;
					double len = std::hypot(v.x, v.y);
					if (len > bestLen) {
						bestLen = len;
						bestAng = std::atan2(v.y, v.x) * 180.0 / M_PI; // degrees to +x
					}
				}
				return bestAng; // [-180,180]
			};
			double ang = edgeAngleDeg(approx);
			auto angNorm = [](double a) {
				// distance to nearest multiple of 90бу
				double a0 = std::fmod(std::abs(a), 180.0);
				double d0 = std::fmod(a0, 90.0);
				return std::min(d0, 90.0 - d0);
			};
			double angle_err = angNorm(ang); // degrees, 0 is axis-aligned

			// Center offset
			double cx = rr.center.x, cy = rr.center.y;
			double centerDist = std::hypot(cx - imgCenter.x, cy - imgCenter.y);
			double center_err = centerDist / (std::min(P.img_w, P.img_h)); // relative

			// Area error (relative to expected)
			double area_err = std::abs(area - expArea) / std::max(1.0, expArea);

			// Aggregate score
			double score = aspect_err * 1.5 + area_err * 1.0 + (angle_err / P.angle_tol_deg) * 1.0 + (center_err / P.center_tol_rel) * 1.0;

			if (score < D.score) {
				D.score = score;
				D.bestRect = rr;
				D.bestContour = approx;
				D.aspect_err = aspect_err;
				D.area_err = area_err;
				D.angle_err = angle_err;
				D.center_err = center_err;
			}
		}

		if (!D.bestContour.empty()) {
			// Final decision against tolerances
			bool ok = (D.aspect_err <= P.aspect_rel_tol) &&
				(D.area_err <= P.area_rel_tol) &&
				(D.angle_err <= P.angle_tol_deg) &&
				(D.center_err <= P.center_tol_rel);
			D.matched = ok;
		}
		return D;
	}

private:
	Params P;
};

class CalibBoardOverlayWidget : public QWidget
{
	Q_OBJECT
public:
	explicit CalibBoardOverlayWidget(QWidget* parent = nullptr)
		: QWidget(parent)
	{
		setMinimumSize(320, 180);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		// Default detector params already set for 1280x720 & given FOVs
	}

	// If your camera resolution differs, call this once.
	void setImageSize(int w, int h) {
		detParams.img_w = w; detParams.img_h = h; detector.setParams(detParams);
	}

	void setFOV(double hfov_deg, double vfov_deg) {
		detParams.hfov_deg = hfov_deg; detParams.vfov_deg = vfov_deg; detector.setParams(detParams);
		update();
	}

	void setBoardSizeMeters(double w_m, double h_m) {
		detParams.board_w_m = w_m; detParams.board_h_m = h_m; detector.setParams(detParams);
		update();
	}

	void setTolerances(double aspect_rel_tol, double area_rel_tol, double angle_tol_deg, double center_tol_rel) {
		detParams.aspect_rel_tol = aspect_rel_tol;
		detParams.area_rel_tol = area_rel_tol;
		detParams.angle_tol_deg = angle_tol_deg;
		detParams.center_tol_rel = center_tol_rel;
		detector.setParams(detParams);
	}

	// Distance in meters (controls the overlay rectangle size)
	void setDistanceMeters(double Zm) {
		distance_m = std::max(0.001, Zm);
		update();
	}

	double distanceMeters() const { return distance_m; }

public slots:
	// Accept BGR8 cv::Mat
	void setFrameBGR(const cv::Mat& frame) {
		if (frame.empty()) return;
		// Ensure size params match
		if (detParams.img_w != frame.cols || detParams.img_h != frame.rows) {
			setImageSize(frame.cols, frame.rows);
		}
		currentBGR = frame.clone();
		// For display, convert to RGB QImage
		cv::Mat rgb;
		if (currentBGR.channels() == 3) {
			cv::cvtColor(currentBGR, rgb, cv::COLOR_BGR2RGB);
			qimg = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
		}
		else if (currentBGR.channels() == 1) {
			qimg = QImage(currentBGR.data, currentBGR.cols, currentBGR.rows, currentBGR.step, QImage::Format_Grayscale8).copy();
		}
		else {
			// Fallback: convert to 8UC3
			cv::Mat tmp;
			currentBGR.convertTo(tmp, CV_8UC3);
			cv::cvtColor(tmp, rgb, cv::COLOR_BGR2RGB);
			qimg = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
		}

		// Run detection on the original resolution
		lastDetection = detector.detect(currentBGR, distance_m);
		bool now = lastDetection.matched;
		if (now != lastMatched) {
			lastMatched = now;
			emit recognitionChanged(now);
		}
		update();
	}

signals:
	void recognitionChanged(bool matched);

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.fillRect(rect(), Qt::black);

		if (qimg.isNull()) return;

		// Compute target rect to keep aspect ratio
		QRectF target = fitAspectRect(qimg.size(), this->rect());
		p.drawImage(target, qimg);

		// Crosshair + expected rectangle
		QColor color = lastMatched ? QColor(0, 200, 0) : QColor(220, 0, 0);
		QPen pen(color, 2.0);
		p.setPen(pen);

		// Crosshair across the displayed image area
		const QPointF c = target.center();
		p.drawLine(QPointF(target.left(), c.y()), QPointF(target.right(), c.y()));
		p.drawLine(QPointF(c.x(), target.top()), QPointF(c.x(), target.bottom()));

		// Expected rectangle in widget coordinates
		const QSizeF expPx = detector.expectedBoardSizePx(distance_m);
		const double scaleX = target.width() / static_cast<double>(detParams.img_w);
		const double scaleY = target.height() / static_cast<double>(detParams.img_h);
		const double rw = expPx.width()  * scaleX;
		const double rh = expPx.height() * scaleY;
		QRectF expRect(c.x() - rw * 0.5, c.y() - rh * 0.5, rw, rh);
		p.drawRect(expRect);

		// Optionally draw the detected quadrilateral (dashed)
		if (!lastDetection.bestContour.empty()) {
			QPen dashPen(color, 2.0);
			QVector<qreal> dashes; dashes << 6 << 4; dashPen.setDashPattern(dashes);
			p.setPen(dashPen);
			auto toWidget = [&](const cv::Point& pt) {
				double x = target.left() + (pt.x * scaleX);
				double y = target.top() + (pt.y * scaleY);
				return QPointF(x, y);
			};
			const auto& poly = lastDetection.bestContour;
			for (size_t i = 0; i < poly.size(); ++i) {
				QPointF a = toWidget(poly[i]);
				QPointF b = toWidget(poly[(i + 1) % poly.size()]);
				p.drawLine(a, b);
			}
		}

		// Tiny HUD info (top-left)
		p.setPen(Qt::white);
		p.setBrush(Qt::NoBrush);
		QFont f = p.font(); f.setPointSizeF(std::max(9.0, width()*0.015)); p.setFont(f);
		const QString hud = QString("Z= %1 m | expected= %2 x %3 px | %4")
			.arg(distance_m, 0, 'f', 3)
			.arg(detector.expectedBoardSizePx(distance_m).width(), 0, 'f', 1)
			.arg(detector.expectedBoardSizePx(distance_m).height(), 0, 'f', 1)
			.arg(lastMatched ? "MATCH" : "NO MATCH");
		p.drawText(target.adjusted(8, 8, -8, -8), Qt::AlignLeft | Qt::AlignTop, hud);
	}

private:
	QRectF fitAspectRect(const QSize& src, const QRect& dst) const {
		if (src.isEmpty() || dst.isEmpty()) return QRectF(dst);
		const double sw = src.width();
		const double sh = src.height();
		const double dw = dst.width();
		const double dh = dst.height();
		const double sAspect = sw / sh;
		const double dAspect = dw / dh;
		double w, h, x, y;
		if (dAspect > sAspect) { // limited by height
			h = dh; w = dh * sAspect; x = dst.x() + (dw - w) / 2.0; y = dst.y();
		}
		else { // limited by width
			w = dw; h = dw / sAspect; x = dst.x(); y = dst.y() + (dh - h) / 2.0;
		}
		return QRectF(x, y, w, h);
	}

	CalibBoardDetector::Params detParams; // initialized to defaults
	CalibBoardDetector detector{ detParams };

	cv::Mat currentBGR;
	QImage qimg;
	double distance_m = 1.0; // default

	CalibBoardDetector::Detection lastDetection;
	bool lastMatched = false;
};

// ---- Optional minimal demo (compile in your test app) ----
// #include <QtWidgets/QApplication>
// #include <QtWidgets/QMainWindow>
// int main(int argc, char** argv){
//     QApplication app(argc, argv);
//     QMainWindow win; auto w = new CalibBoardOverlayWidget; win.setCentralWidget(w); win.resize(1000,600); win.show();
//     // Feed a test image periodically or from your camera callback:
//     // cv::Mat frame = ...; w->setFrameBGR(frame); w->setDistanceMeters(1.2);
//     return app.exec();
// }

// Notes:
// * If your capture thread is separate, emit a signal with the cv::Mat and connect to setFrameBGR using Qt::QueuedConnection.
// * If your image stream is grayscale, setFrameBGR accepts it as CV_8UC1 as well.
// * Tolerances are conservative; refine them after a few real captures.
