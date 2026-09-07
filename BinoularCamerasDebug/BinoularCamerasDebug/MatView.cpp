#include "MatView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMutexLocker>
#include <QtMath>

#include <opencv2/imgproc.hpp>
#include <qdebug.h>
#include <QPainterPath>

static inline QPointF scalePoint(const QPointF& p, const QSize& fromSz, const QSize& toSz) {
	if (fromSz.isEmpty() || toSz.isEmpty()) return p;
	return QPointF(p.x() * double(toSz.width()) / double(fromSz.width()),
		p.y() * double(toSz.height()) / double(fromSz.height()));
}
static inline QPolygonF scalePolygon(const QPolygonF& poly, const QSize& fromSz, const QSize& toSz) {
	QPolygonF out; out.reserve(poly.size());
	for (const auto& pt : poly) out << scalePoint(pt, fromSz, toSz);
	return out;
}

static inline cv::Matx34f makeProj34(const RotationMatrix& rot) {
	const float* m = rot.real3DToImage;
	return cv::Matx34f(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11]);
}

static inline QPointF projectWorldToPixel(const cv::Matx34f& M, const cv::Point3f& Pw) {
	cv::Vec4f X(Pw.x, Pw.y, Pw.z, 1.f);
	cv::Vec3f UVW = M * X;
	float w = UVW[2];
	if (std::abs(w) < 1e-6f) return QPointF(-1e9, -1e9);
	return QPointF(UVW[0] / w, UVW[1] / w); // 图像像素坐标（以原图像分辨率为基准）
}

// 图像像素 → QWidget坐标（基于 contentRect() 的等比缩放）
static inline QPoint mapImgPtToWidget(const QPointF& pImg, const QRect& contentRect, const QSize& imgSize) {
	double s = (double)contentRect.width() / imgSize.width();
	QPointF t(contentRect.left() + pImg.x()*s, contentRect.top() + pImg.y()*s);
	return QPoint(qRound(t.x()), qRound(t.y()));
}

// 像素行 y -> 地面距离 Z（取中线 X=0, 地面 Y=H）
static inline float solveZforRow(const cv::Matx34f& M, float H, float y) {
	// 注意：Matx34f 是行优先 M(r,c)
	float m11 = M(1, 1), m12 = M(1, 2), m13 = M(1, 3);
	float m21 = M(2, 1), m22 = M(2, 2), m23 = M(2, 3);
	float den = (m12 - y * m22);
	if (std::abs(den) < 1e-9f) return std::numeric_limits<float>::quiet_NaN();
	float num = (y*m21 - m11)*H + (y*m23 - m13);
	return num / den;
}

MatView::MatView(QWidget* parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);   // 避免系统清背景
	setAutoFillBackground(false);
	setMinimumSize(80, 60);
}

void MatView::setFrame(const cv::Mat& mat) {
	if (mat.empty()) return;
	QImage img = matToQImage(mat);
	{
		QMutexLocker lock(&m_mutex);
		m_image = std::move(img);
	}
	rescale();
	update();
}

void MatView::setImage(const QImage& img) {
	if (img.isNull()) return;
	{
		QMutexLocker lock(&m_mutex);
		m_image = img.copy();
	}
	rescale();
	update();
}

void MatView::clear() {
	QMutexLocker lock(&m_mutex);
	m_image = QImage();
	m_scaled = QImage();
	update();
}

void MatView::moveSmallCrossUp() {
	m_smallCrossYOffset -= 1;
	update(); // 重绘后会在 paintEvent 里发 smallCrossYChanged
}
void MatView::moveSmallCrossDown() {
	m_smallCrossYOffset += 1;
	update();
}

void MatView::moveBigCrossLeft() {
	m_bigCrossXOffset -= 1;
	update(); // 重绘后会在 paintEvent 里发 bigCrossXChanged
}
void MatView::moveBigCrossRight() {
	m_bigCrossXOffset += 1;
	update();
}

void MatView::resetCrossOffsets() {
	m_smallCrossYOffset = 0;
	m_bigCrossXOffset = 0;
	update();
}

void MatView::setKeepAspect(bool on) {
	m_keepAspect = on;
	rescale();
	update();
}

void MatView::setSmoothScale(bool on) {
	m_smooth = on;
	rescale();
	update();
}

