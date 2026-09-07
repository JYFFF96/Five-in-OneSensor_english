#pragma once

// MatView.h — QWidget 控件：自适应显示 cv::Mat，叠加“单个”十字线与按距离计算的方框
// 依赖：Qt 5.12 + OpenCV 3.4.x

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QMutex>
#include <QMetaType>

#include <opencv2/core.hpp>
#include "common.h"
#include <QVector>
#include <QRectF>
#include <QSize>
#include <QMap>
#include <QPolygonF>   // 头部 include 区域补充

// 可共享的车道线几何（图像坐标）
struct LaneGeometry {
	QPolygonF leftLineImg;    // 左车道线折线
	QPolygonF rightLineImg;   // 右车道线折线
	QPolygonF laneRegionImg;  // 车道填充区域（闭合多边形）
	float laneWidth_m = 3.5f;
	float maxDist_m = 30.f;
	int   version = 1;
};

// 若需跨线程通过信号传递 cv::Mat：在 main.cpp 里注册
// qRegisterMetaType<cv::Mat>("cv::Mat");
Q_DECLARE_METATYPE(cv::Mat)

class MatView : public QWidget {
	Q_OBJECT
		Q_PROPERTY(bool   keepAspect     READ keepAspect     WRITE setKeepAspect)
		Q_PROPERTY(bool   smoothScale    READ smoothScale    WRITE setSmoothScale)
		Q_PROPERTY(QColor background     READ background     WRITE setBackground)
		Q_PROPERTY(double distanceMeters READ distanceMeters WRITE setDistanceMeters)
		Q_PROPERTY(bool   drawCrosshair  READ drawCrosshair  WRITE setDrawCrosshair)
		Q_PROPERTY(bool   drawBox        READ drawBox        WRITE setDrawBox)
public:
	enum class OverlayColor { Red, Green };
	Q_ENUM(OverlayColor)

		explicit MatView(QWidget* parent = nullptr);

public slots:
	// 显示一帧图像（CV_8UC1 / CV_8UC3(BGR) / CV_8UC4(BGRA)）
	void setFrame(const cv::Mat& mat);
	// 若已有 QImage，可直接传入
	void setImage(const QImage& img);
	// 清空内容
	void clear();
	// 梯形阶段：小十字线上下移动（单位：图像像素；默认每次1px）
	void moveSmallCrossUp();
	void moveSmallCrossDown();

	// 车道线阶段：大十字线 + 车道线整体左右移动（单位：图像像素；默认每次1px）
	void moveBigCrossLeft();
	void moveBigCrossRight();
	// 可选：一键归零
	void resetCrossOffsets();

	// 接收外部实例发布的车道几何，并保存其 sourceSize（外部几何的原图坐标系）
	void setExternalLaneGeometry(const LaneGeometry& geom, const QSize& sourceSize);
	// 是否使用外部车道线几何（默认 false：保持旧行为，使用本实例内部车道参数）
	void setUseExternalLaneGeometry(bool on);
	bool useExternalLaneGeometry() const;

public: // 参数与外观控制
	bool keepAspect() const { return m_keepAspect; }
	void setKeepAspect(bool on);

	bool smoothScale() const { return m_smooth; }
	void setSmoothScale(bool on);

	QColor background() const { return m_bg; }
	void setBackground(const QColor& c);

	// 叠加层：距离/尺寸/FOV/颜色
	void   setDistanceMeters(double d);
	double distanceMeters() const { return m_distance_m; }

	void setTargetSizeMeters(double width_m, double height_m); // 默认 0.40 x 0.90（米）
	void setFov(double hfov_deg, double vfov_deg);             // 默认 38° / 21°
	void setOverlayColor(OverlayColor c);                      // 默认红色

	// 叠加显示开关（默认不绘制）
	bool drawCrosshair() const { return m_drawCross; }
	void setDrawCrosshair(bool on);

	bool drawBox() const { return m_drawBox; }
	void setDrawBox(bool on);

	// === 新增：绘制开关 ===
	void setDrawGroundBox(bool on);
	void setDrawLaneLines(bool on);


	// === 地面梯形参数（单位：米）===
	// worldYSign: +1 表示地面 Y = +cameraHeight；-1 表示 Y = -cameraHeight
	void setGroundRegion(float Znear, float Zfar, float Wnear, float Wfar, int worldYSign = +1, int thicknessPx = 2, const QColor& color = QColor(0, 255, 0));

	// === 车道线参数 ===
	// useCarWidth=true→两条线基于车辆宽度；false→使用 laneWidthMeters（默认3.5m）
	void setLaneParams(bool useCarWidth, float laneWidthMeters = 3.5f, float maxDistanceMeters = 30.0f, int thicknessPx = 2, const QColor& color = QColor(255, 0, 0));

	void setMiddleLineVisible(bool enable);   // 控制是否显示中间横线

	// =========障碍物相关的接口========================
	void setDrawObstacles(bool on);
	bool drawObstacles() const { return m_drawObstacles; }

	void setObstaclesDistanceOnly(bool on); // true=仅“ID/Z/X”文字；false=再画框
	bool obstaclesDistanceOnly() const { return m_obstaclesDistanceOnly; }

	// 每帧更新目标列表（boxes/距离等以 sourceSize 为坐标系）
	void setObstacleDetections(const QVector<ObstacleDet>& dets, const QSize& sourceSize);

	// 按类型自定义颜色（可选）
	void setObstacleClassColor(ObstacleType type, const QColor& color);

