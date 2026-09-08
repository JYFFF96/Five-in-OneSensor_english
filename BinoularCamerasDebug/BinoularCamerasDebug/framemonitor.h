#ifndef FRAMEMONITOR_H
#define FRAMEMONITOR_H

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>

#include <opencv2/core.hpp>
#include "camerahandler.h"
#include "common.h"

class FrameMonitor : public CameraHandler
{
public:
    FrameMonitor();
    virtual ~FrameMonitor() {}

    // image handler func in recv thread of SATP Protocol(based on tcp)
    void handleRawFrame(const RawImageFrame *rawFrame);

    // custom function for processing recieved frame data in handleRawFrame()
    void processFrame(const RawImageFrame *rawFrame);

    // the draw function must be called in main thread loop!!!
    void waitForFrames();

    cv::Mat getFrameMat(int frameId);

	uint32_t getSpeed();

	void processObsFrame(const RawImageFrame *raw, QVector<ObstacleDet>& out, QSize& sourceSize);

	void processFrame1(int frameId, const unsigned char *image, int64_t time, int width, int height, int frameFormat);

	QVector<ObstacleDet> getObstacleDetVec();
	QSize getSrcSize();

private:
	void test(int frameId, char *image, char *extended, int64_t time, int width, int height);

protected:
    void loadFrameData2Mat(const RawImageFrame *frameData, cv::Mat &dstMat);

private:
    std::mutex mMutex;
    std::condition_variable mFrameReadyCond;
    bool mFrameReadyFlag;

    cv::Mat mLeftMat;
    cv::Mat mRightMat;
	cv::Mat mDisparityMat;
	cv::Mat mCompoundMat;


    std::unique_ptr<unsigned char[]> mRgbBuffer;
	uint32_t speed;
	QVector<ObstacleDet> mObstacleDetVec;
	QSize mSrcSize;
};

#endif // FRAMEMONITOR_H
