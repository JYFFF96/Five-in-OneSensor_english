#include "SimplePointCloudView.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <unordered_map>
#include <algorithm>
using std::vector;
static inline QVector3D mul(const QMatrix3x3& M,const QVector3D& v){
    return { M(0,0)*v.x()+M(0,1)*v.y()+M(0,2)*v.z(),
            M(1,0)*v.x()+M(1,1)*v.y()+M(1,2)*v.z(),
            M(2,0)*v.x()+M(2,1)*v.y()+M(2,2)*v.z() };
}

// ---- 体素降采样（voxel<=0 跳过） ----
struct VKey { int x,y,z; };
struct VKeyHash { size_t operator()(const VKey& k) const {
        return (std::hash<long long>()(((long long)k.x<<42)^((long long)k.y<<21)^k.z)); }};
static bool operator==(const VKey&a,const VKey&b){ return a.x==b.x&&a.y==b.y&&a.z==b.z; }

static void voxelDownsample(const QList<PointXYZI>& in, float voxel, vector<QVector3D>& out){
    if (voxel<=0.f) { out.reserve(in.size()); for (auto& p: in) out.emplace_back(p.x,p.y,p.z); return; }
    std::unordered_map<VKey,QVector3D,VKeyHash> grid; grid.reserve(in.size());
    for (const auto& p: in){
        VKey k{ int(std::floor(p.x/voxel)), int(std::floor(p.y/voxel)), int(std::floor(p.z/voxel)) };
        if (!grid.count(k)) grid[k] = QVector3D(p.x,p.y,p.z);
    }
    out.reserve(grid.size()); for (auto& kv: grid) out.push_back(kv.second);
}

// ---- 简易 DBSCAN（O(N^2)，先跑通） ----
static inline float sqr(float x){ return x*x; }
#include <unordered_map>
#include <vector>
#include <queue>
#include <cmath>
#include <tuple>

struct Cluster { std::vector<int> idx; };

struct CellKey { int x,y,z; };
struct CellHash {
    size_t operator()(const CellKey& k) const noexcept {
        // 混合哈希，避免碰撞
        return ( (uint64_t)(uint32_t)k.x * 73856093u ) ^
               ( (uint64_t)(uint32_t)k.y * 19349663u ) ^
               ( (uint64_t)(uint32_t)k.z * 83492791u );
    }
};
inline bool operator==(const CellKey& a, const CellKey& b) noexcept {
    return a.x==b.x && a.y==b.y && a.z==b.z;
}
static std::vector<Cluster>
gridCluster(const std::vector<QVector3D>& pts,
            float eps, int minPts, int maxPts)
{
    const float eps2 = eps*eps;
    const float cell = eps;                 // 一个单元边长≈eps
    const int   N    = (int)pts.size();

    // 1) 把每个点丢进对应的栅格桶
    std::unordered_map<CellKey, std::vector<int>, CellHash> buckets;
    buckets.reserve(N*1.3);
    std::vector<CellKey> keys; keys.reserve(N);

    auto toKey = [&](const QVector3D& p)->CellKey {
        return { (int)std::floor(p.x()/cell),
                (int)std::floor(p.y()/cell),
                (int)std::floor(p.z()/cell) };
    };
    for (int i=0;i<N;++i) {
        CellKey k = toKey(pts[i]);
        keys.push_back(k);
        buckets[k].push_back(i);
    }

    // 2) 访问标记 + 结果
    std::vector<char> vis(N, 0);
    std::vector<Cluster> clusters;
    clusters.reserve(N/32);

    auto push_if_near = [&](int j, int cid, std::queue<int>& q, Cluster& C){
        if (vis[j]) return;
        const QVector3D& a = pts[j];
        // 距离验证（只查近邻桶，大多数都能快速过滤）
        // 注意：这里不需要对比到所有邻居，只对已入队元素生长即可
        vis[j]=1; q.push(j); C.idx.push_back(j);
    };

    // 3) 遍历所有点，做“桶邻域”的区域生长（27 邻域）
    for (int i=0;i<N;++i) {
        if (vis[i]) continue;

        // 局部邻域查找
        Cluster C; C.idx.reserve(64);
        std::queue<int> q;
        vis[i]=1; q.push(i); C.idx.push_back(i);

        while (!q.empty()) {
            int u = q.front(); q.pop();
            const CellKey& ku = keys[u];

            for (int dx=-1; dx<=1; ++dx)
                for (int dy=-1; dy<=1; ++dy)
                    for (int dz=-1; dz<=1; ++dz) {
                        CellKey kn{ku.x+dx, ku.y+dy, ku.z+dz};
                        auto it = buckets.find(kn);
                        if (it==buckets.end()) continue;

                        // 检查该桶里的点
                        for (int j : it->second) {
                            if (vis[j]) continue;
                            const auto& pj = pts[j];
                            const auto& pu = pts[u];
                            float d2 = (pj.x()-pu.x())*(pj.x()-pu.x())
                                       + (pj.y()-pu.y())*(pj.y()-pu.y())
                                       + (pj.z()-pu.z())*(pj.z()-pu.z());
                            if (d2 <= eps2) push_if_near(j, 0, q, C);
                        }
                    }

            if ((int)C.idx.size() >= maxPts) break; // 防止巨大簇
        }

        if ((int)C.idx.size() >= minPts) {
            // 可选：截断到 maxPts，避免 OBB PCA 超大矩阵
            if ((int)C.idx.size() > maxPts) C.idx.resize(maxPts);
            clusters.push_back(std::move(C));
        }
    }
    return clusters;
}

