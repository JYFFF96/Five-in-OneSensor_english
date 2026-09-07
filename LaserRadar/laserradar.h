#ifndef LASERRADAR_H
#define LASERRADAR_H

#include <QMainWindow>
#include <QToolButton>
#include "colorbarwidget.h"
#include <QPushButton>
#include "udpreceiver.h"
#include "pointdatapagemodel.h"
#include "pointcloudfiltermanager.h"  // 需要加这个！
#include "exportframedialog.h"
#include "browserdialog.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class LaserRadar;
}
QT_END_NAMESPACE
class LaserRadar : public QMainWindow
{
    Q_OBJECT

public:
    LaserRadar(QWidget *parent = nullptr);
    ~LaserRadar();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void signalExportToUDP(const ExportParams& params);

private slots:
    void on_checkBox_toggled(bool checked);

    void on_comboBox_currentIndexChanged(int index);
    void wgtCloseBtnClicked();
    void on_btn_basicSetting_clicked();

    void on_btn_start_clicked();

    void on_btn_colorSetting_clicked();

    void on_btn_radarBD_clicked();

    void on_btn_obs_clicked();

    void on_btn_dataShow_clicked();

    void on_btn_dylb_clicked();

    void on_btn_import_clicked();

    void on_btn_export_clicked();

    void updatePointData(QList<PointData> pointDataList);  // 你原来的

    // 🚀 新增：自动滤波槽函数
    void onPointDataUpdated(const QList<PointXYZI> &points);

    void on_pushButton_clicked();

    void on_spinPassXMax_valueChanged(double arg1);

    void on_spinPassXMin_valueChanged(double arg1);

    void on_spinPassYMax_valueChanged(double arg1);

    void on_spinPassYMin_valueChanged(double arg1);

    void on_spinPassZMax_valueChanged(double arg1);

    void on_spinPassZMin_valueChanged(double arg1);
    void openExprotFrameDlg(ExportType type);
    void onExportParamsReceived(const ExportParams& params);
    void onExportFinished();
    void onStopExport();

    // void on_checkBox_enable_toggled(bool checked);

    // void on_checkBox_disable_toggled(bool checked);

    void on_checkBox_enable_clicked();

    void on_checkBox_disable_clicked();

    void on_label_link_linkActivated(const QString &link);

    void on_btn_connectDev_clicked();

    void on_btn_connectDev_2_clicked();

    void on_label_2_linkActivated(const QString &link);
    void updateLBQpage();
    void updateObsParams();

private:
    void initUI();
    void initTittleBar();
    void resizeWindow(const QPoint &delta);
    void updateCursorShape(const QPoint &cursorPos);
    void checkResizeDirection(const QPoint &cursorPos);
    void enableMouseTrackingForAll(QWidget *parent);
    void initWidgets(QWidget *wgt, int w, int h);
    void setupButtons();
    void toggleButton(QPushButton *clickedBtn);
    void setButtonStyle(QPushButton *btn, bool isSelected);
    void showWgtAnimation(QWidget *wgt);

    // 🚀 新增：初始化滤波器界面
    void initFilterUI();
    void updateFilterSettings();  // 新增，更新 filterManager 的 filterList

    void updateExportSatatus(bool exporting);

private:
    Ui::LaserRadar *ui;
    QPoint lastMousePos;
    bool mousePressed = false;
    bool isDragging = false;
    QRect normalGeometry;
    QToolButton *minButton;
    QToolButton *maxButton;
    QToolButton *closeButton;
    bool isResizing = false;
    int resizingDirection = 0;
    Qt::Edges resizeDirection;

    ColorBarWidget *colorBar;
    bool initColorBar = false;

    QMap<QWidget*, bool> wgtShowMap;
    QMap<QPushButton*, bool> buttonStates;
    QList<QPushButton *> buttons;

    UdpReceiver *udp;
    PointDataPageModel *model;

    bool enableObstacleDetection = false;
    bool obstacleDetecting = false;
    // 🚀 新增：滤波器管理器
    PointCloudFilterManager *filterManager = nullptr;

    bool enableFilter = false;  // 是否启用滤波器

    // 🚀 缓存原始点云
    QList<PointXYZI> originalPoints;

    QAction* actionRecord;
    QMenu* exportSubMenu;
    QAction* actionStopExport;
};

#include <QDialog>
#include <QVBoxLayout>
#include <QSvgWidget>

class SvgDialog : public QDialog {
public:
    SvgDialog(const QString &svgFile, QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("LiDAR Coordinates and Rotation Diagram");

        QVBoxLayout *layout = new QVBoxLayout(this);
        QSvgWidget *svgWidget = new QSvgWidget(svgFile, this);
        svgWidget->setMinimumSize(400, 400);

        layout->addWidget(svgWidget);
        setLayout(layout);
    }
};

#endif // LASERRADAR_H
