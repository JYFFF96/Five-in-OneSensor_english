#include "common.h"

QVector<float> COR_VERT_ANG = {
    -15.84f, -15.17f, -14.49f, -13.82f, -13.14f, -12.47f, -11.80f, -11.12f, -10.45f, -9.77f, -9.10f, -8.43f, -7.75f, -7.08f, -6.40f, -5.73f, -5.06f, -4.38f, -3.71f, -3.03f, -2.36f, -1.69f, -1.01f, -0.34f, 0.34f, 1.01f, 1.69f, 2.36f, 3.03f, 3.71f, 4.38f, 5.06f, 5.73f, 6.40f, 7.08f, 7.75f, 8.43f, 9.10f, 9.77f, 10.45f, 11.12f, 11.80f, 12.47f, 13.14f, 13.82f, 14.49f, 15.17f, 15.84f
};

common::common() {}
#include <QVector>
#include <QList>
#include <QVector3D>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

inline pcl::PointCloud<pcl::PointXYZ>::Ptr toPclCloud(const QList<PointXYZI>& points)
{
    auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>());
    cloud->reserve(points.size());
    for (const auto& p : points)
        cloud->emplace_back(p.x, p.y, p.z);
    return cloud;
}

inline pcl::PointCloud<pcl::PointXYZ>::Ptr toPclCloud(const QVector<QVector3D>& points)
{
    auto cloud = pcl::PointCloud<pcl::PointXYZ>::Ptr(new pcl::PointCloud<pcl::PointXYZ>());
    cloud->reserve(points.size());
    for (const auto& p : points)
        cloud->emplace_back(p.x(), p.y(), p.z());
    return cloud;
}

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/features/moment_of_inertia_estimation.h>
#include <pcl/common/common.h>
#include <QDebug>

// 适合 Qt 调用的固定类型函数
void detectObstaclesWithOBB(
    pcl::PointCloud<pcl::PointXYZ>::Ptr inputCloud,
    float voxelLeafSize,
    float clusterTolerance,
    int minClusterSize,
    int maxClusterSize)
{
    if (!inputCloud || inputCloud->empty()) {
        qDebug() << "Input point cloud is empty!";
        return;
    }

    pcl::PointXYZ minPt, maxPt;
    pcl::getMinMax3D(*inputCloud, minPt, maxPt);
    float dx = maxPt.x - minPt.x;
    float dy = maxPt.y - minPt.y;
    float dz = maxPt.z - minPt.z;
    float maxRange = std::max({dx, dy, dz});
    qDebug() << "Raw point-cloud range:"
             << "X:" << minPt.x << "~" << maxPt.x
             << "Y:" << minPt.y << "~" << maxPt.y
             << "Z:" << minPt.z << "~" << maxPt.z
             << " maxRange:" << maxRange;

    // =========================
    // 自动单位缩放 (避免 mm/m 导致 leafSize 异常)
    float scaleFactor = 1.0f;
    if (maxRange < 1.0f) {         // 可能是毫米
        scaleFactor = 1000.0f;
    } else if (maxRange > 1000.0f) { // 可能是千米
        scaleFactor = 0.001f;
    }
    if (scaleFactor != 1.0f) {
        for (auto &p : inputCloud->points) {
            p.x *= scaleFactor;
            p.y *= scaleFactor;
            p.z *= scaleFactor;
        }
        qDebug() << "Auto-scale point cloud scaleFactor=" << scaleFactor;
    }

    pcl::getMinMax3D(*inputCloud, minPt, maxPt);
    dx = maxPt.x - minPt.x;
    dy = maxPt.y - minPt.y;
    dz = maxPt.z - minPt.z;
    maxRange = std::max({dx, dy, dz});

    // =========================
    // 自适应滤波: 逐步减小 leafSize 保证点数
    float originalSize = inputCloud->size();
    float leafSize = maxRange / 50.0f; // 初始大体积
    if (leafSize < 0.01f) leafSize = 0.01f;
    if (leafSize > 1.0f)  leafSize = 1.0f;

    pcl::PointCloud<pcl::PointXYZ>::Ptr filteredCloud(new pcl::PointCloud<pcl::PointXYZ>);
    bool foundGoodLeaf = false;
    while (leafSize >= 0.01f) {
        pcl::VoxelGrid<pcl::PointXYZ> voxel;
        voxel.setInputCloud(inputCloud);
        voxel.setLeafSize(leafSize, leafSize, leafSize);
        voxel.filter(*filteredCloud);

        qDebug() << "Trying leafSize=" << leafSize << " Points after filtering:" << filteredCloud->size();
        if (filteredCloud->size() > std::max(1000.0f, originalSize * 0.1f)) {
            foundGoodLeaf = true;
            break;
        }
        leafSize /= 2.0f;
    }

    if (!foundGoodLeaf) {
        qDebug() << "No suitable leaf size; using raw point cloud";
        filteredCloud = inputCloud;
    }

    // =========================
    // 安全检查，避免传空到 KdTree
    if (!filteredCloud || filteredCloud->empty() || filteredCloud->size() < 5) {
        qDebug() << "Point cloud is empty or has too few points (" << filteredCloud->size() << "); skipping clustering";
        return;
    }

    // =========================
    // 欧式聚类
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    try {
        tree->setInputCloud(filteredCloud);
    } catch (std::exception &e) {
        qDebug() << "KdTree build error:" << e.what();
        return;
    }

    std::vector<pcl::PointIndices> clusterIndices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(clusterTolerance);
    ec.setMinClusterSize(minClusterSize);
    ec.setMaxClusterSize(maxClusterSize);
    ec.setSearchMethod(tree);
    ec.setInputCloud(filteredCloud);
    try {
        ec.extract(clusterIndices);
    } catch (std::exception &e) {
        qDebug() << "Cluster extraction error:" << e.what();
        return;
    }

    qDebug() << "Detected" << clusterIndices.size() << "clusters";

    // =========================
    // 计算 OBB
    int clusterId = 0;
    for (const auto &indices : clusterIndices) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZ>);
        for (int idx : indices.indices)
            cluster->points.push_back(filteredCloud->points[idx]);
        cluster->width = cluster->points.size();
        cluster->height = 1;
        cluster->is_dense = true;

        if (cluster->empty()) {
            qDebug() << "Cluster is empty; skipping OBB";
            continue;
        }

        pcl::MomentOfInertiaEstimation<pcl::PointXYZ> featureExtractor;
        featureExtractor.setInputCloud(cluster);
        featureExtractor.compute();

        pcl::PointXYZ minPointOBB, maxPointOBB, positionOBB;
        Eigen::Matrix3f rotationalMatrixOBB;
        featureExtractor.getOBB(minPointOBB, maxPointOBB, positionOBB, rotationalMatrixOBB);

        qDebug() << "Cluster #" << clusterId++
                 << " Center (" << positionOBB.x << "," << positionOBB.y << "," << positionOBB.z << ")"
                 << " Dimensions: L="
                 << (maxPointOBB.x - minPointOBB.x)
                 << " W=" << (maxPointOBB.y - minPointOBB.y)
                 << " H=" << (maxPointOBB.z - minPointOBB.z);
    }
}