void MatView::setBackground(const QColor& c) {
	m_bg = c;
	update();
}

void MatView::setDistanceMeters(double d) {
	if (d <= 0) return; // 防止非法值
	QMutexLocker lock(&m_mutex);
	m_distance_m = d;
	lock.unlock();
	update();
}

void MatView::setTargetSizeMeters(double width_m, double height_m) {
	if (width_m <= 0 || height_m <= 0) return;
	QMutexLocker lock(&m_mutex);
	m_target_w_m = width_m;
	m_target_h_m = height_m;
	lock.unlock();
	update();
}

void MatView::setFov(double hfov_deg, double vfov_deg) {
	if (hfov_deg <= 0 || vfov_deg <= 0) return;
	QMutexLocker lock(&m_mutex);
	m_hfov_deg = hfov_deg;
	m_vfov_deg = vfov_deg;
	lock.unlock();
	update();
}

void MatView::setOverlayColor(OverlayColor c) {
	if (m_overlayColor == c) {
		return;
	}
	QMutexLocker lock(&m_mutex);
	m_overlayColor = c;
	lock.unlock();
	update();
}

void MatView::setDrawCrosshair(bool on) {
	QMutexLocker lock(&m_mutex);
	m_drawCross = on;
	lock.unlock();
	update();
}

void MatView::setDrawBox(bool on) {
	QMutexLocker lock(&m_mutex);
	m_drawBox = on;
	lock.unlock();
	update();
}

void MatView::setDrawGroundBox(bool on)
{
	m_drawGroundBox = on; 
	update();
}

void MatView::setDrawLaneLines(bool on)
{
	m_drawLaneLines = on;
	update();
}

void MatView::setGroundRegion(float Znear, float Zfar, float Wnear, float Wfar, int worldYSign, int thicknessPx, const QColor & color)
{
	if (Znear > 0 && Zfar > Znear) { m_Znear = Znear; m_Zfar = Zfar; }
	if (Wnear > 0 && Wfar > 0) { m_Wnear = Wnear; m_Wfar = Wfar; }
	m_worldYSign = (worldYSign >= 0 ? +1 : -1);
	if (thicknessPx > 0) m_groundThick = thicknessPx;
	m_groundColor = color;
	update();
}

void MatView::setLaneParams(bool useCarWidth, float laneWidthMeters, float maxDistanceMeters, int thicknessPx, const QColor & color)
{
	m_laneUseCarWidth = useCarWidth;
	if (laneWidthMeters > 0)  m_laneWidth = laneWidthMeters;
	if (maxDistanceMeters > 0) m_laneMaxDist = maxDistanceMeters;
	if (thicknessPx > 0)      m_laneThick = thicknessPx;
	m_laneColor = color;
	update();
}

void MatView::setMiddleLineVisible(bool enable)
{
	if (m_drawMiddleLine != enable) {
		m_drawMiddleLine = enable;
		update(); // 触发重绘
	}
}

void MatView::setDrawObstacles(bool on)
{
	QMutexLocker lk(&m_mutex);
	if (m_drawObstacles == on) return;
	m_drawObstacles = on;
	lk.unlock();
	update();
}

void MatView::setObstaclesDistanceOnly(bool on)
{
	QMutexLocker lk(&m_mutex);
	if (m_obstaclesDistanceOnly == on) return;
	m_obstaclesDistanceOnly = on;
	lk.unlock();
	update();
}

void MatView::setObstacleDetections(const QVector<ObstacleDet>& dets, const QSize & sourceSize)
{
	QMutexLocker lk(&m_mutex);
	m_obstacles = dets;
	m_obsSrcSize = sourceSize.isValid() ? sourceSize : m_image.size();
	lk.unlock();
	update(); // 仅更新数据，不强制开启绘制
}

void MatView::setObstacleClassColor(ObstacleType type, const QColor & color)
{
	QMutexLocker lk(&m_mutex);
	m_clsColor[type] = color;
	lk.unlock();
	update();
}

void MatView::setObstacleSummaryVisible(bool on)
{
	QMutexLocker lk(&m_mutex);
	if (m_showObstacleSummary == on) return;
	m_showObstacleSummary = on;
	lk.unlock();
	update();
}

