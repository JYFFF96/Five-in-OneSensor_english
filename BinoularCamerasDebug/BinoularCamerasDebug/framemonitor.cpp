#include "framemonitor.h"

#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include "satpext.h"
#include "frameid.h"
#include "frameformat.h"
#include "disparityconvertor.h"
#include "yuv2rgb.h"
#include "qdebug.h"

#include "frameext.h"
#include "roadwaypainter.h"
#include "obstaclepainter.h"


FrameMonitor::FrameMonitor()
    : mFrameReadyFlag(false),
      mRgbBuffer(new unsigned char[1280 * 720 * 3])
{
    // support gray or rgb

	mRightMat.create(720, 1280, CV_8UC1);
    mLeftMat.create(720, 1280 , CV_8UC3);
    mDisparityMat.create(720, 1280, CV_8UC3);
	mCompoundMat.create(720, 1280, CV_8UC3);
}

void FrameMonitor::handleRawFrame(const RawImageFrame *rawFrame)
{
    processFrame(rawFrame);
	//processFrame1(rawFrame->frameId, (const unsigned char*)rawFrame + sizeof(RawImageFrame),
	//	rawFrame->time, rawFrame->width, rawFrame->height, rawFrame->format);
}

void FrameMonitor::processFrame(const RawImageFrame *rawFrame)
{
	speed = rawFrame->speed;
	switch (rawFrame->frameId) {
	case FrameId::Disparity:
	{
		//onlyforFrameFormat::Disparity16,bitNum=5
		std::lock_guard<std::mutex>lock(mMutex);
		loadFrameData2Mat(rawFrame, mDisparityMat);
		//std::cout<<"updatedisparitymat"<<std::endl;
		mFrameReadyFlag = true;
		mFrameReadyCond.notify_one();
	}
	break;
	case FrameId::CalibLeftCamera:
	{
		std::lock_guard<std::mutex>lock(mMutex);
		loadFrameData2Mat(rawFrame, mLeftMat);
		//std::cout<<"updateleftmat"<<std::endl;
		mFrameReadyFlag = true;
		mFrameReadyCond.notify_one();
	}
	break;
	case FrameId::RightCamera:
	{
		std::lock_guard<std::mutex>lock(mMutex);
		loadFrameData2Mat(rawFrame, mRightMat);
		//std::cout<<"updaterightmat"<<std::endl;
		mFrameReadyFlag = true;
		mFrameReadyCond.notify_one();
	}
	break;
	case FrameId::Obstacle:
	{
		qDebug() << "--------Obstacle-";
		
		processObsFrame(rawFrame, mObstacleDetVec, mSrcSize);

	}
	case FrameId::Compound:
	{
		
	}
	}
}

void FrameMonitor::waitForFrames()
{
    std::unique_lock<std::mutex> lock(mMutex);
    mFrameReadyCond.wait_for(lock, std::chrono::milliseconds(240), [this]{
        return mFrameReadyFlag;
    });

    mFrameReadyFlag = false;
}

cv::Mat FrameMonitor::getFrameMat(int frameId)
{
    std::lock_guard<std::mutex> lock(mMutex);

    switch (frameId) {
    case FrameId::Disparity:
        return mDisparityMat.clone();
	case FrameId::RightCamera:
		return mRightMat.clone();
	case FrameId::CalibLeftCamera:
		return mLeftMat.clone();
    }
}

uint32_t FrameMonitor::getSpeed()
{
	return speed;
}

void FrameMonitor::processObsFrame(const RawImageFrame *raw, QVector<ObstacleDet>& out, QSize& sourceSize)
{
	if (!raw || raw->frameId != FrameId::Obstacle) return;

	// image 指向数据体开头（见官方 handleRawFrame 指针偏移示例）
	// image 布局: [int blockNum][int reserved][OutputObstacles array...]
	const char* image = reinterpret_cast<const char*>(raw) + sizeof(RawImageFrame); // :contentReference[oaicite:3]{index=3}
	const int* ip = reinterpret_cast<const int*>(image);
	const int blockNum = ip[0];                                                      // :contentReference[oaicite:4]{index=4}
	const OutputObstacles* obs = reinterpret_cast<const OutputObstacles*>(ip + 2);   // :contentReference[oaicite:5]{index=5}

	out.clear();
	out.reserve(std::max(0, blockNum));
	sourceSize = QSize(raw->width, raw->height);

	for (int i = 0; i < blockNum; ++i) {
		const auto& o = obs[i];

		// 1) 方框：用四角求包围矩形（更稳妥）
		const unsigned short xs[4] = { o.firstPointX, o.secondPointX, o.thirdPointX,  o.fourthPointX };
		const unsigned short ys[4] = { o.firstPointY, o.secondPointY, o.thirdPointY,  o.fourthPointY };
		const int minx = std::min(std::min(xs[0], xs[1]), std::min(xs[2], xs[3]));
		const int maxx = std::max(std::max(xs[0], xs[1]), std::max(xs[2], xs[3]));
		const int miny = std::min(std::min(ys[0], ys[1]), std::min(ys[2], ys[3]));
		const int maxy = std::max(std::max(ys[0], ys[1]), std::max(ys[2], ys[3]));
		QRectF bbox(minx, miny, std::max(1, maxx - minx), std::max(1, maxy - miny)); // :contentReference[oaicite:6]{index=6}

		// 2) 距离/类型/速度
		ObstacleDet d{};
		d.id = static_cast<int>(o.trackId);
		d.type = toObstacleType(o.obstacleType);             // :contentReference[oaicite:7]{index=7}
		d.z_m = o.avgDistanceZ;                             // Z 平均距离（m） :contentReference[oaicite:8]{index=8}
		d.x_m = o.real3DCenterX;                            // 中心 X（m）     :contentReference[oaicite:9]{index=9}
		d.vx_mps = o.fuzzyRelativeSpeedCenterX;              // 相对横向速度     :contentReference[oaicite:10]{index=10}
		d.vz_mps = o.fuzzyRelativeSpeedZ;                    // 相对纵向速度     :contentReference[oaicite:11]{index=11}
		d.bbox = bbox;

		// 3) HMW / TTC
		const float v_ego = std::max(o.currentSpeed, 0.1f);  // 自车速度 (m/s)   :contentReference[oaicite:12]{index=12}
		d.hmw_s = (o.currentSpeed > 0.5f && d.z_m > 0.f) ? (d.z_m / v_ego)
			: std::numeric_limits<float>::infinity();
		d.ttc_s = (o.fuzzyEstimationValid && o.fuzzyCollisionTimeZ > 0.f)
			? o.fuzzyCollisionTimeZ
			: std::numeric_limits<float>::infinity();  // SDK 已给 TTC     :contentReference[oaicite:13]{index=13}

		out.push_back(d);
	}
}