// ---- PCA 求 OBB ----
static OBB computeOBB(const vector<QVector3D>& pts, const vector<int>& ids){
    int m=(int)ids.size();
    Eigen::MatrixXf X(3,m); Eigen::Vector3f mu(0,0,0);
    for (int c=0;c<m;++c){ const auto& p=pts[ids[c]];
        X(0,c)=p.x(); X(1,c)=p.y(); X(2,c)=p.z(); mu += Eigen::Vector3f(p.x(), p.y(), p.z()); }
    mu/=float(m); for (int c=0;c<m;++c) X.col(c)-=mu;

    Eigen::Matrix3f Cov=(X*X.transpose())/float(m);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> es(Cov);

    // 主轴按特征值降序
    Eigen::Matrix3f E=es.eigenvectors(); Eigen::Matrix3f A;
    A.col(0)=E.col(2); A.col(1)=E.col(1); A.col(2)=E.col(0);
    if (A.col(0).cross(A.col(1)).dot(A.col(2))<0) A.col(2)*=-1.f;

    float minx=1e9,maxx=-1e9, miny=1e9,maxy=-1e9, minz=1e9,maxz=-1e9;
    for (int c=0;c<m;++c){
        Eigen::Vector3f q=A.transpose()*X.col(c);
        minx=std::min(minx,q.x()); maxx=std::max(maxx,q.x());
        miny=std::min(miny,q.y()); maxy=std::max(maxy,q.y());
        minz=std::min(minz,q.z()); maxz=std::max(maxz,q.z());
    }
    Eigen::Vector3f c_local((minx+maxx)/2.f,(miny+maxy)/2.f,(minz+maxz)/2.f);
    Eigen::Vector3f c_world=mu + A*c_local;

    OBB b; b.center={c_world.x(),c_world.y(),c_world.z()};
    b.half={(maxx-minx)/2.f,(maxy-miny)/2.f,(maxz-minz)/2.f}; b.num_points=m;
    for (int r=0;r<3;++r) for (int c=0;c<3;++c) b.R(r,c)=A(r,c);
    return b;
}

// ---- 主流程：从 displayPoints 计算 OBB 列表 ----
void SimplePointCloudView::recomputeOBB(){
    vector<QVector3D> cloud; cloud.reserve(points.size());
    voxelDownsample(points, m_obbParam.voxel, cloud);

    auto clusters=gridCluster(cloud, m_obbParam.eps, m_obbParam.minPts, m_obbParam.maxPts);

    m_boxes.clear(); m_boxes.reserve(clusters.size());
    for (auto& c: clusters){
        if ((int)c.idx.size() < m_obbParam.minPts) continue;
        m_boxes.push_back(computeOBB(cloud, c.idx));
    }
    update();  // 让 paintGL 重绘
}

