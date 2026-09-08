#ifndef ULTRASONICRADAR_H
#define ULTRASONICRADAR_H

#include <QMainWindow>
#include <QVBoxLayout>
#include "mycustomplot.h"
#include "can.h"
#include "canmodel.h"
#include "HexInputFilter.h"
#include "QTimer"
#include "canthread.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class UltrasonicRadar;
}
QT_END_NAMESPACE

enum ResizeDirection {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8
};
class UltrasonicRadar : public QMainWindow
{
    Q_OBJECT

public:
    UltrasonicRadar(QWidget *parent = nullptr);
    ~UltrasonicRadar();
protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private slots:
    void on_btn_connet_clicked();

    void on_btn_scale_clicked();

    void on_btn_startRadar_clicked();

    void on_btn_radarBD_clicked();

    void on_btn_commProtocol_clicked();

    void on_btn_dataSave_clicked();

    void wgtCloseBtnClicked();

    void on_btn_setRange_clicked();
    void receiveMsg(QString str);
    void receiveData(VCI_CAN_OBJ data);
    void receiveSignal(bool flag, VCI_CAN_OBJ data);

    void on_btn_connectDev_clicked();

    void on_btn_clearTable_clicked();

    void on_btn_pause_clicked();

    void on_checkBox_parseData_toggled(bool checked);
    void scrollToBottom();
    void updateAxisRange(double xRange, double yRange);
    void sendData2Dev();
    void on_pushButton_send_clicked();

    void on_comboBox_dataLen_currentIndexChanged(int index);

    void on_btn_radar1bd_clicked();

    void on_btn_radar2bd_clicked();

    void on_btn_radar3bd_clicked();

    void on_btn_radar4bd_clicked();

    void on_pushButton_stop_clicked();

    void on_pushButton_22_clicked();

    void on_lineEdit_savaPath_textChanged(const QString &arg1);

    void on_checkBox_savaData_toggled(bool checked);

    void on_lineEdit_saveLine_textChanged(const QString &arg1);
    void saveFileFinished();

    void on_btn_comDoc_clicked();

    void on_lineEdit_xAxisRange_textChanged(const QString &arg1);

    void on_lineEdit_yAxisRange_textEdited(const QString &arg1);

    void on_pushButton_23_clicked();

private:
    void initUI();
    void initTittleBar();
    void initWidgets(QWidget *wgt, int w, int h);
    void updateDataShowGeo();
    // 是否是收回
    bool isOutCurrentWgt(QWidget *wgt);
    void showWgtAnimation(QWidget *wgt);
    void parseData(VCI_CAN_OBJ data);
    HexInputFilter* setupHexInput(QLineEdit *lineEdit, int maxBytes);
    bool isDataLengthValid(QLineEdit *lineEdit, QComboBox *comboBox); // 判断长度是否一致
    void setupButtons();
    void toggleButton(QPushButton *clickedBtn);
    void setButtonStyle(QPushButton *btn, bool isSelected);
    void updateCursorShape(const QPoint &pos);
    void resizeWindow(const QPoint &delta);
    void checkResizeDirection(const QPoint &cursorPos);
    void enableMouseTrackingForAll(QWidget *parent);
    double adjustYAxisRange(QCustomPlot *customPlot, double xMin, double xMax);
    double adjustXAxisRange(QCustomPlot *customPlot, double yMin, double yMax);
private:
    Ui::UltrasonicRadar *ui;
    QMap<QWidget*, bool> wgtShowMap;
    // Thread *canThread;
    CANThread *canThread;
    bool mconnect=false;
    bool isPaused = false;
    QList<QPair<CanMsgType, VCI_CAN_OBJ>> bufferedData;
    CanModel *model;
    QTimer *sendTimer;
    HexInputFilter* dataFilter;
    int sendTimes; // 发送次数
    int currentTimes = 0; // 发送次数
    VCI_CAN_OBJ psend;
    bool isBding =false; // 是否标定中
    QPair<int, int> currentBdRadar; // 雷达序号， 标定距离
    QMap<QPushButton*, bool> buttonStates;  // 存储按钮的选中状态（true: 选中，false: 未选中）
    QList<QPushButton *> buttons;           // 按钮列表
    // bool changedColor = false;
    QPoint lastMousePos;
    bool mousePressed = false;
    bool isDragging = false;  // 是否正在拖动
    QRect normalGeometry;  // 记录窗口还原前的位置
    QToolButton *minButton;
    QToolButton *maxButton;
    QToolButton *closeButton;
    bool isResizing = false;         // 是否正在调整窗口大小
    int resizingDirection = 0;       // 当前调整窗口的方向
    Qt::Edges resizeDirection;
};
#endif // ULTRASONICRADAR_H
