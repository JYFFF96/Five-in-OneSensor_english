
#include "ChessboardFinder.h"

ChessboardFinder::ChessboardFinder(QObject *parent) : QObject(parent) {}
void ChessboardFinder::setPatternSize(int cols, int rows){ patternSize_ = cv::Size(cols, rows); }
void ChessboardFinder::setFlags(int cvFlags){ flags_ = cvFlags; }
void ChessboardFinder::setSubpix(bool on, int win, int maxIter, double eps){ useSubpix_=on; subpixWin_=win; subpixMaxIter_=maxIter; subpixEps_=eps; }
void ChessboardFinder::setDownscale(int factor){ downscale_ = std::max(1, factor); }

bool ChessboardFinder::find(const cv::Mat &frameBGRorGray, QRectF &outRect, QVector<QPointF> &outCorners){
    outRect = QRectF(); outCorners.clear();
    if (frameBGRorGray.empty()) return false;
    cv::Mat gray;
    if (frameBGRorGray.channels()==3) cv::cvtColor(frameBGRorGray, gray, cv::COLOR_BGR2GRAY);
    else gray = frameBGRorGray;

    double scale = 1.0;
    cv::Mat src = gray;
    if (downscale_>1) {
        scale = 1.0 / downscale_;
        cv::resize(gray, src, cv::Size(), scale, scale, cv::INTER_AREA);
    }
    std::vector<cv::Point2f> corners;
    bool found = cv::findChessboardCorners(src, patternSize_, corners, flags_);
    if (!found) return false;
    if (useSubpix_) {
        cv::cornerSubPix(src, corners, cv::Size(subpixWin_,subpixWin_), cv::Size(-1,-1),
                        cv::TermCriteria(cv::TermCriteria::EPS+cv::TermCriteria::MAX_ITER, subpixMaxIter_, subpixEps_));
    }
    if (downscale_>1) for (auto &pt : corners) { pt.x /= scale; pt.y /= scale; }
    cv::Rect bb = cv::boundingRect(corners);
    outRect = QRectF(bb.x, bb.y, bb.width, bb.height);
    outCorners.reserve((int)corners.size());
    for (const auto &pt : corners) outCorners.push_back(QPointF(pt.x, pt.y));
    return true;
}