// ---- 绘制 OBB ----
static QVector3D corner(const OBB& b,int sx,int sy,int sz){
    return b.center
           + mul(b.R,{ b.half.x()*(sx?1:-1),0,0 })
           + mul(b.R,{ 0,b.half.y()*(sy?1:-1),0 })
           + mul(b.R,{ 0,0,b.half.z()*(sz?1:-1) });
}
void SimplePointCloudView::drawOBB(const OBB& b){
    auto L=[&](const QVector3D&a,const QVector3D&c){ glVertex3f(a.x(),a.y(),a.z()); glVertex3f(c.x(),c.y(),c.z()); };
    QVector3D v000=corner(b,0,0,0), v100=corner(b,1,0,0), v010=corner(b,0,1,0), v110=corner(b,1,1,0);
    QVector3D v001=corner(b,0,0,1), v101=corner(b,1,0,1), v011=corner(b,0,1,1), v111=corner(b,1,1,1);
    glLineWidth(2.f); glBegin(GL_LINES);
    L(v000,v100); L(v100,v110); L(v110,v010); L(v010,v000);   // 底
    L(v001,v101); L(v101,v111); L(v111,v011); L(v011,v001);   // 顶
    L(v000,v001); L(v100,v101); L(v110,v111); L(v010,v011);   // 立柱
    glEnd();
}

SimplePointCloudView::SimplePointCloudView(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    zoom = 1.0f;
    xRot = -70.0f;  // 让 Z 轴朝上（从 XY 平面转到 XZ）
    yRot = 0.0f;
}

void SimplePointCloudView::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
}

void SimplePointCloudView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearPlane = 0.1f;
    float farPlane = 40000.0f;
    QMatrix4x4 proj;
    proj.perspective(60.0f, float(w) / float(h), nearPlane, farPlane);
    glLoadMatrixf(proj.constData());
}

void SimplePointCloudView::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -zoom * 50.0f); // 控制相机距原点距离
    glRotatef(xRot, 1, 0, 0);           // 控制上下俯仰（Pitch）
    glRotatef(yRot, 0, 1, 0);           // 控制左右水平旋转（Yaw）
    glRotatef(-270.0f, 0, 0, 1);

    drawGrid();
    drawPointCloud();
    glColor3f(1.f, 0.f, 0.f);
    for (const auto& b : m_boxes) drawOBB(b);
    drawHudCoordinateAxes();
}

void SimplePointCloudView::mousePressEvent(QMouseEvent *event)
{
    lastMousePos = event->pos();
}

void SimplePointCloudView::mouseMoveEvent(QMouseEvent *event)
{
    int dx = event->x() - lastMousePos.x();
    int dy = event->y() - lastMousePos.y();
    xRot += dy;
    yRot += dx;
    lastMousePos = event->pos();
    update();
}

void SimplePointCloudView::wheelEvent(QWheelEvent *event)
{
    zoom *= (event->angleDelta().y() > 0) ? 0.9f : 1.1f;
    zoom = qBound(0.001f, zoom, 100.0f);
    update();
}

void SimplePointCloudView::updatePointCloud(QList<PointXYZI> pointList)
{
    points = pointList;
    if (drawObb) {
        recomputeOBB();
    }
    else {
        m_boxes.clear();
    }
    update();
}

void SimplePointCloudView::setGridVisible(bool visible)
{
    gridVisible = visible;
    update();
}

void SimplePointCloudView::resetView()
{
    zoom = 1.0f;
    xRot = -70.0f;
    yRot = 0.0f;
    update();
}

void SimplePointCloudView::setColorBarWidget(ColorBarWidget *colorBar)
{
    colorBarWidget = colorBar;
}

void SimplePointCloudView::clearPointCloud()
{
    points.clear();
    update();
}

QColor SimplePointCloudView::getColorFromIntensity(float intensity)
{
    float norm = qBound(0.0f, intensity / 255.0f, 1.0f);
    QColor color;
    color.setHsvF((1.0 - norm) * 240.0 / 360.0, 1.0, 1.0);
    return color;
}