void MatView::setObstacleSummaryTargetId(int id)
{
	QMutexLocker lk(&m_mutex);
	m_summaryId = id;
	lk.unlock();
	update();
}


void MatView::paintEvent(QPaintEvent* /*ev*/) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.fillRect(rect(), m_bg);

	// ==== 先绘制图像 ====
	QImage toDraw;
	{
		QMutexLocker lock(&m_mutex);
		toDraw = m_scaled.isNull() ? m_image : m_scaled;
	}

	QRect contentRect = rect(); // 图像内容区域（考虑纵横比时的内边距）
	if (!toDraw.isNull()) {
		if (!m_keepAspect) {
			p.drawImage(rect(), toDraw);
		}
		else {
			const int x = (width() - toDraw.width()) / 2;
			const int y = (height() - toDraw.height()) / 2;
			contentRect = QRect(QPoint(x, y), toDraw.size());
			p.drawImage(contentRect.topLeft(), toDraw);
		}
	}

	// ==== 叠加颜色 ====
	QColor color = Qt::red;
	bool drawCross, drawBox;
	double hfov_deg, vfov_deg, target_w_m, target_h_m, distance_m;
	{
		QMutexLocker lock(&m_mutex);
		if (m_overlayColor == OverlayColor::Green) color = Qt::green;
		drawCross = m_drawCross;
		drawBox = m_drawBox;
		hfov_deg = m_hfov_deg;  vfov_deg = m_vfov_deg;
		target_w_m = m_target_w_m; target_h_m = m_target_h_m;
		distance_m = m_distance_m;
	}

	QPen pen(color);
	pen.setWidth(2);
	p.setPen(pen);

	// ==== 绘制十字线（单个，居中充满整个控件）====
	if (drawCross) {
		const QPoint center(width() / 2, height() / 2);
		p.drawLine(QPoint(0, center.y()), QPoint(width(), center.y()));
		p.drawLine(QPoint(center.x(), 0), QPoint(center.x(), height()));
	}

	// ==== 绘制方框：按 FOV + 距离换算像素尺寸，居中落在内容区域 ====
	if (drawBox && distance_m > 0 && contentRect.width() > 0 && contentRect.height() > 0) {
		const double hfov_rad = qDegreesToRadians(hfov_deg);
		const double vfov_rad = qDegreesToRadians(vfov_deg);

		// 距离 D 处的可视宽/高（米）
		const double world_w_at_D = 2.0 * distance_m * std::tan(hfov_rad / 2.0);
		const double world_h_at_D = 2.0 * distance_m * std::tan(vfov_rad / 2.0);

		// 每米对应的像素（以显示后的“图像内容区域”像素计）
		const double px_per_m_x = contentRect.width() / world_w_at_D;
		const double px_per_m_y = contentRect.height() / world_h_at_D;

		const double box_w_px = target_w_m * px_per_m_x;
		const double box_h_px = target_h_m * px_per_m_y;

		const QPointF c = contentRect.center();
		const QRectF box(c.x() - box_w_px / 2.0,
			c.y() - box_h_px / 2.0,
			box_w_px,
			box_h_px);
		p.drawRect(box);
	}

	const QSize imgSize = m_image.size();

	// 2) 投影矩阵与地面 Y
	const cv::Matx34f M = makeProj34(g_rotationMatrix);
	const float H = (m_worldYSign >= 0 ? +g_cameraInfoParams.cameraHeight
		: -g_cameraInfoParams.cameraHeight);

	QPointF p3 = projectWorldToPixel(M, { +m_Wfar * 0.5f, H, m_Zfar });
	QPointF p4 = projectWorldToPixel(M, { -m_Wfar * 0.5f, H, m_Zfar });
	// 3) 地面梯形（空心）
	if (m_drawGroundBox) {
		QPointF p1 = projectWorldToPixel(M, { -m_Wnear * 0.5f, H, m_Znear });
		QPointF p2 = projectWorldToPixel(M, { +m_Wnear * 0.5f, H, m_Znear });
		if (p1.x() > -1e8 && p2.x() > -1e8 && p3.x() > -1e8 && p4.x() > -1e8) {
			QPen pen(m_groundColor); pen.setWidth(m_groundThick); p.setPen(pen);
			QPoint q1 = mapImgPtToWidget(p1, contentRect, imgSize);
			QPoint q2 = mapImgPtToWidget(p2, contentRect, imgSize);
			QPoint q3 = mapImgPtToWidget(p3, contentRect, imgSize);
			QPoint q4 = mapImgPtToWidget(p4, contentRect, imgSize);
			p.drawLine(q1, q2); p.drawLine(q2, q3); p.drawLine(q3, q4); p.drawLine(q4, q1);
		}
		// 注意：p3/p4 是远端上边两个顶点（图像像素坐标，尚未映射）
		QPointF topMidImg = (p3 + p4) * 0.5;
		topMidImg.setY(topMidImg.y() + m_smallCrossYOffset); // 上下微调（像素，>0 向下）

		// 缓存：供车道线阶段作为“交点锚”
		m_crossImgCached = topMidImg;

		// 上报 Y（限制在图像范围内）
		int y_img = qBound(0, qRound(topMidImg.y()), imgSize.height() - 1);
		if (y_img != m_lastSmallY) { m_lastSmallY = y_img; emit smallCrossYChanged(y_img); }
		QPointF topMidDisp = topMidImg;
		topMidDisp.setX(topMidDisp.x() + m_bigCrossXOffset);
		// 画一个小十字（±4 像素）
		QPoint topMidW = mapImgPtToWidget(topMidImg, contentRect, imgSize);
		QPen crossPen(Qt::green); crossPen.setWidth(2);
		p.setPen(crossPen);
		const int k = 4;
		p.drawLine(QPoint(topMidW.x() - k, topMidW.y()), QPoint(topMidW.x() + k, topMidW.y()));
		p.drawLine(QPoint(topMidW.x(), topMidW.y() - k), QPoint(topMidW.x(), topMidW.y() + k));
	}

	// 4) 车道线（两侧透视直线）
	// === 车道线（贴底，远端保留到交点前的gap，不与交点相交）===
	// === 车道线：近端贴底，远端距离交点 gapPx（像素） ===
	if (m_drawLaneLines) {
		const cv::Matx34f M = makeProj34(g_rotationMatrix);
		const float H = (m_worldYSign >= 0 ? +g_cameraInfoParams.cameraHeight
			: -g_cameraInfoParams.cameraHeight);
		const QRect  cr = contentRect;
		const QSize  szImg = m_image.size();
		const double scale = double(cr.width()) / szImg.width();

		auto img2widget = [&](const QPointF& pImg) {
			return QPointF(cr.left() + pImg.x()*scale, cr.top() + pImg.y()*scale);
		};
		auto projectImg = [&](float X, float Z)->QPointF {
			cv::Vec4f Xw(X, H, Z, 1.f);
			cv::Vec3f UVW = M * Xw; float w = UVW[2];
			if (std::abs(w) < 1e-6f) return QPointF(-1e9, -1e9);
			return QPointF(UVW[0] / w, UVW[1] / w); // 图像像素坐标
		};

		QPointF crossImg = m_crossImgCached;
		if (!std::isfinite(crossImg.x()) || !std::isfinite(crossImg.y()))
			crossImg = QPointF(szImg.width()*0.5, szImg.height()*0.5);
		crossImg.setX(crossImg.x() + m_bigCrossXOffset);

		const int gapToCrossPx = 80;
		const int nearLiftPx = 0;

		float y_near = float(szImg.height() - 1 - nearLiftPx);
		float y_far = float(std::min<int>(szImg.height() - 1, int(crossImg.y()) + gapToCrossPx));

		float Znear = solveZforRow(M, H, y_near);
		float Zfar = solveZforRow(M, H, y_far);
		if (!std::isfinite(Znear) || Znear < 0.3f) Znear = 0.3f;
		if (!std::isfinite(Zfar) || Zfar <= Znear) Zfar = Znear + 0.5f;

		float halfW = (m_laneUseCarWidth ? g_cameraInfoParams.carWidth : m_laneWidth) * 0.5f;

		const float nearWidenScale = 1.25f;
		halfW *= nearWidenScale;

		// （如果你的 p3/p4 在别处定义，这里保留你的自适应放大逻辑即可）
		// float topY = ((p3.y() + p4.y()) * 0.5f);
		// float dy = (m_crossImgCached.y() - topY);
		// float k = 0.10f / 20.0f;
		// float factor = std::clamp(1.0f + (-dy) * k, 0.7f, 1.4f);
		// halfW *= factor;

		halfW = std::max(0.7f, halfW);
		if (!std::isfinite(Zfar) || Zfar <= Znear) Zfar = Znear + 10.0f;

		// —— 新增：收集左右两侧“图像坐标”的近/远端端点
		QPointF leftNearImg, leftFarImg, rightNearImg, rightFarImg;
		bool leftOK = false, rightOK = false;

		auto drawSegment = [&](int side, QPointF* outNearImg, QPointF* outFarImg)->bool {
			QPointF pNearImg = projectImg(side * halfW, Znear);
			if (pNearImg.x() < -1e8) return false;

			const double denom = (crossImg.y() - pNearImg.y());
			QPointF pFarImg;
			if (std::abs(denom) < 1e-6) {
				pFarImg = QPointF((pNearImg.x() + crossImg.x()) * 0.5, y_far);
			}
			else {
				const double t = (y_far - pNearImg.y()) / denom; // 0..1
				const double x = pNearImg.x() + t * (crossImg.x() - pNearImg.x());
				pFarImg = QPointF(x, y_far);
			}

			// —— 绘制（保持你原逻辑）
			QLineF seg(img2widget(pNearImg), img2widget(pFarImg));
			QLineF edges[4] = {
				QLineF(cr.topLeft(),     cr.topRight()),
				QLineF(cr.topRight(),    cr.bottomRight()),
				QLineF(cr.bottomRight(), cr.bottomLeft()),
				QLineF(cr.bottomLeft(),  cr.topLeft())
			};
			QVector<QPointF> pts; pts.reserve(4);
			for (auto& e : edges) {
				QPointF ip;
				if (seg.intersect(e, &ip) == QLineF::BoundedIntersection) pts.push_back(ip);
			}
			if (cr.contains(seg.p1().toPoint())) pts.push_back(seg.p1());
			if (cr.contains(seg.p2().toPoint())) pts.push_back(seg.p2());
			if (pts.size() >= 2) {
				std::sort(pts.begin(), pts.end(), [&](const QPointF&a, const QPointF&b) {
					return QLineF(seg.p1(), a).length() < QLineF(seg.p1(), b).length();
					});
				QPen pen(m_laneColor); pen.setWidth(m_laneThick); p.setPen(pen);
				p.drawLine(pts.first(), pts.last());
			}

			if (outNearImg) *outNearImg = pNearImg;
			if (outFarImg)  *outFarImg = pFarImg;
			return true;
		};

		leftOK = drawSegment(-1, &leftNearImg, &leftFarImg); // 左
		rightOK = drawSegment(+1, &rightNearImg, &rightFarImg); // 右

		// —— 大十字线照旧
		{
			int x_img = qBound(0, qRound(crossImg.x()), szImg.width() - 1);
			if (x_img != m_lastBigX) { m_lastBigX = x_img; emit bigCrossXChanged(x_img); }
			QPointF crossW = img2widget(crossImg);
			QPen hPen(Qt::green);  hPen.setWidth(2); p.setPen(hPen);
			p.drawLine(QPointF(cr.left(), crossW.y()), QPointF(cr.right(), crossW.y()));
			QPen vPen(Qt::yellow); vPen.setWidth(2); p.setPen(vPen);
			p.drawLine(QPointF(crossW.x(), cr.top()), QPointF(crossW.x(), cr.bottom()));
		}

		// ====== 在这里：构造 LaneGeometry 并发出（图像坐标）======
		if (leftOK && rightOK) {
			// 两条线的折线（你当前是两点成线；后续如果采样多点就 push 多点即可）
			QPolygonF leftPtsImg;  leftPtsImg << leftNearImg << leftFarImg;
			QPolygonF rightPtsImg; rightPtsImg << rightNearImg << rightFarImg;

			// 车道区域四边形（闭合）
			QPolygonF laneRegion;
			laneRegion << leftNearImg << leftFarImg << rightFarImg << rightNearImg << leftNearImg;

			// （可选）去重：只有几何变了才发
			static QPolygonF lastRegion;
			if (laneRegion != lastRegion) {
				lastRegion = laneRegion;

				LaneGeometry geom;
				geom.leftLineImg = leftPtsImg;
				geom.rightLineImg = rightPtsImg;
				geom.laneRegionImg = laneRegion;
				geom.laneWidth_m = (m_laneUseCarWidth ? g_cameraInfoParams.carWidth : m_laneWidth);
				geom.maxDist_m = m_laneMaxDist;

				if (m_autoEmitLaneGeometry) {
					emit laneGeometryUpdated(geom, szImg);
				}
			}
		}
	}

	// ==== 绘制中间横线 ====
	if (m_drawMiddleLine) {
		QPen linePen(Qt::yellow);
		linePen.setWidth(2);
		p.setPen(linePen);

		int y = height() / 2; // 图像中间
		p.drawLine(QPoint(0, y), QPoint(width(), y));
	}

	// ==== 障碍物叠加：最近目标红框，其他按类别色；左上角始终 ID/X/Z；右上角按总开关 ====
	{
		QVector<ObstacleDet> obs;
		QSize   srcSize, baseImgSz;
		bool    drawObs = false, showSummary = false;
		QMap<ObstacleType, QColor> classColors;
		{
			QMutexLocker lk(&m_mutex);
			drawObs = m_drawObstacles;
			showSummary = m_showObstacleSummary;  // true 则每个框右上角都画 Vx/Vz/HMW/TTC
			obs = m_obstacles;
			srcSize = m_obsSrcSize.isValid() ? m_obsSrcSize : m_image.size();
			baseImgSz = m_image.size();
			classColors = m_clsColor;
		}

		if (drawObs && !obs.isEmpty() && !baseImgSz.isEmpty() && contentRect.isValid()) {
			// 两段映射：sourceSize -> 原图尺寸 -> contentRect（兼容 KeepAspect/IgnoreAspect）
			const double kx = srcSize.width() > 0 ? double(baseImgSz.width()) / srcSize.width() : 1.0;
			const double ky = srcSize.height() > 0 ? double(baseImgSz.height()) / srcSize.height() : 1.0;
			const double sX = baseImgSz.width() > 0 ? double(contentRect.width()) / baseImgSz.width() : 1.0;
			const double sY = baseImgSz.height() > 0 ? double(contentRect.height()) / baseImgSz.height() : 1.0;
			const QPoint origin(contentRect.left(), contentRect.top());

			// ① 找到“Z 距离最近”的索引（仅考虑 Z>0 且有限）
			int   nearestIdx = -1;
			float bestZ = std::numeric_limits<float>::infinity();
			for (int i = 0; i < obs.size(); ++i) {
				const float z = obs[i].z_m;
				if (std::isfinite(z) && z > 0.f && z < bestZ) { bestZ = z; nearestIdx = i; }
			}

			// 文本工具
			QFont f = p.font(); f.setPointSizeF(f.pointSizeF() + 1.0); p.setFont(f);
			auto drawLabelBG_L = [&](const QString& text, const QPoint& tl) {
				QFontMetricsF fm(f);
				QRectF r = fm.boundingRect(text).adjusted(-4, -2, 4, 2);
				QRect  box(tl, QSize(qRound(r.width()), qRound(r.height())));
				p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 160)); p.drawRect(box);
				p.setPen(Qt::white); p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
			};
			auto drawLabelBG_TR = [&](const QString& text, const QPoint& tr) {
				QFontMetricsF fm(f);
				QRectF r = fm.boundingRect(text).adjusted(-6, -3, 6, 3);
				int w = qRound(r.width()), h = qRound(r.height());
				QRect box(tr.x() - w, tr.y(), w, h);
				p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 160)); p.drawRect(box);
				p.setPen(Qt::white); p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, text);
				return box;
			};

			for (int i = 0; i < obs.size(); ++i) {
				const auto& d = obs[i];

				// 源坐标 -> 原图 -> contentRect
				QRectF rImg(d.bbox.left()*kx, d.bbox.top()*ky, d.bbox.width()*kx, d.bbox.height()*ky);
				QRectF rW(origin.x() + rImg.left()*sX,
					origin.y() + rImg.top() *sY,
					rImg.width()*sX, rImg.height()*sY);

				// ② 最近目标 → 红色；否则类别色
				QColor col = (i == nearestIdx) ? QColor(255, 0, 0)
					: classColors.value(d.type, QColor(0, 255, 0));

				QPen boxPen(col,
					/*线宽*/ 5.0  /*若你已做接口：改成 m_obstacleBoxThick*/);
				boxPen.setCosmetic(true);
				p.setPen(boxPen); p.setBrush(Qt::NoBrush);
				p.drawRect(rW);

				// 左上角：ID + X + Z（始终）
				const QString tl = QString("ID:%1  X:%2 m  Z:%3 m")
					.arg(d.id).arg(d.x_m, 0, 'f', 1).arg(d.z_m, 0, 'f', 1);
				drawLabelBG_L(tl, (rW.topLeft() + QPointF(1, 1)).toPoint());

				// 右上角：Vx/Vz/HMW/TTC（当总开关开启时，对每个框都画）
				if (showSummary) {
					QString l1 = QString("Vz:%1 m/s   Vx:%2 m/s")
						.arg(d.vz_mps, 0, 'f', 1).arg(d.vx_mps, 0, 'f', 1);
					QRect box1 = drawLabelBG_TR(l1, (rW.topRight() + QPointF(-1, 1)).toPoint());

					auto fmt = [](float v)->QString { return std::isfinite(v) ? QString::number(v, 'f', 1) : QStringLiteral("—"); };
					QString l2 = QString("HMW:%1 s   TTC:%2 s").arg(fmt(d.hmw_s), fmt(d.ttc_s));
					QPoint tr2(box1.topRight().x(), box1.bottom() + 4);
					drawLabelBG_TR(l2, tr2);
				}
			}
		}
	}
	// ==== 障碍物叠加结束 ====
}

