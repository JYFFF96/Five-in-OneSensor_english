#ifndef MMWAVERADAR_H
#define MMWAVERADAR_H

#include <QMainWindow>
#include <QToolButton>
#include <QPushButton>
#include "can.h"
#include "canmodel.h"
#include "common.h"
#include "HexInputFilter.h"
#include "canthread.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class mmWaveRadar;
}
QT_END_NAMESPACE

class mmWaveRadar : public QMainWindow
{
    Q_OBJECT

public:
    mmWaveRadar(QWidget *parent = nullptr);
    ~mmWaveRadar();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void on_btn_setRange_clicked();
    void updatePlotRange(double xRange, double yRange);
    void wgtCloseBtnClicked();
    void receiveData(VCI_CAN_OBJ data);
    void receiveSignal(bool flag, VCI_CAN_OBJ data);
    void on_btn_connectDev_clicked();
    void scrollToBottom();
    void saveFileFinished();

    void on_btn_pause_clicked();
    void receiveObstacleInfoList(QList<ObstacleInfo> obsList);
    void on_btn_clearTable_clicked();

    void on_checkBox_parseData_toggled(bool checked);

    void on_btn_obstacleSetSure_clicked();
    void onAllTargetStateChanged(int state);
    void onIndividualTargetStateChanged(int);
    void onAllTypeCheckBoxChanged(int state);
    void onIndividualTypeCheckBoxChanged(int);

    void on_btn_1mbd_clicked();

    void on_btn_3mbd_clicked();

    void on_btn_5mbd_clicked();

    void on_btn_45bd_clicked();

    void on_btn_fu45bd_clicked();

    void on_btn_comDoc_clicked();

    void on_comboBox_dataLen_currentIndexChanged(int index);

    void on_pushButton_send_clicked();
    void sendData2Dev();

    void on_pushButton_stop_clicked();

    void on_pushButton_22_clicked();

    void on_pushButton_23_clicked();

private:
    void initUI();
    void initTittleBar();
    void setupButtons();
    void setButtonStyle(QPushButton *btn, bool isSelected);
    void toggleButton(QPushButton *clickedBtn);
    void initWidgets(QWidget *wgt, int w, int h);
    void enableMouseTrackingForAll(QWidget *parent);
    void updateCursorShape(const QPoint &pos);
    void resizeWindow(const QPoint &delta);
    void checkResizeDirection(const QPoint &cursorPos);
    void showWgtAnimation(QWidget *wgt);
    void initTableview();
    void parseData(VCI_CAN_OBJ data);
    HexInputFilter* setupHexInput(QLineEdit *lineEdit, int maxBytes);
    bool isDataLengthValid(QLineEdit *lineEdit, QComboBox *comboBox); // 判断长度是否一致

private:
    Ui::mmWaveRadar *ui;
    QToolButton *minButton;
    QToolButton *maxButton;
    QToolButton *closeButton;
    QPoint lastMousePos;
    bool mousePressed = false;
    bool isDragging = false;  // 是否正在拖动
    QRect normalGeometry;  // 记录窗口还原前的位置
    bool isResizing = false;         // 是否正在调整窗口大小
    int resizingDirection = 0;       // 当前调整窗口的方向
    Qt::Edges resizeDirection;
    QMap<QWidget*, bool> wgtShowMap;
    QMap<QPushButton*, bool> buttonStates;  // 存储按钮的选中状态（true: 选中，false: 未选中）
    QList<QPushButton *> buttons;           // 按钮列表
    // Thread *canThread;
    CANThread *canThread;
    bool mconnect=false;
    bool isPaused = false;
    CanModel *model;
    QList<QPair<CanMsgType, VCI_CAN_OBJ>> bufferedData;
    int obsCount = 0; // 障碍物数量
    QList<ObstacleInfo> obsList;
    QVector<QCheckBox*> targetCheckboxes;
    bool updatingCheckStates = false;
    double currentBDDistance; // 当前标定距离
    double currentBDAngle;    // 当前标定角度
    bool isBding = false; // 是否正在标定
    QCheckBox *currentBDCheckBox = nullptr;
    QTimer *sendTimer;
    HexInputFilter* dataFilter;
    int sendTimes; // 发送次数
    int currentTimes = 0; // 发送次数
    VCI_CAN_OBJ psend;
};
#endif // MMWAVERADAR_H
