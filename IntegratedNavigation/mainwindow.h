#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "compasswidget.h"
#include "QChartView"
#include <QTimer>
#include <QtSerialPort/QSerialPort>
#include <mapbridge.h>
#include "trajectorysimulator.h"
#include "satellitecounter.h"
#include "simulatepolarplot.h"
#include <QToolButton>
#include <QPushButton>
#include "fastcommanddialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // bool eventFilter(QObject *obj, QEvent *event) override;  // 事件过滤器，监听 tabBar 的双击事件
    void mousePressEvent(QMouseEvent *event) override;      // 处理鼠标按下事件
    void mouseMoveEvent(QMouseEvent *event) override;       // 处理鼠标拖动事件
    void mouseReleaseEvent(QMouseEvent *event) override;    // 处理鼠标释放事件
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
    void initTittleBar();
    void initSatelliteInfo();
    void updatePortList(QStringList list);
    void saveDatatoFile(QByteArray data);
    void initBaiduOnlineMap();
    QList<SatelliteGsvData> analyzeGsvStatements(QString nmeaGSV);
    void updateCnrPlot(QList<SatelliteGsvData> data);
    void updateDataViewFromCarInfo(carPostionInfo carInfo);
    void updateDataViewFromGsa(QString gsa);
    void updateDataViewFromGst(QString gst);
    void toggleMaximizeRestore(); // 切换最大化/还原窗口
    void enableMouseTrackingForAll(QWidget *parent);
    bool isClickOnTabBarEmptyArea(const QPoint &mousePos);

    void updateCursorShape(const QPoint &pos);
    void resizeWindow(const QPoint &delta);
    void checkResizeDirection(const QPoint &cursorPos);
    void setupButtons();
    void setButtonStyle(QPushButton *btn, bool isSelected);
    void toggleButton(QPushButton *clickedBtn);
    void parseSentence(QByteArray data);
private slots:
    void UpdateSerialPort();

    void on_btn_connect_clicked();
    void readSerialData();

    void on_btn_clear_clicked();

    void on_btn_pause_clicked();

    void on_btn_biaoding_clicked();

    void on_btn_send_clicked();

    void on_btn_savePath_clicked();
    void receiveCoordinatePoints(QVector<GPSPoint> points);

    void on_btn_start_clicked();
    void slotSendCurrentPos(GPSPoint currentPos);
    void showVisualSatelliteList(QVector<SatelliteData> list);
    void slotReceviedCarInfo(carPostionInfo carInfo);

    void on_btn_nextPage_clicked();

    void on_btn_connect2_clicked();

    void on_btn_prevPage_clicked();

    void on_btn_openDoc_clicked();

    void on_ben_openExample_clicked();

    void on_pushButton_clicked();
    void slotSendFastCmd(const QString& commandContent);
signals:
    void sendRoll(double angle);
    void sendPitch(double angle);
    void sendYaw(double angle);

private:
    Ui::MainWindow *ui;
    CompassWidget *compass = nullptr;
    QTimer *refreshSerialPortTimer = nullptr;
    QStringList portList; // 串口列表
    QSerialPort *serial = nullptr;
    bool b_pause = false; // 暂停报文显示
    QByteArray hisData;
    QStringList sendMsgList;
    QString savePath; // 存储路径
    MapBridge *bridge;
    QVector<GPSPoint> m_points; // 导航轨迹坐标点
    SatelliteCounter *satelliteCounter;
    bool connectFlag = false;
    QPoint dragPos;
    QWidget *buttonContainer;
    QToolButton *minButton;
    QToolButton *maxButton;
    QToolButton *closeButton;
    bool mousePressed = false;
    bool isDragging = false;
    bool isResizing = false;
    Qt::Edges resizeDirection;
    QPoint lastMousePos;
    QRect normalGeometry;
    QMap<QPushButton*, bool> buttonStates;  // 存储按钮的选中状态（true: 选中，false: 未选中）
    QList<QPushButton *> buttons;           // 按钮列表
    QByteArray rxBuf;
    TrajectorySimulator *simulator;
    FastCommandDialog *fastCmdDlg;
};
#endif // MAINWINDOW_H