void MatView::resizeEvent(QResizeEvent* /*ev*/) {
	rescale();
}

QImage MatView::matToQImage(const cv::Mat& mat) {
	if (mat.empty()) return {};

	switch (mat.type()) {
	case CV_8UC1: {
		// 8位灰度
		QImage img(mat.data, mat.cols, mat.rows,
			static_cast<int>(mat.step), QImage::Format_Grayscale8);
		return img.copy();
	}
	case CV_8UC3: {
		// BGR -> RGB
		cv::Mat rgb;
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
		QImage img(rgb.data, rgb.cols, rgb.rows,
			static_cast<int>(rgb.step), QImage::Format_RGB888);
		return img.copy();
	}
	case CV_8UC4: {
		// BGRA -> RGBA
		cv::Mat rgba;
		cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
		QImage img(rgba.data, rgba.cols, rgba.rows,
			static_cast<int>(rgba.step), QImage::Format_RGBA8888);
		return img.copy();
	}
	default: {
		// 其它位深/通道：归一化转 8 位再转 RGB
		cv::Mat tmp8, rgb;
		if (mat.channels() == 1) {
			double minv, maxv; cv::minMaxLoc(mat, &minv, &maxv);
			mat.convertTo(tmp8, CV_8UC1, maxv > minv ? 255.0 / (maxv - minv) : 1.0, -minv);
			cv::cvtColor(tmp8, rgb, cv::COLOR_GRAY2RGB);
		}
		else {
			mat.convertTo(tmp8, CV_8UC3);
			cv::cvtColor(tmp8, rgb, cv::COLOR_BGR2RGB);
		}
		QImage img(rgb.data, rgb.cols, rgb.rows,
			static_cast<int>(rgb.step), QImage::Format_RGB888);
		return img.copy();
	}
	}
}