void SimplePointCloudView::drawGrid()
{
    if (!gridVisible) return;

    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    float extent = 100.0f * zoom;
    float step = qMax(1.0f, extent / 10.0f);
    for (float i = -extent; i <= extent; i += step) {
        glVertex3f(i, -extent, 0.0f);  // 平行Y线 (在 XY 平面)
        glVertex3f(i, extent, 0.0f);
        glVertex3f(-extent, i, 0.0f);  // 平行X线
        glVertex3f(extent, i, 0.0f);
    }
    glEnd();
}



void SimplePointCloudView::drawTextStroke3D(const QVector3D &pos, const QString &text, const QColor &color)
{
    glPushMatrix();
    glTranslatef(pos.x(), pos.y(), pos.z());
    glScalef(0.1f, 0.1f, 0.1f);

    std::vector<QVector<QVector3D>> outlines = generateTextPath3D(text, QFont("Arial", 20, QFont::Bold));

    glColor3f(color.redF(), color.greenF(), color.blueF());
    glLineWidth(2.0f);
    for (const auto& contour : outlines) {
        glBegin(GL_LINE_STRIP);
        for (const auto& pt : contour) {
            glVertex3f(pt.x(), pt.y(), pt.z());
        }
        glEnd();
    }

    glPopMatrix();
}

std::vector<QVector<QVector3D>> SimplePointCloudView::generateTextPath3D(const QString &text, const QFont &font)
{
    std::vector<QVector<QVector3D>> outlines;

    QPainterPath path;
    path.addText(0, 0, font, text);

    QRectF bounds = path.boundingRect();
    QTransform centering;
    centering.translate(-bounds.center().x(), -bounds.center().y());
    path = centering.map(path);

    for (int i = 0; i < path.elementCount(); ++i) {
        QPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo()) {
            outlines.push_back(QVector<QVector3D>());
        }
        if (!outlines.empty()) {
            outlines.back().append(QVector3D(e.x, -e.y, 0));
        }
    }

    return outlines;
}

