
#pragma once
#include <QObject>
#include <QRectF>
#include <QVector>
#include <QPointF>
#include <opencv2/opencv.hpp>

class ChessboardFinder : public QObject {
    Q_OBJECT
public:
    explicit ChessboardFinder(QObject *parent=nullptr);
    void setPatternSize(int cols, int rows);
    void setFlags(int cvFlags);
    void setSubpix(bool on, int win=5, int maxIter=20, double eps=0.01);
    void setDownscale(int factor);
    bool find(const cv::Mat &frameBGRorGray, QRectF &outRect, QVector<QPointF> &outCorners);
private:
    cv::Size patternSize_ = {13, 6};
    int flags_ = cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FAST_CHECK;
    bool useSubpix_ = true; int subpixWin_ = 5; int subpixMaxIter_ = 20; double subpixEps_ = 0.01;
    int downscale_ = 1;
};