void MatView::rescale() {
	QMutexLocker lock(&m_mutex);
	if (m_image.isNull()) { m_scaled = QImage(); return; }
	const Qt::TransformationMode mode = m_smooth ? Qt::SmoothTransformation
		: Qt::FastTransformation;
	const QSize target = size();
	if (target.isEmpty()) { m_scaled = QImage(); return; }
	m_scaled = m_image.scaled(target,
		m_keepAspect ? Qt::KeepAspectRatio
		: Qt::IgnoreAspectRatio,
		mode);
}

void MatView::setUseExternalLaneGeometry(bool on) {
	if (m_useExternalLane == on) return;
	m_useExternalLane = on;
	update();
}
bool MatView::useExternalLaneGeometry() const { return m_useExternalLane; }

void MatView::setExternalLaneGeometry(const LaneGeometry& geom, const QSize& sourceSize) {
	m_externalLaneGeom = geom;
	m_laneGeomSrcSize = sourceSize;

	update();
}

void MatView::setObstaclesOnlyInsideLane(bool on) {
	if (m_obstaclesOnlyInsideLane == on) return;
	m_obstaclesOnlyInsideLane = on;
	update();
}
bool MatView::obstaclesOnlyInsideLane() const { return m_obstaclesOnlyInsideLane; }