void SimplePointCloudView::drawHudCoordinateAxes()
{
    // 禁用深度写入与测试，确保 HUD 永远在前
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // 设置屏幕空间投影
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width(), 0, height(), -1, 1);  // 屏幕空间

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float len = 60.0f;

    // 旋转矩阵（跟随视角）
    QMatrix4x4 rot;
    rot.setToIdentity();
    rot.rotate(xRot, 1, 0, 0);
    rot.rotate(yRot, 0, 1, 0);
    // 与场景一致：绕 Z -90°
    rot.rotate(-270.0f, 0, 0, 1);

    QVector3D origin(80, 80, 0);  // 屏幕左下角起点
    QVector3D xAxis = rot * QVector3D(1, 0, 0);
    QVector3D yAxis = rot * QVector3D(0, 1, 0);
    QVector3D zAxis = rot * QVector3D(0, 0, 1);

    QVector3D xEnd = origin + xAxis * len;
    QVector3D yEnd = origin + yAxis * len;
    QVector3D zEnd = origin + zAxis * len;

    // 绘制轴线
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(origin.x(), origin.y(), 0); glVertex3f(xEnd.x(), xEnd.y(), 0);
    glColor3f(0, 1, 0); glVertex3f(origin.x(), origin.y(), 0); glVertex3f(yEnd.x(), yEnd.y(), 0);
    glColor3f(0, 0, 1); glVertex3f(origin.x(), origin.y(), 0); glVertex3f(zEnd.x(), zEnd.y(), 0);
    glEnd();

    // 绘制箭头（三角形）
    auto drawArrow = [](const QVector3D& start, const QVector3D& dir, const QColor& color) {
        QVector3D norm = dir.normalized();
        QVector3D ortho1(-norm.y(), norm.x(), 0);
        if (ortho1.lengthSquared() < 1e-4) ortho1 = QVector3D(1, 0, 0);
        QVector3D tip = start;
        QVector3D base1 = tip - norm * 6.0f + ortho1 * 4.0f;
        QVector3D base2 = tip - norm * 6.0f - ortho1 * 4.0f;

        glColor3f(color.redF(), color.greenF(), color.blueF());
        glBegin(GL_TRIANGLES);
        glVertex3f(tip.x(), tip.y(), 0);
        glVertex3f(base1.x(), base1.y(), 0);
        glVertex3f(base2.x(), base2.y(), 0);
        glEnd();
    };

    drawArrow(xEnd, xAxis, QColor(255, 0, 0));
    drawArrow(yEnd, yAxis, QColor(0, 255, 0));
    drawArrow(zEnd, zAxis, QColor(0, 0, 255));

    // ---- 绘制 X/Y/Z 标签（屏幕空间 + 旋转矩阵） ----
    auto drawHudLabel = [this](const QVector3D &anchor, const QString &text, const QColor &color, const QMatrix4x4 &rotation) {
        glPushMatrix();
        glTranslatef(anchor.x(), anchor.y(), 0);             // 位置
        glMultMatrixf(rotation.constData());                 // 旋转
        glScalef(1.0f, 1.0f, 0.1f);                           // 缩放
        auto outlines = generateTextPath3D(text, QFont("Arial", 20, QFont::Bold));
        glColor3f(color.redF(), color.greenF(), color.blueF());
        for (const auto& contour : outlines) {
            glBegin(GL_LINE_STRIP);
            for (const auto& pt : contour) {
                glVertex3f(pt.x(), pt.y(), 0);
            }
            glEnd();
        }
        glPopMatrix();
    };

    float labelOffset = 10.0f;
    drawHudBillboardText(xEnd + xAxis.normalized() * labelOffset, "X", Qt::red);
    drawHudBillboardText(yEnd + yAxis.normalized() * labelOffset, "Y", Qt::green);
    drawHudBillboardText(zEnd + zAxis.normalized() * labelOffset, "Z", Qt::blue);

    // 恢复状态
    glPopMatrix();  // modelview
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();  // projection
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

}
void SimplePointCloudView::drawHudBillboardText(const QVector3D &screenPos, const QString &text, const QColor &color)
{
    glPushMatrix();
    glTranslatef(screenPos.x(), screenPos.y(), 0);  // 平移到目标屏幕位置
    glScalef(0.5f, 0.5f, 1.0f);                      // 适当缩放

    std::vector<QVector<QVector3D>> outlines = generateTextPath3D(text, QFont("Arial", 20, QFont::Bold));

    glColor3f(color.redF(), color.greenF(), color.blueF());
    glLineWidth(1.0f);
    for (const auto& contour : outlines) {
        glBegin(GL_LINE_STRIP);
        for (const auto& pt : contour) {
            glVertex3f(pt.x(), pt.y(), 0);
        }
        glEnd();
    }

    glPopMatrix();
}

void SimplePointCloudView::drawPointCloud()
{
    glPointSize(pointSize);
    glBegin(GL_POINTS);
    for (const PointXYZI &pt : points) {
        QColor color;
        if (colorBarWidget)
            color = colorBarWidget->getColorForValue(pt.intensity);
        else
            color = getColorFromIntensity(pt.intensity);  // fallback
        glColor3f(color.redF(), color.greenF(), color.blueF());
        glVertex3f(pt.x, pt.y, pt.z);
    }
    glEnd();
}

void SimplePointCloudView::drawOBBs()
{
    glColor3f(1.0f, 0.0f, 0.0f);  // 红色
    glLineWidth(2.0f);

    for (const auto& ob : obstacles) {
        Eigen::Vector3f center = ob.obb_position;
        Eigen::Matrix3f rot = ob.obb_rotation;
        Eigen::Vector3f halfDims = ob.obb_dimensions * 0.5f;

        std::vector<Eigen::Vector3f> corners;
        for (int x = -1; x <= 1; x += 2)
            for (int y = -1; y <= 1; y += 2)
                for (int z = -1; z <= 1; z += 2)
                    corners.push_back(rot * (Eigen::Vector3f(x,y,z).cwiseProduct(halfDims)) + center);

        glBegin(GL_LINES);
        auto edge = [&](int i, int j) {
            glVertex3f(corners[i].x(), corners[i].y(), corners[i].z());
            glVertex3f(corners[j].x(), corners[j].y(), corners[j].z());
        };
        edge(0,1); edge(0,2); edge(0,4);
        edge(1,3); edge(1,5);
        edge(2,3); edge(2,6);
        edge(3,7);
        edge(4,5); edge(4,6);
        edge(5,7);
        edge(6,7);
        glEnd();
    }
}

