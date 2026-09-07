
#define PCL_NO_PRECOMPILE

#include "pointcloudfiltermanager.h"

#include <pcl/point_types.h>
#include <pcl/point_cloud.h>

#include <pcl/filters/passthrough.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/radius_outlier_removal.h>
#include <pcl/filters/bilateral.h>
#include <pcl/surface/mls.h>
#include <pcl/search/kdtree.h>
#include <pcl/filters/fast_bilateral_omp.h>
#include <QDebug>


PointCloudFilterManager::PointCloudFilterManager(QObject *parent)
    : QObject(parent)
{
}

void PointCloudFilterManager::setFilterList(const QList<FilterSettings> &filters)
{
    filterList = filters;
}

QList<PointXYZI> PointCloudFilterManager::applyFilters(const QList<PointXYZI>& inputPoints)
{
    qDebug() << "Raw Data:" << ", Remaining points:" << inputPoints.size();
    QList<PointXYZI> filteredPoints = inputPoints;

    for (const FilterSettings& setting : filterList)
    {
        if (filteredPoints.isEmpty()) break;
        if (setting.name == "Pass-Through Filter")
        {
            QList<PointXYZI> tempPoints;

            for (const PointXYZI& pt : filteredPoints)
            {
                bool pass = true;

                if (setting.passX)
                {
                    if (pt.x < setting.passXMin || pt.x > setting.passXMax)
                        pass = false;
                }

                if (setting.passY)
                {
                    if (pt.y < setting.passYMin || pt.y > setting.passYMax)
                        pass = false;
                }

                if (setting.passZ)
                {
                    if (pt.z < setting.passZMin || pt.z > setting.passZMax)
                        pass = false;
                }

                if (pass)
                {
                    tempPoints.append(pt);
                }
            }

            filteredPoints = tempPoints;
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
        // 体素滤波段
        else if (setting.name == "Voxel Filter")
        {

            pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloudFiltered(new pcl::PointCloud<pcl::PointXYZI>());

            int badCount = 0;

            for (const PointXYZI& pt : filteredPoints)
            {
                if (std::isnan(pt.x) || std::isnan(pt.y) || std::isnan(pt.z) ||
                    std::isinf(pt.x) || std::isinf(pt.y) || std::isinf(pt.z))
                {
                    ++badCount;
                    continue;
                }

                pcl::PointXYZI p;
                p.x = pt.x;
                p.y = pt.y;
                p.z = pt.z;
                p.intensity = pt.intensity;
                cloud->points.push_back(p);
            }

            qDebug() << "Voxel Filter: Input Points:" << filteredPoints.size()
                     << " Valid Points:" << cloud->points.size()
                     << " Invalid Points(NaN/Inf):" << badCount;

            if (cloud->points.empty())
            {
                qDebug() << "Voxel Filter: No valid input points，skipping processing";
                filteredPoints.clear();
                return filteredPoints;
            }

            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = true;

            pcl::VoxelGrid<pcl::PointXYZI> voxel;
            voxel.setInputCloud(cloud);
            voxel.setLeafSize(setting.voxelSize, setting.voxelSize, setting.voxelSize);
            voxel.setMinimumPointsNumberPerVoxel(1);

            try
            {
                voxel.filter(*cloudFiltered);
            }
            catch (const std::exception& ex)
            {
                qDebug() << "Voxel Filter: Exception:" << ex.what();
                filteredPoints.clear();
                return filteredPoints;
            }

            filteredPoints.clear();
            for (const auto& p : cloudFiltered->points)
            {
                PointXYZI pt;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                pt.intensity = p.intensity;
                filteredPoints.append(pt);
            }

            qDebug() << "Filter:" << setting.name
                     << ", After VoxelGrid, Remaining points:" << filteredPoints.size();
        }


        else if (setting.name == "Statistical Filter")
        {
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloudFiltered(new pcl::PointCloud<pcl::PointXYZI>());

            for (const PointXYZI& pt : filteredPoints)
            {
                pcl::PointXYZI p;
                p.x = pt.x;
                p.y = pt.y;
                p.z = pt.z;
                p.intensity = pt.intensity;
                cloud->points.push_back(p);
            }

            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = true;

            pcl::StatisticalOutlierRemoval<pcl::PointXYZI> sor;
            sor.setInputCloud(cloud);
            sor.setMeanK(setting.statMeanK);
            sor.setStddevMulThresh(setting.statStddevMulThresh);
            sor.filter(*cloudFiltered);

            filteredPoints.clear();
            for (const auto& p : cloudFiltered->points)
            {
                PointXYZI pt;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                pt.intensity = p.intensity;
                filteredPoints.append(pt);
            }
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
        else if (setting.name == "Radius Filter")
        {
            qDebug() << "Radius Filter Parameters: radius=" << setting.radius
                     << ", minNeighbors=" << setting.radiusMinNeighbors;
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloudFiltered(new pcl::PointCloud<pcl::PointXYZI>());

            for (const PointXYZI& pt : filteredPoints)
            {
                pcl::PointXYZI p;
                p.x = pt.x;
                p.y = pt.y;
                p.z = pt.z;
                p.intensity = pt.intensity;
                cloud->points.push_back(p);
            }

            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = true;

            pcl::RadiusOutlierRemoval<pcl::PointXYZI> radiusFilter;
            radiusFilter.setInputCloud(cloud);
            radiusFilter.setRadiusSearch(setting.radius);
            radiusFilter.setMinNeighborsInRadius(setting.radiusMinNeighbors);
            radiusFilter.filter(*cloudFiltered);

            filteredPoints.clear();
            for (const auto& p : cloudFiltered->points)
            {
                PointXYZI pt;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                pt.intensity = p.intensity;
                filteredPoints.append(pt);
            }
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
        else if (setting.name == "Gaussian Filter")
        {

            pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
            pcl::PointCloud<pcl::PointXYZI> cloudSmoothed;

            for (const PointXYZI& pt : filteredPoints)
            {
                pcl::PointXYZI p;
                p.x = pt.x;
                p.y = pt.y;
                p.z = pt.z;
                p.intensity = pt.intensity;
                cloud->points.push_back(p);
            }

            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = true;

            pcl::search::KdTree<pcl::PointXYZI>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZI>);

            pcl::MovingLeastSquares<pcl::PointXYZI, pcl::PointXYZI> mls;
            mls.setComputeNormals(false);
            mls.setInputCloud(cloud);
            mls.setPolynomialOrder(2);
            mls.setSearchMethod(tree);
            mls.setSearchRadius(setting.gaussianSigmaS);  // 作为平滑半径
            mls.process(cloudSmoothed);

            filteredPoints.clear();
            for (const auto& p : cloudSmoothed.points)
            {
                PointXYZI pt;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                pt.intensity = p.intensity;
                filteredPoints.append(pt);
            }
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
        else if (setting.name == "Bilateral Filter")
        {
            // 创建 cloud
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
            pcl::PointCloud<pcl::PointXYZI>::Ptr cloudFiltered(new pcl::PointCloud<pcl::PointXYZI>());

            // 统计 NaN / Inf 点
            int badCount = 0;

            for (const PointXYZI& pt : filteredPoints)
            {
                if (std::isnan(pt.x) || std::isnan(pt.y) || std::isnan(pt.z) ||
                    std::isinf(pt.x) || std::isinf(pt.y) || std::isinf(pt.z))
                {
                    ++badCount;
                    continue; // skip
                }

                pcl::PointXYZI p;
                p.x = pt.x;
                p.y = pt.y;
                p.z = pt.z;
                p.intensity = pt.intensity;
                cloud->points.push_back(p);
            }

            qDebug() << "Bilateral Filter: Input Points:" << filteredPoints.size()
                     << " Valid Points:" << cloud->points.size()
                     << " NaN/Inf Point Count:" << badCount;

            if (cloud->points.empty())
            {
                qDebug() << "Bilateral Filter: No valid input points，skipping processing";
                filteredPoints.clear();
                return filteredPoints;
            }

            cloud->width = cloud->points.size();
            cloud->height = 1;
            cloud->is_dense = true;

            // 打印参数
            qDebug() << "Bilateral Filter Parameters: SigmaS =" << setting.bilateralSigmaS
                     << ", SigmaR =" << setting.bilateralSigmaR;

            // 使用 FastBilateralFilterOMP
            pcl::FastBilateralFilterOMP<pcl::PointXYZI> bilateral;
            bilateral.setInputCloud(cloud);
            bilateral.setSigmaS(setting.bilateralSigmaS); // 推荐 0.05 ~ 0.1
            bilateral.setSigmaR(setting.bilateralSigmaR); // 推荐 2.0 ~ 5.0

            try
            {
                bilateral.filter(*cloudFiltered);
            }
            catch (const std::exception& ex)
            {
                qDebug() << "Bilateral Filter: Exception:" << ex.what();
                filteredPoints.clear();
                return filteredPoints;
            }

            // 回填结果
            float avgBeforeX = 0, avgAfterX = 0;
            for (const PointXYZI& pt : filteredPoints)
                avgBeforeX += pt.x;
            avgBeforeX /= filteredPoints.size();

            filteredPoints.clear();
            for (const auto& p : cloudFiltered->points)
            {
                PointXYZI pt;
                pt.x = p.x;
                pt.y = p.y;
                pt.z = p.z;
                pt.intensity = p.intensity;
                filteredPoints.append(pt);

                avgAfterX += pt.x;
            }
            avgAfterX /= filteredPoints.size();

            qDebug() << "Bilateral Filter: Coordinate mean change X: Before=" << avgBeforeX << ", After=" << avgAfterX;
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
        else if (setting.name == "Frequency Filter")
        {
            QList<PointXYZI> tempPoints;

            // 简单示例：剔除强度 < 阈值的点
            for (const PointXYZI& pt : filteredPoints)
            {
                if (pt.intensity >= setting.freqThreshold)
                {
                    tempPoints.append(pt);
                }
            }

            filteredPoints = tempPoints;
            qDebug() << "Filter:" << setting.name << ", Remaining points:" << filteredPoints.size();
        }
    }

    return filteredPoints;
}
