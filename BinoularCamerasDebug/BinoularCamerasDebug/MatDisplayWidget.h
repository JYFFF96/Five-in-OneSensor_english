#pragma once

#include <QLabel>
#include <QImage>
#include <opencv2/opencv.hpp>

class MatDisplayWidget : public QLabel
{
	Q_OBJECT

public:
	explicit MatDisplayWidget(QWidget* parent = nullptr);

	// 设置图像：OpenCV 的 cv::Mat
	void setImage(const cv::Mat& mat);

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	QImage _qimage;  // 原始图像（转换后的）

	QImage cvMatToQImage(const cv::Mat& mat);
	void updateScaledPixmap();
};
