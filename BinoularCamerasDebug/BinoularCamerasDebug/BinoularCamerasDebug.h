#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_BinoularCamerasDebug.h"
#include "stereocamera.h"
#include <qtimer.h>
#include "framemonitor.h"
#include "common.h"
#include <QMutex>
#include <QMessageBox>
#include <qtoolbutton.h>

struct BoardResult {
	cv::Rect outer_bbox;                 // 棋盘格外框
	std::vector<cv::Point2f> centers;    // 棋盘格中心点（先左列从上到下，再右列从上到下）
};
class BinoularCamerasDebug : public QMainWindow
{
    Q_OBJECT

public:
    BinoularCamerasDebug(QWidget *parent = nullptr);
    ~BinoularCamerasDebug();
	void setIpAddress(QString ip);
	void startCamera();

protected:
	bool eventFilter(QObject* obj, QEvent* ev);

	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
	void initTittleBar();
	void initDebugWinUi();
	void initDebugStudyBtnStatus();
	void initStudyWinUi();
	void updateTakePhotoBtnStatus(bool flag);
	void applyIntRange(QLineEdit *le, int min, int max, QObject *parent);
	void setupIntLine(QLineEdit *edit, int min, int max);
	void checkRange(QLineEdit *edit);
	void initDebugBtnStyle(QToolButton* b, const QString& text, const QIcon& icon);
	void setDebugBtnValidated(QToolButton* b, bool ok);
	void saveImage(cv::Mat img, QString path);
	void enableMouseTrackingForAll(QWidget *parent);
	void checkResizeDirection(const QPoint &cursorPos);
	void resizeWindow(const QPoint &delta);
	void updateCursorShape(const QPoint &pos);

private slots:
	void onFrameUpdate();
	void updateCamreaPage();
	void setDebugNextPage();
	void SaveInstall();
	void SaveConfig();
	void SaveBiaoding();
	void btn_takePhoto_clicked();

	void updateMainPage();
	void updateStudyMainPage();
	void loadBiaodingImage(const QString& lin);
	void updateDebugStudyPage();
	void returnMainStudyPage();
	void debugStudyNextStep();
	void debugStudyPrevStep();
	void startDebugStudy();
	void slotDebugBtnClicked();
	void slotFinishGroundStudy();
	void slotStudyPageChanged(int page);
	void slotGroundStudyPageChanged(int page);
	void slotFinishDebug();
	void slotSavePicture(bool checked);
	void slotSetShowNearestObs(bool checked);
	void slotSetShowDriveArea(bool checked);

	void Biaoding5MNextStep(bool flag);
	void Biaoding10MNextStep(bool flag);
	void Biaoding15MNextStep(bool flag);


private:
    Ui::BinoularCamerasDebug ui;
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
	QString m_ip;
	QTimer* timer;
	FrameMonitor* frameMonitor;
	StereoCamera* camera;
	double curBiaodingDistance = 5.0;
	bool startSudy = false;
	bool groundBiaoding = false;
	bool drawCarLine = false;
	bool targetStudy = false;
	bool b_checkTarget = false; // 靶标学习是否成功
	bool b_biaodingFinish = false;
	bool b_studyFinish = false;
	bool b_debugFinish = false; // 安装调试是否完成
	bool b_saveImages = false;
};