bool MatView::sendLaneGeometry()
{
	if (!m_drawLaneLines || m_image.isNull()) return false;

	const cv::Matx34f M = makeProj34(g_rotationMatrix);
	const float H = (m_worldYSign >= 0 ? +g_cameraInfoParams.cameraHeight
		: -g_cameraInfoParams.cameraHeight);

	const QSize szImg = m_image.size();

	auto projectImg = [&](float X, float Z)->QPointF {
		cv::Vec4f Xw(X, H, Z, 1.f);
		cv::Vec3f UVW = M * Xw; float w = UVW[2];
		if (std::abs(w) < 1e-6f) return QPointF(-1e9, -1e9);
		return QPointF(UVW[0] / w, UVW[1] / w); // 图像像素坐标
	};

	// 交点（小十字 + 大十字偏移）
	QPointF crossImg = m_crossImgCached;
	if (!std::isfinite(crossImg.x()) || !std::isfinite(crossImg.y()))
		crossImg = QPointF(szImg.width()*0.5, szImg.height()*0.5);
	crossImg.setX(crossImg.x() + m_bigCrossXOffset);

	const int gapToCrossPx = 80;
	const int nearLiftPx = 0;

	float y_near = float(szImg.height() - 1 - nearLiftPx);
	float y_far = float(std::min<int>(szImg.height() - 1, int(crossImg.y()) + gapToCrossPx));

	float Znear = solveZforRow(M, H, y_near);
	float Zfar = solveZforRow(M, H, y_far);
	if (!std::isfinite(Znear) || Znear < 0.3f) Znear = 0.3f;
	if (!std::isfinite(Zfar) || Zfar <= Znear) Zfar = Znear + 0.5f;

	float halfW = (m_laneUseCarWidth ? g_cameraInfoParams.carWidth : m_laneWidth) * 0.5f;
	const float nearWidenScale = 1.25f;
	halfW *= nearWidenScale;
	halfW = std::max(0.7f, halfW);
	if (!std::isfinite(Zfar) || Zfar <= Znear) Zfar = Znear + 10.0f;

	auto endPoints = [&](int side, QPointF& nearImg, QPointF& farImg)->bool {
		nearImg = projectImg(side * halfW, Znear);
		if (nearImg.x() < -1e8) return false;
		const double denom = (crossImg.y() - nearImg.y());
		if (std::abs(denom) < 1e-6) {
			farImg = QPointF((nearImg.x() + crossImg.x()) * 0.5, y_far);
		}
		else {
			const double t = (y_far - nearImg.y()) / denom; // 0..1
			const double x = nearImg.x() + t * (crossImg.x() - nearImg.x());
			farImg = QPointF(x, y_far);
		}
		return true;
	};

	QPointF ln, lf, rn, rf;
	if (!endPoints(-1, ln, lf) || !endPoints(+1, rn, rf)) return false;

	QPolygonF leftPtsImg;  leftPtsImg << ln << lf;
	QPolygonF rightPtsImg; rightPtsImg << rn << rf;
	QPolygonF laneRegion;  laneRegion << ln << lf << rf << rn << ln;

	LaneGeometry geom;
	geom.leftLineImg = leftPtsImg;
	geom.rightLineImg = rightPtsImg;
	geom.laneRegionImg = laneRegion;
	geom.laneWidth_m = (m_laneUseCarWidth ? g_cameraInfoParams.carWidth : m_laneWidth);
	geom.maxDist_m = m_laneMaxDist;

	emit laneGeometryUpdated(geom, szImg);
	return true;
}
