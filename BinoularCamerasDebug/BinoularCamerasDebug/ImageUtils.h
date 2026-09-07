
#pragma once
#include <QImage>
#include <opencv2/opencv.hpp>

namespace ImageUtils {
static inline QImage matToQImage(const cv::Mat &m){
    if (m.empty()) return QImage();
    switch (m.type()) {
        case CV_8UC3: {
                #if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
                QImage img(m.data, m.cols, m.rows, m.step, QImage::Format_BGR888);
                return img.copy();
                #else
                cv::Mat rgb; cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
                QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
                return img.copy();
                #endif
            }
        case CV_8UC1: {
            QImage img(m.data, m.cols, m.rows, m.step, QImage::Format_Grayscale8);
            return img.copy();
        }
        default: {
            cv::Mat tmp; m.convertTo(tmp, CV_8U);
            QImage img(tmp.data, tmp.cols, tmp.rows, tmp.step, QImage::Format_Grayscale8);
            return img.copy();
        }
    }
}
} // namespace ImageUtils
