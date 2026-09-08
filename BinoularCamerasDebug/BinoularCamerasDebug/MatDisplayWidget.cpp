#include "MatDisplayWidget.h"

MatDisplayWidget::MatDisplayWidget(QWidget* parent)
	: QLabel(parent)
{
	setScaledContents(false); // 不强制填满控件，避免拉伸
	setAlignment(Qt::AlignCenter); // 图像居中显示
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setMinimumSize(100, 100);
}

void MatDisplayWidget::setImage(const cv::Mat& mat)
{
	_qimage = cvMatToQImage(mat);
	updateScaledPixmap();
}

void MatDisplayWidget::resizeEvent(QResizeEvent* event)
{
	QLabel::resizeEvent(event);
	updateScaledPixmap();
}

void MatDisplayWidget::updateScaledPixmap()
{
	if (!_qimage.isNull()) {
		QPixmap pixmap = QPixmap::fromImage(_qimage);
		QSize labelSize = this->size();
		QPixmap scaled = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		setPixmap(scaled);
	}
}

QImage MatDisplayWidget::cvMatToQImage(const cv::Mat& mat)
{
	if (mat.empty()) return QImage();

	switch (mat.type()) {
	case CV_8UC3: {
		cv::Mat rgb;
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
		return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
	}
	case CV_8UC1:
		return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
	default:
		return QImage(); // 不支持其他格式
	}
}
