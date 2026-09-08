#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
struct PointXYZI {
    float x; // X坐标
    float y; // Y坐标
    float z; // z坐标
    float intensity; // 反射强度
};

struct PointData {
    double time; // 时间
    double x; // X坐标
    double y; // Y坐标
    double z; // z坐标
    float azimuthValue; //方位角
    double distance; // 距离
    float intensity; // 反射强度
    int channel; // 通道
};

enum ExportType {
    CSV,
    PCAP,
    LAS,
    PCD,
    UnknownType
};

enum FrameType {
    currentFrame,
    allFrames,
    rangeFrame,
    UnknownFrame
};

enum FileType {
    singleFile,
    perFrameFile,
    UnknownFile
};

struct ExportParams {
    ExportType exportType;
    FrameType frameType;   // 当前帧 / 所有帧 / 范围
    int startFrame;
    int endFrame;
    FileType fileType;    // 单个文件 / 每帧文件
    QString filePath;
};

// 添加你的包围盒结构体
struct AABB {
    QVector3D minCorner;
    QVector3D maxCorner;
};


struct Obstacle {
    pcl::PointXYZ min_point;
    pcl::PointXYZ max_point;
    pcl::PointXYZ centroid;
    Eigen::Vector3f obb_position;
    Eigen::Vector3f obb_dimensions;
    Eigen::Matrix3f obb_rotation;
    int point_count;
};

struct InternetInfo {
    QString lidarIp;
    QString destPCIp;
    QString macAddr;
    quint16 dataPort;
    quint16 devInfoPort;
};

// 时间同步信息
struct TimeSyncInfo {
    quint8 type; // 0x00：GPS 同步；0x01：E2E-L4 同步；0x02：P2P 同步；0x03：gPTP同步；0x04：E2E-L2 同步。
    quint8 status; // 0x00：未同步；0x01：GPS 同步成功；0x02：PTP 同步成功。
};

struct DifopInfo {
    quint16 speed;
    InternetInfo netInfo;
    quint16 fov_start;
    quint16 fov_end;
    quint16 mot_phase; //电机锁相相位
    TimeSyncInfo timeSyncInfo;
    quint8 returnMode; // 回波模式 返回模式
    quint8 rainMode;
};
extern QVector<float> COR_VERT_ANG;

class common
{
public:
    common();
};
void detectObstaclesWithOBB(
    pcl::PointCloud<pcl::PointXYZ>::Ptr inputCloud,
    float voxelLeafSize,
    float clusterTolerance,
    int minClusterSize,
    int maxClusterSize);
#endif // COMMON_H