	// 右上角综合信息（speedX/speedZ/HMW/TTC）开关 + 目标ID
	void setObstacleSummaryVisible(bool on);
	void setObstacleSummaryTargetId(int id); // -1 关闭

	 // 仅显示“车道范围内”的障碍物（默认 false：保持旧行为）
	void setObstaclesOnlyInsideLane(bool on);
	bool obstaclesOnlyInsideLane() const;

	// 手动计算并发送一次（成功返回 true；失败返回 false）
	bool sendLaneGeometry();

	// 可选：开启/关闭自动发送（默认 false = 不在 paintEvent 里发）
	void setAutoEmitLaneGeometry(bool on) { m_autoEmitLaneGeometry = on; }
	bool autoEmitLaneGeometry() const { return m_autoEmitLaneGeometry; }

signals:
	// 实时坐标上报（以“图像左上角”为 (0,0)）
	void smallCrossYChanged(int y_img);        // 小十字线的 Y 坐标（图像像素）
	void bigCrossXChanged(int x_img);          // 大十字线交叉点的 X 坐标（图像像素）
	// 由“发布者实例”在内部更新/绘制车道线后发出（A→B）
	void laneGeometryUpdated(const LaneGeometry& geom, const QSize& sourceSize); 

protected:
	void paintEvent(QPaintEvent* ev) override;
	void resizeEvent(QResizeEvent* ev) override;

private:
	static QImage matToQImage(const cv::Mat& mat);
	void rescale();

private:
	// 图像缓存
	QImage m_image;   // 原始 QImage（由 cv::Mat 转来）
	QImage m_scaled;  // 已按窗口尺寸缩放好的缓存

	// 显示参数
	QColor m_bg = Qt::black;
	bool   m_keepAspect = true;
	bool   m_smooth = true;
	QMutex m_mutex;

	// 相机与目标参数（用于按距离换算方框尺寸）
	double m_hfov_deg = 38.0;  // 水平 FOV（度）
	double m_vfov_deg = 21.0;  // 垂直 FOV（度）
	double m_target_w_m = 0.40;  // 目标物理宽（米）= 40 cm
	double m_target_h_m = 0.90;  // 目标物理高（米）= 90 cm
	double m_distance_m = 1.0;   // 距离（米），外部实时更新

	OverlayColor m_overlayColor = OverlayColor::Red;

	// 叠加显示开关（默认 false：不绘制）
	bool m_drawCross = false;
	bool m_drawBox = false;

	// —— 新增成员 —— //
	// GroundBox
	bool   m_drawGroundBox = false;
	float  m_Znear = 4.5f, m_Zfar = 35.f;     // m
	float  m_Wnear = 1.8f, m_Wfar = 2.8f;    // m
	int    m_worldYSign = +1;                // +1 / -1
	int    m_groundThick = 2;
	QColor m_groundColor = QColor(0, 255, 0);

	// LaneLines
	bool   m_drawLaneLines = false;
	bool   m_laneUseCarWidth = true;
	float  m_laneWidth = 3.5f;               // m（当 useCarWidth=false）
	float  m_laneMaxDist = 30.f;             // m
	int    m_laneThick = 2;
	QColor m_laneColor = QColor(255, 0, 0);

	// 小十字线（梯形阶段）
	bool   m_smallCrossEnabled = true;     // 内部开关
	int    m_smallCrossYOffset = 0;        // 以“图像像素”为单位（>0 向下）
	QPointF m_crossImgCached = QPointF(qQNaN(), qQNaN()); // 小十字线的图像坐标（缓存给车道线阶段）
	int    m_lastSmallY = std::numeric_limits<int>::min(); // 去重上报

	// 大十字线（车道线阶段）
	bool   m_bigCrossEnabled = true;       // 内部开关
	int    m_bigCrossXOffset = 0;          // 以“图像像素”为单位（>0 向右）
	int    m_lastBigX = std::numeric_limits<int>::min();   // 去重上报

	bool m_drawMiddleLine = false;            // 默认不画

	//========障碍物相关========
	bool m_drawObstacles = false;            // 1) 默认不绘制
	bool m_obstaclesDistanceOnly = true;     // 2) 默认仅显示左上角 ID/X/Z
	QVector<ObstacleDet> m_obstacles;        // 最近一帧的目标
	QSize  m_obsSrcSize;                     // 这些bbox的原坐标系尺寸（Obstacle帧宽高）
	QMap<ObstacleType, QColor> m_clsColor = { // 3) 类型上色（按你图例）
		{ObstacleType::Pedestrian, QColor(255, 0, 255)},   // Magenta
		{ObstacleType::Cyclist,    QColor(255,165,  0)},   // Orange
		{ObstacleType::Vehicle,    QColor(0,255, 255)},  // Cyan-ish
		{ObstacleType::Other,      QColor(128,255,170)}    // Teal/Green
	};

	// 4) 右上角综合信息（默认不显示）
	bool m_showObstacleSummary = false;
	int  m_summaryId = -1;                   // 目标ID；-1 表示未指定

	// --- 共享车道几何（复用）---
	bool m_useExternalLane = false;
	LaneGeometry m_externalLaneGeom;
	QSize        m_laneGeomSrcSize; // 外部几何的原坐标系尺寸

	// --- 障碍物过滤：仅显示车道内 ---
	bool m_obstaclesOnlyInsideLane = false;
	bool m_autoEmitLaneGeometry = false; // 控制是否在 paintEvent 自动发送（默认关闭）

};