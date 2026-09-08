#ifndef POINTCLOUDFILTERMANAGER_H
#define POINTCLOUDFILTERMANAGER_H

#include <QObject>
#include <QStringList>
#include <QList>
#include "common.h"  // 这里写你的 PointData 定义头文件

// PCL 头文件
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/bilateral.h>
#include <pcl/surface/mls.h>
#include <pcl/search/kdtree.h>
// 其它 PCL 头可按需加

// 滤波器设置结构体
struct FilterSettings {
    QString name;

    // 直通滤波器
    bool passX = false;
    double passXMin = 0.0;
    double passXMax = 0.0;
    bool passY = false;
    double passYMin = 0.0;
    double passYMax = 0.0;
    bool passZ = false;
    double passZMin = 0.0;
    double passZMax = 0.0;

    // 体素滤波器
    double voxelSize = 0.01;

    // 统计滤波器
    int statMeanK = 10;
    double statStddevMulThresh = 1.0;

    // 半径滤波器
    double radius = 0.1;
    int radiusMinNeighbors = 5;

    // 高斯滤波器
    double gaussianSigmaS = 1.0;
    double gaussianSigmaR = 1.0;

    // 双边滤波器
    double bilateralSigmaS = 1.0;
    double bilateralSigmaR = 1.0;

    // 频率滤波器
    int freqWindow = 10;
    double freqThreshold = 0.5;
};

class PointCloudFilterManager : public QObject
{
    Q_OBJECT
public:
    explicit PointCloudFilterManager(QObject *parent = nullptr);

    // 设置滤波器列表
    void setFilterList(const QList<FilterSettings> &filters);

    // 应用滤波器链，返回滤波后的结果
    QList<PointXYZI> applyFilters(const QList<PointXYZI> &inputPoints);

private:
    QList<FilterSettings> filterList;  // 当前配置的滤波器链
};

#endif // POINTCLOUDFILTERMANAGER_H