void FrameMonitor::processFrame1(int frameId, const unsigned char * image, int64_t time, int width, int height, int frameFormat)
{
	auto hasValidFloat = [&](const float* f, int n) {
		bool anyValid = false;
		float fmin = std::numeric_limits<float>::infinity();
		float fmax = -fmin;

		for (int i = 0; i < n; ++i) {
			float v = f[i];
			if (!std::isfinite(v)) continue; // 忽略 NaN/Inf

			if (v != 0.0f) {
				anyValid = true; // ✅ 检测到至少一个非零有效值
			}

			if (v < fmin) fmin = v;
			if (v > fmax) fmax = v;
		}

		qDebug() << "hasValidFloat =" << anyValid
			<< "min =" << fmin
			<< "max =" << fmax;
		return anyValid;
	};

	switch (frameId) {
		case FrameId::Disparity:
		{
			int bitNum = DisparityConvertor::getDisparityBitNum(frameFormat);
			static unsigned char* rgbBuf = new unsigned char[width*height * 3];
			static float* floatData = new float[width*height];
			
			DisparityConvertor::convertDisparity2FloatFormat(image, width, height, bitNum,
				floatData);
			DisparityConvertor::convertDisparity2RGB(floatData, width, height,
				0.0f, 75.0f, rgbBuf);
			hasValidFloat(floatData, width*height);
			
		}
		break;
	default:
		break;
	}
}

QVector<ObstacleDet> FrameMonitor::getObstacleDetVec()
{
	return mObstacleDetVec;
}

QSize FrameMonitor::getSrcSize()
{
	return mSrcSize;
}

void FrameMonitor::test(int frameId, char * image, char * extended, int64_t time, int width, int height)
{
	FrameDataExtHead *header = reinterpret_cast<FrameDataExtHead *>(extended);
	static unsigned char *rgbBuf = new unsigned char[width * height * 3];
	RoadwayPainter::imageGrayToRGB((unsigned char*)image, rgbBuf, width, height);
	bool bbb = ObstaclePainter::paintObstacle(header->data, rgbBuf, width, height, true, false);
	cv::Mat rgb(height, width, CV_8UC3, rgbBuf);
	cv::Mat bgr; 
	cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
	cv::imshow("Compound", bgr);
}

void FrameMonitor::loadFrameData2Mat(const RawImageFrame *frameData, cv::Mat &dstMat)
{
    int width = frameData->width;
    int height = frameData->height;
    const unsigned char *imageData = frameData->image;

    switch (frameData->format) {
    case FrameFormat::Disparity16:
    case FrameFormat::DisparityDens16:
    {
		static float *floatData = new float[width*height];
        DisparityConvertor::convertDisparity2FloatFormat(imageData, width, height, 5, floatData);
        DisparityConvertor::convertDisparity2RGB(floatData, width, height, 0, 45, mRgbBuffer.get());
        cv::Mat dispMat(height, width, CV_8UC3, mRgbBuffer.get());
        cv::resize(dispMat, dstMat, dstMat.size());
		
    }
        break;
    case FrameFormat::Gray:
    {
        cv::Mat grayMat(height, width, CV_8UC1, (void*)imageData);
        cv::resize(grayMat, dstMat, dstMat.size());
    }
        break;
    case FrameFormat::YUV422:
    {
        YuvToRGB::YCbYCr2Rgb(imageData, (char *)mRgbBuffer.get(), width, height);
        cv::Mat yuv422Mat(height, width, CV_8UC3, mRgbBuffer.get());
        cv::resize(yuv422Mat, dstMat, dstMat.size());
        cv::cvtColor(dstMat, dstMat, CV_RGB2BGR);
    }
        break;
    case FrameFormat::YUV422Plannar:
    {
        YuvToRGB::YCbYCrPlannar2Rgb(imageData, (char *)mRgbBuffer.get(), width, height);
        cv::Mat yuv422PlannarMat(height, width, CV_8UC3, mRgbBuffer.get());
        cv::resize(yuv422PlannarMat, dstMat, dstMat.size());
        cv::cvtColor(dstMat, dstMat, CV_RGB2BGR);
    }
        break;
    }
}
