#include "BinoularCamerasDebug.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>

#include "framemonitor.h"
#include "frameid.h"
#include "taskiddef.h"
#include <qlistwidget.h>
#include "ChessboardAimerWidget.h"
#include "board_detect.h"
#include <qdebug.h>
#include "qvalidator.h"
#include <qbuttongroup.h>
#include <qdir.h>
#include <qfile.h>
#include <QKeyEvent>   
#include "frameext.h"
#include "roadwaypainter.h"
#include "obstaclepainter.h"
#include <QDateTime>
#include "ActionOnlyCheckBox.h"
#include <QStandardPaths>

static const char *kMinProp = "rg_min";
static const char *kMaxProp = "rg_max";
static inline bool isUnderProgramFiles(const QString& p) {
#if defined(Q_OS_WIN)
	const QString pf = qEnvironmentVariable("ProgramFiles");
	const QString pf86 = qEnvironmentVariable("ProgramFiles(x86)");
	const QString dir = QDir(p).absolutePath().toLower();
	if (!pf.isEmpty() && dir.startsWith(QDir(pf).absolutePath().toLower()))   return true;
	if (!pf86.isEmpty() && dir.startsWith(QDir(pf86).absolutePath().toLower())) return true;
#endif
	return false;
}

static inline bool ensureWritableDir(QString& dirPath, QString* why = nullptr) {
	if (dirPath.isEmpty()) { if (why) *why = "empty path"; return false; }
	if (!QDir().mkpath(dirPath)) { if (why) *why = "mkpath failed"; return false; }
	QString probe = QDir(dirPath).filePath(".write_probe.tmp");
	QFile f(probe);
	if (!f.open(QIODevice::WriteOnly)) {
		if (why) *why = QString("cannot open probe: %1").arg(f.errorString());
		return false;
	}
	f.write("ok"); f.close(); QFile::remove(probe);
	return true;
}

static inline QString pickFallbackDir() {
	QString d = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	if (d.isEmpty()) d = QDir::tempPath();
	d = QDir(d).filePath("BinocularCameras"); // 你自定义的子目录
	QDir().mkpath(d);
	return d;
}

// 非 8bit 转 8bit，保证 QImage 可保存
static inline cv::Mat to8uIfNeeded(const cv::Mat& src) {
	if (src.empty() || src.depth() == CV_8U) return src;
	cv::Mat out; double minv = 0, maxv = 0;
	cv::minMaxLoc(src, &minv, &maxv);
	const double scale = (maxv - minv) > 1e-12 ? 255.0 / (maxv - minv) : 1.0;
	const double shift = -minv * scale;
	src.convertTo(out, CV_MAKETYPE(CV_8U, src.channels()), scale, shift);
	return out;
}
BinoularCamerasDebug::BinoularCamerasDebug(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	initTittleBar();
	initDebugWinUi();
	initStudyWinUi();
	// 设置按钮互斥
	auto *group = new QButtonGroup(this);
	group->setExclusive(true);               // 开启互斥
	for (auto *b : { ui.mainBtn_debug, ui.mainBtn_study }) {
		b->setCheckable(true);               // 必须可切换
		group->addButton(b);
		b->setProperty("class", "topTab");
	}
	ui.mainBtn_debug->setChecked(true);              // 默认选中

	auto *group1 = new QButtonGroup(this);
	group1->setExclusive(true);               // 开启互斥
	for (auto *b : { ui.btn_leftCamera, ui.bth_rightCamera }) {
		b->setCheckable(true);               // 必须可切换
		group1->addButton(b);
		b->setProperty("class", "topTab");
	}
	ui.btn_leftCamera->setChecked(true);              // 默认选中

	auto *group2 = new QButtonGroup(this);
	group2->setExclusive(true);               // 开启互斥
	for (auto *b : { ui.btn_leftCamera_study, ui.btn_rightCamera_study, ui.btn_disparity }) {
		b->setCheckable(true);               // 必须可切换
		group2->addButton(b);
		b->setProperty("class", "topTab");
	}
	ui.btn_leftCamera_study->setChecked(true);              // 默认选中


	connect(ui.btn_leftCamera, &QPushButton::clicked, this, &BinoularCamerasDebug::updateCamreaPage);
	connect(ui.bth_rightCamera, &QPushButton::clicked, this, &BinoularCamerasDebug::updateCamreaPage);
	connect(ui.mainBtn_debug, &QPushButton::clicked, this, &BinoularCamerasDebug::updateMainPage);
	connect(ui.mainBtn_study, &QPushButton::clicked, this, &BinoularCamerasDebug::updateMainPage);

	connect(ui.btn_leftCamera_study, &QPushButton::clicked, this, &BinoularCamerasDebug::updateStudyMainPage);
	connect(ui.btn_rightCamera_study, &QPushButton::clicked, this, &BinoularCamerasDebug::updateStudyMainPage);
	connect(ui.btn_disparity, &QPushButton::clicked, this, &BinoularCamerasDebug::updateStudyMainPage);
	connect(ui.btn_acquisition, &QPushButton::clicked, this, &BinoularCamerasDebug::updateStudyMainPage);

	//ui.aimerView->setFovDegrees(38, 21);
	//ui.aimerView->setBoardSizeMeters(0.40, 0.90);
	//ui.aimerView->setTolerances(0.20, 0.05);
	this->setStyleSheet(
		"QMainWindow{"
		"  border-image:url(:/images/images/background.png) 0 0 0 0 stretch stretch;"
		"}"
	);

	//setDebugBtnValidated(ui.btn_install, true);
	ui.btn_cfg->setChecked(true);
	ui.mainBtn_study->setEnabled(true);
	ui.btn_autoStudy->hide();
	ui.btn_toolStudy->hide();

	enableMouseTrackingForAll(this);
} 

BinoularCamerasDebug::~BinoularCamerasDebug()
{
	if (camera->isConnected()) {
		camera->disconnectFromServer();
	}
}


void BinoularCamerasDebug::setIpAddress(QString ip)
{
	m_ip = ip;
}

void BinoularCamerasDebug::startCamera()
{
	camera = StereoCamera::connect(m_ip.toUtf8());
	frameMonitor = new FrameMonitor;
	camera->enableTasks(TaskId::DisplayTask | TaskId::ObstacleTask);
	camera->requestFrame(frameMonitor, FrameId::CalibLeftCamera | FrameId::RightCamera | FrameId::Disparity | FrameId::Obstacle);

	while (!camera->isConnected()) {
		printf("connecting...\n");
		std::this_thread::sleep_for(std::chrono::seconds(1));
	} 
	bool test = camera->isConnected();

	timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &BinoularCamerasDebug::onFrameUpdate);
	timer->start(20); // 每 80ms 更新一次
}

bool BinoularCamerasDebug::eventFilter(QObject * obj, QEvent * ev)
{
	auto *edit = qobject_cast<QLineEdit*>(obj);
	if (!edit) return QWidget::eventFilter(obj, ev);

	// 失去焦点时检查
	if (ev->type() == QEvent::FocusOut) {
		checkRange(edit);
	}
	// 按下回车/小键盘回车时检查
	else if (ev->type() == QEvent::KeyPress) {
		auto *ke = static_cast<QKeyEvent*>(ev);
		if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
			checkRange(edit);
			// 如果不希望事件继续冒泡，可以 return true;
		}
	}
	return QWidget::eventFilter(obj, ev);
}

void BinoularCamerasDebug::resizeEvent(QResizeEvent * event)
{
}

void BinoularCamerasDebug::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		QWidget *clickedWidget = childAt(event->pos());

		// 仅当鼠标点击 wgt_top 时，才允许拖动窗口
		bool canDrag = (clickedWidget == ui.wgt_top);

		if (clickedWidget && clickedWidget->inherits("QToolButton")) {
			return;  // 点击的是按钮，不执行拖动或缩放
		}

		mousePressed = true;
		lastMousePos = event->pos();
		isDragging = false;
		isResizing = false;
		resizeDirection = Qt::Edges();

		if (isMaximized()) {
			if (canDrag) {
				isDragging = true;  // 仅允许拖动，不调整大小
				normalGeometry = geometry();
			}
		}
		else {
			checkResizeDirection(event->pos());
			if (resizeDirection != Qt::Edges()) {
				isResizing = true;  // 进入调整大小模式
			}
			else if (canDrag) {
				isDragging = true;  // 进入拖动模式
			}
		}
	}
}

void BinoularCamerasDebug::mouseMoveEvent(QMouseEvent * event)
{
	if (!mousePressed) {
		if (!isMaximized()) {  // 🔹 窗口最大化时不更新鼠标光标
			updateCursorShape(event->pos());
		}
		else {
			setCursor(Qt::ArrowCursor);
		}
		return;
	}

	QPoint delta = event->pos() - lastMousePos;

	if (isMaximized() && isDragging) {
		// 退出最大化，并调整窗口位置
		showNormal();
		maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
		move(event->pos() - QPoint(width() / 2, 10));
	}
	else if (isResizing && !isMaximized()) {  // 🔹 窗口最大化时不能调整大小
		resizeWindow(delta);
	}
	else if (isDragging) {  // 只有在 isDragging 允许时才拖动窗口
		move(pos() + delta);
	}

	lastMousePos = event->pos();
}

void BinoularCamerasDebug::mouseReleaseEvent(QMouseEvent * event)
{
	Q_UNUSED(event);
	mousePressed = false;
	isDragging = false;
	isResizing = false;
	resizeDirection = Qt::Edges();
}

void BinoularCamerasDebug::mouseDoubleClickEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton) {
		QWidget *clickedWidget = childAt(event->pos());

		// 仅当鼠标点击 wgt_top 时，才允许拖动窗口
		if (clickedWidget == ui.wgt_top) {
			if (isMaximized()) {
				showNormal();
				maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
			}
			else {
				showMaximized();
				maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
			}

		}
	}
}

void BinoularCamerasDebug::initTittleBar()
{
	QIcon icon(":/images/images/logo.png");
	this->setWindowIcon(icon);

	this->setWindowFlags(Qt::FramelessWindowHint); // 隐藏标题栏
	this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
	this->setAttribute(Qt::WA_TranslucentBackground, false); // 若要半透明窗口则设 true
	QHBoxLayout *layout = new QHBoxLayout(ui.wgt_top);
	layout->setSpacing(5);
	minButton = new QToolButton(ui.wgt_top);
	maxButton = new QToolButton(ui.wgt_top);
	closeButton = new QToolButton(ui.wgt_top);

	// 设置按钮大小
	minButton->setFixedSize(40, 30);
	maxButton->setFixedSize(40, 30);
	closeButton->setFixedSize(40, 30);
	minButton->setIconSize(QSize(15, 15));
	maxButton->setIconSize(QSize(15, 15));
	closeButton->setIconSize(QSize(15, 15));

	// 设置图标
	minButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
	maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
	closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));

	// 按钮样式
	QString minButtonStyle = (
		"QToolButton { background: transparent; border: none; color: white; padding-bottom: 10px; qproperty-alignment: AlignCenter; }"
		"QToolButton:hover { background: rgba(255, 255, 255, 0.2); }"
		"QToolButton:pressed { background: rgba(255, 255, 255, 0.1); }"
		);
	QString minMaxButtonStyle =
		"QToolButton { background: transparent; border: none; color: white; padding: 0px; }"
		"QToolButton:hover { background: rgba(255, 255, 255, 0.2); }"
		"QToolButton:pressed { background: rgba(255, 255, 255, 0.1); }"; // 点击时变暗一点点

	// 关闭按钮样式（默认透明，悬停红色，点击更深红色）
	QString closeButtonStyle =
		"QToolButton { background: transparent; border: none; color: white; padding: 0px; }"
		"QToolButton:hover { background: red; }"
		"QToolButton:pressed { background: darkred; }"; // 点击时变为深红色
	minButton->setStyleSheet(minButtonStyle);
	maxButton->setStyleSheet(minMaxButtonStyle);
	closeButton->setStyleSheet(closeButtonStyle);

	// 绑定按钮事件
	connect(minButton, &QToolButton::clicked, this, &QMainWindow::showMinimized);
	connect(maxButton, &QToolButton::clicked, this, [=]() {
		if (isMaximized()) {
			showNormal();
			maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
		}
		else {
			showMaximized();
			maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
		}
	});
	connect(closeButton, &QToolButton::clicked, this, &QMainWindow::close);
	layout->addWidget(minButton);
	layout->addWidget(maxButton);
	layout->addWidget(closeButton);
	ui.wgt_top->layout()->addItem(layout);
}

void BinoularCamerasDebug::initDebugWinUi()
{
	initDebugBtnStyle(ui.btn_cfg, QStringLiteral("Communication Settings"), QIcon(":/images/images/prev_cfg.png"));
	initDebugBtnStyle(ui.btn_install, QStringLiteral("Camera Installation"), QIcon(":/images/images/prev_install.png"));
	initDebugBtnStyle(ui.btn_biaoding, QStringLiteral("Camera Calibration"), QIcon(":/images/images/prev_biaoding.png"));
	initDebugBtnStyle(ui.btn_study, QStringLiteral("Pose Learning"), QIcon(":/images/images/prev_study.png"));
	initDebugBtnStyle(ui.btn_yjset, QStringLiteral("Warning Settings"), QIcon(":/images/images/prev_yjset.png"));
	auto ss = R"(
QToolButton{
    border: none;
    border-radius: 12px;
    padding: 10px 10px 10px 14px;   /* 右侧预留 42px 放“验证成功”图标 */
    background: rgba(255,255,255,0.08);
    color: rgba(255,255,255,0.85);
    qproperty-iconSize: 30px 30px;
	min-width: 160px;
    min-height: 60px;
	font-size: 30px;
}
QToolButton:hover{   background: rgba(255,255,255,0.12); }
QToolButton:pressed{ background: rgba(255,255,255,0.16); }

/* 选中高亮 */
QToolButton:checked{
    background: #6A8FF0;
    color: white;
}

/* 验证成功：在右侧放一张小图（深色底用浅色图标） */
QToolButton[validated="true"]{
     background: #67C23A;              /* 绿色 */
    background-image: none;            /* 取消原来的图片 */
}
/* 选中 + 验证成功：换成白色图标以适配蓝底 */
QToolButton:checked[validated="true"]{
    background: #67C23A;
}
)";
	ui.btn_cfg->setStyleSheet(ss);
	ui.btn_install->setStyleSheet(ss);
	ui.btn_biaoding->setStyleSheet(ss);
	ui.btn_study->setStyleSheet(ss);
	ui.btn_yjset->setStyleSheet(ss);
	QButtonGroup *group = new QButtonGroup(this);
	group->setExclusive(true);
	for (QToolButton* b : { ui.btn_cfg, ui.btn_install, ui.btn_biaoding, ui.btn_study, ui.btn_yjset }) {
		b->setCheckable(true);
		group->addButton(b);
		connect(b, &QToolButton::clicked, this, &BinoularCamerasDebug::slotDebugBtnClicked);
	}

	//setupIntLine(ui.lineEdit_left2ground, 0, 300);
	//setupIntLine(ui.lineEdit_left2glassL, 30, 250);
	//setupIntLine(ui.lineEdit_left2bxg, 0, 250);
	//setupIntLine(ui.lineEdit_left2glassR, 30, 250);
	//setupIntLine(ui.lineEdit_car2ground, 0, 300);
	//setupIntLine(ui.lineEdit_carqljj, 100, 500);

	ui.btnSave_biaoding->hide();
	ui.btn_takePhoto->setEnabled(false);

	ui.checkBox_yp5m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui.checkBox_yp10m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui.checkBox_yp15m->setAttribute(Qt::WA_TransparentForMouseEvents, true);


	QFont font = ui.label_loadImage->font();
	font.setUnderline(true);
	ui.label_loadImage->setFont(font);

	// 如果想当按钮用，需要开启鼠标事件
	ui.label_loadImage->setCursor(Qt::PointingHandCursor);  // 鼠标悬停时变小手


	ui.stackedWidget_3->setCurrentWidget(ui.mainDebug);
	ui.stackedWidget->setCurrentWidget(ui.page_config);
	ui.matLeft_debugBiaoding->setDrawCrosshair(true);
	ui.matLeft_debugBiaoding->setDrawBox(true);
	ui.matLeft_debugBiaoding->setOverlayColor(MatView::OverlayColor::Red);
	ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);

	ui.stackedWidget_study->setCurrentWidget(ui.page_mainStudy);


	connect(ui.btnSave_cfg, &QPushButton::clicked, this, &BinoularCamerasDebug::SaveConfig);
	connect(ui.btnSave_install, &QPushButton::clicked, this, &BinoularCamerasDebug::SaveInstall);
	connect(ui.btn_takePhoto, &QPushButton::clicked, this, &BinoularCamerasDebug::btn_takePhoto_clicked);
	connect(ui.btnSave_biaoding, &QPushButton::clicked, this, &BinoularCamerasDebug::SaveBiaoding);
	connect(ui.label_loadImage, &QLabel::linkActivated, this, &BinoularCamerasDebug::loadBiaodingImage);
	initDebugStudyBtnStatus();
	connect(ui.btn_nextStep, &QPushButton::clicked, this, &BinoularCamerasDebug::debugStudyNextStep);
	connect(ui.btn_prevStep, &QPushButton::clicked, this, &BinoularCamerasDebug::debugStudyPrevStep);
	connect(ui.btn_startStudy, &QPushButton::clicked, this, &BinoularCamerasDebug::startDebugStudy);
	connect(ui.btn_groundFinish, &QPushButton::clicked, this, &BinoularCamerasDebug::slotFinishGroundStudy);
	connect(ui.btn_debugFinish, &QPushButton::clicked, this, &BinoularCamerasDebug::slotFinishDebug);
	connect(ui.stackedWidget_study, &QStackedWidget::currentChanged, this, &BinoularCamerasDebug::slotStudyPageChanged); 
		connect(ui.stackedWidget_groundStudy, &QStackedWidget::currentChanged, this, &BinoularCamerasDebug::slotGroundStudyPageChanged);
		connect(ui.checkBox_load5m, &ActionOnlyCheckBox::nextStep, this, &BinoularCamerasDebug::Biaoding5MNextStep);
		connect(ui.checkBox_load10m, &ActionOnlyCheckBox::nextStep, this, &BinoularCamerasDebug::Biaoding10MNextStep);
		connect(ui.checkBox_load15m, &ActionOnlyCheckBox::nextStep, this, &BinoularCamerasDebug::Biaoding15MNextStep);
}

void BinoularCamerasDebug::initDebugStudyBtnStatus()
{
	auto *fit = new FitIconFilter(this);

	// 小助手：配置按钮（黑白=未选中，彩色=选中）
	auto setupBtn = [&](QPushButton* b, const char* off, const char* on) {
		QIcon icon;
		icon.addPixmap(QPixmap(off), QIcon::Normal, QIcon::Off); // 未选中
		icon.addPixmap(QPixmap(on), QIcon::Normal, QIcon::On);  // 选中
		b->setIcon(icon);

		b->setCheckable(true);       // 需要两态
		b->setText(QString());       // 不要文字
		b->setFlat(true);
		b->setCursor(Qt::PointingHandCursor);

		// 去掉内边距与边框，保持图标尽可能铺满（仍等比居中，不变形）
		b->setStyleSheet(
			"QPushButton { border: none; padding: 0; }"
			"QPushButton:hover  { background: rgba(255,255,255,0.06); }"
			"QPushButton:checked{ background: rgba(60,130,255,0.12); border-radius: 8px; }"
		);

		// 安装过滤器，自动同步 iconSize = 按钮内容区域大小
		b->installEventFilter(fit);
	};

	// 应用到你的四个按钮
	setupBtn(ui.btn_autoStudy, ":/images/images/autoStudy_bw.png", ":/images/images/autoStudy_color.png");
	setupBtn(ui.btn_toolStudy, ":/images/images/toolStudy_bw.png", ":/images/images/toolStudy_color.png");
	setupBtn(ui.btn_targetStudy, ":/images/images/targetStudy_bw.png", ":/images/images/targetStudy_color.png");
	setupBtn(ui.btn_groundStudy, ":/images/images/groundStudy_bw.png", ":/images/images/groundStudy_color.png");

	// 互斥选择（保持原有）
	auto *group = new QButtonGroup(this);
	group->setExclusive(true);
	group->addButton(ui.btn_autoStudy);
	group->addButton(ui.btn_toolStudy);
	group->addButton(ui.btn_targetStudy);
	group->addButton(ui.btn_groundStudy);
	connect(ui.btn_moveUp, &QPushButton::clicked, ui.mat_drawGround, &MatView::moveSmallCrossUp);
	connect(ui.btn_moveDown, &QPushButton::clicked, ui.mat_drawGround, &MatView::moveSmallCrossDown);
	connect(ui.btn_moveLeft, &QPushButton::clicked, ui.mat_drawCarLine, &MatView::moveBigCrossLeft);
	connect(ui.btn_moveRight, &QPushButton::clicked, ui.mat_drawCarLine, &MatView::moveBigCrossRight);
	connect(ui.mat_drawGround, &MatView::smallCrossYChanged, this, [=](int value) {
		ui.label_yPos->setText(QString::number(value));
		// 处理值变化
		}); 
	connect(ui.mat_drawCarLine, &MatView::bigCrossXChanged, this, [=](int value) {
			ui.label_xPos->setText(QString::number(value));
			// 处理值变化
			});
	ui.mat_drawTargetStudy->setMiddleLineVisible(true);
	ui.btn_nextStep->show();
	ui.btn_prevStep->hide();
	ui.btn_startStudy->hide();
	ui.btn_groundFinish->hide();
	ui.wgt_upDown->hide();
	ui.wgt_leftRight->hide();
}

void BinoularCamerasDebug::initStudyWinUi()
{
	ui.toolButton_carLine->setCheckable(true);
	ui.toolButton_carLine->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.toolButton_carLine->setIconSize(QSize(96, 96));
	QIcon carLineIcon;
	carLineIcon.addPixmap(QPixmap(":/images/images/carLine.png"), QIcon::Normal, QIcon::On);
	carLineIcon.addPixmap(QPixmap(":/images/images/carLine_gray.png"), QIcon::Normal, QIcon::Off);
	ui.toolButton_carLine->setIcon(carLineIcon);
	ui.toolButton_carLine->setChecked(false);
	ui.checkBox_showDriveArea->setChecked(false);

	connect(ui.toolButton_carLine, &QToolButton::toggled, this, [=](bool checked) {
		if (!checked) {
			ui.checkBox_showDriveArea->setChecked(checked);
		}
		ui.checkBox_showDriveArea->setCheckable(checked);
	});

	ui.toolButton_obs->setCheckable(true);
	ui.toolButton_obs->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	ui.toolButton_obs->setIconSize(QSize(96, 96));
	QIcon obsIcon;
	obsIcon.addPixmap(QPixmap(":/images/images/obstacle.png"), QIcon::Normal, QIcon::On);
	obsIcon.addPixmap(QPixmap(":/images/images/obs_gray.png"), QIcon::Normal, QIcon::Off);
	ui.toolButton_obs->setIcon(obsIcon);
	ui.toolButton_obs->setChecked(false);
	auto *g = new QButtonGroup(this);
	g->setExclusive(true);
	g->addButton(ui.radioButton_onlyDis);
	g->addButton(ui.radioButton_obsInfo);
	ui.radioButton_onlyDis->setChecked(true);
	ui.radioButton_onlyDis->setCheckable(false);
	ui.radioButton_obsInfo->setCheckable(false);
	ui.checkBox_nearestObs->setCheckable(false);

	connect(ui.radioButton_obsInfo, &QRadioButton::toggled, ui.matLeft_study, &MatView::setObstacleSummaryVisible);//setDrawObstacles
	connect(ui.toolButton_obs, &QToolButton::toggled, this, [=](bool checked) {

		ui.matLeft_study->setDrawObstacles(checked);
		ui.radioButton_onlyDis->setCheckable(checked);
		ui.radioButton_obsInfo->setCheckable(checked);
		ui.checkBox_nearestObs->setCheckable(checked);
		if (!checked) {
			ui.radioButton_onlyDis->setChecked(checked);
			ui.radioButton_obsInfo->setChecked(checked);
			ui.checkBox_nearestObs->setChecked(checked);
		}
		else {
			ui.radioButton_onlyDis->setChecked(checked);
		}
		});
	//connect(ui.stackedWidget_4, &QStackedWidget::currentChanged, this, [=](int idx) {
	//	if (ui.stackedWidget_4->currentWidget() != ui.page_leftCamera) {
	//		ui.toolButton_carLine->setCheckable(false);
	//		ui.toolButton_obs->setCheckable(false);
	//	}
	//	else {
	//		ui.toolButton_carLine->setCheckable(true);
	//		ui.toolButton_obs->setCheckable(true);
	//	}
	//	// TODO: 切换后的处理
	//	});
	connect(ui.btn_acquisition, &QPushButton::toggled, this, &BinoularCamerasDebug::slotSavePicture);
	ui.stackedWidget_4->setCurrentWidget(ui.page_leftCamera);
	ui.legend->setItems({
	{"Continuous Obstacle Box", QColor(112, 48, 160), true},              // 空心紫框（可选）
	{"Pedestrian",        colorForType(ObstacleType::Pedestrian), false},
	{"Cyclist",      colorForType(ObstacleType::Cyclist),    false},
	{"Motor Vehicle",    colorForType(ObstacleType::Vehicle),    false},
	{"Other",      colorForType(ObstacleType::Other),      false},
	{"Nearest Obstacle Box", QColor(255, 0, 0), false}        // 最近：红色
		});


	connect(ui.mat_drawCarLine, &MatView::laneGeometryUpdated, ui.matLeft_study, &MatView::setExternalLaneGeometry);
	connect(ui.checkBox_nearestObs, &QCheckBox::toggled, this, &BinoularCamerasDebug::slotSetShowNearestObs);
	connect(ui.checkBox_showDriveArea, &QCheckBox::toggled, this, &BinoularCamerasDebug::slotSetShowDriveArea);
}

void BinoularCamerasDebug::updateTakePhotoBtnStatus(bool flag)
{
	if (ui.btn_takePhoto->isEnabled() == flag) {
		return;
	}
	ui.btn_takePhoto->setEnabled(flag);
}

void BinoularCamerasDebug::applyIntRange(QLineEdit * le, int min, int max, QObject * parent)
{

}

void BinoularCamerasDebug::setupIntLine(QLineEdit * edit, int min, int max)
{
	// 仅限制“整数字符”，允许为空（空是否允许你可以自己改在 checkRange 里管）
	const bool allowNegative = (min < 0);
	QRegularExpression re(allowNegative ? "-?\\d*" : "\\d*");
	edit->setValidator(new QRegularExpressionValidator(re, edit));

	// 记住每个框的范围（存在控件属性里）
	edit->setProperty(kMinProp, min);
	edit->setProperty(kMaxProp, max);

	// 统一拦截焦点/按键事件
	edit->installEventFilter(this);

	// 友好提示（可选）
	edit->setPlaceholderText(QString("%1 - %2").arg(min).arg(max));
}

void BinoularCamerasDebug::checkRange(QLineEdit * edit)
{
	if (!edit->property(kMinProp).isValid() || !edit->property(kMaxProp).isValid())
		return; // 这个框没配置范围就不管

	const int min = edit->property(kMinProp).toInt();
	const int max = edit->property(kMaxProp).toInt();

	const QString s = edit->text();

	// 如果你想“不能为空”，把这段改成弹窗即可
	if (s.isEmpty()) return;

	bool ok = false;
	const int v = s.toInt(&ok);
	if (!ok || v < min || v > max) {
		// 提示 + 把焦点拉回去，选中便于重输
		QMessageBox::warning(this, "Input Error",
			QString("Enter %1 - %2 an integer between！").arg(min).arg(max));
		edit->setFocus();
		edit->selectAll();
	}
}


void BinoularCamerasDebug::initDebugBtnStyle(QToolButton* b, const QString& text, const QIcon& icon)
{
	//b->setText(text);
	b->setIcon(icon);                          // 左侧图标（png/jpg/svg 都行）
	b->setIconSize(QSize(40, 40));
	b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); // 图标在左、文字在右
	b->setCheckable(true);                     // 允许选中高亮
}

void BinoularCamerasDebug::setDebugBtnValidated(QToolButton * b, bool ok)
{
	b->setProperty("validated", ok);   // 关键：动态属性驱动样式选择器
	b->style()->unpolish(b);           // 触发样式重算（保险）
	b->style()->polish(b);
	b->update();
}

void BinoularCamerasDebug::saveImage(cv::Mat img, QString path)
{
	if (img.empty()) { qWarning() << "saveImage: img is empty"; return; }

	// 只看“是否可写”，不要再用 isUnderProgramFiles()
	QString outDir = path;
	QString reason;
	if (outDir.isEmpty() || !ensureWritableDir(outDir, &reason)) {
		qWarning() << "saveImage: ensureWritableDir failed for" << outDir << ":" << reason;
		outDir = pickFallbackDir();              // 你原先的回退函数
		QString why2;
		if (!ensureWritableDir(outDir, &why2)) {
			qWarning() << "saveImage: no writable directory found:" << why2;
			return;
		}
	}


	// 2) 生成文件名（默认 png；要 jpg 只改扩展名）
	const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz");
	const QString filename = ts + ".png";
	const QString fullPath = QDir(outDir).filePath(filename);

	// 3) 准备图像：统一到 8bit，并转换到 Qt 需要的颜色格式
	cv::Mat mat8 = to8uIfNeeded(img);
	cv::Mat conv = mat8;
	QImage::Format qfmt = QImage::Format_Grayscale8;

	if (mat8.channels() == 1) {
		qfmt = QImage::Format_Grayscale8;
	}
	else if (mat8.channels() == 3) {
		cv::cvtColor(mat8, conv, cv::COLOR_BGR2RGB);
		qfmt = QImage::Format_RGB888;
	}
	else if (mat8.channels() == 4) {
		cv::cvtColor(mat8, conv, cv::COLOR_BGRA2RGBA);
		qfmt = QImage::Format_RGBA8888;
	}
	else {
		qWarning() << "saveImage: unsupported channel count:" << mat8.channels();
		return;
	}

	// 4) 用 Qt 保存（原生支持中文路径）
	QImage qimg(conv.data, conv.cols, conv.rows, int(conv.step), qfmt);
	const bool ok = qimg.save(fullPath);  // 依赖 qpng/qjpeg 插件

	if (!ok) {
		qWarning() << "saveImage: QImage::save failed:" << fullPath
			<< " (check that plugins/imageformats/qpng.dll or qjpeg.dll is packaged)";
		return;
	}

	qDebug() << "saveImage: saved to" << fullPath;
}

void BinoularCamerasDebug::checkResizeDirection(const QPoint & cursorPos)
{
	int x = cursorPos.x();
	int y = cursorPos.y();
	int w = width();
	int h = height();
	int margin = 4; // 边界范围

	resizeDirection = Qt::Edges();

	if (x < margin) resizeDirection |= Qt::LeftEdge;
	if (x > w - margin) resizeDirection |= Qt::RightEdge;
	if (y < margin) resizeDirection |= Qt::TopEdge;
	if (y > h - margin) resizeDirection |= Qt::BottomEdge;
}

void BinoularCamerasDebug::resizeWindow(const QPoint & delta)
{
	QRect newGeom = geometry();

	if (resizeDirection & Qt::LeftEdge) {
		newGeom.setLeft(qMin(newGeom.right() - 100, newGeom.left() + delta.x())); // 最小宽度100
	}
	if (resizeDirection & Qt::RightEdge) {
		newGeom.setRight(qMax(newGeom.left() + 100, newGeom.right() + delta.x()));
	}
	if (resizeDirection & Qt::TopEdge) {
		newGeom.setTop(qMin(newGeom.bottom() - 100, newGeom.top() + delta.y())); // 最小高度100
	}
	if (resizeDirection & Qt::BottomEdge) {
		newGeom.setBottom(qMax(newGeom.top() + 100, newGeom.bottom() + delta.y()));
	}

	setGeometry(newGeom);
}

void BinoularCamerasDebug::updateCursorShape(const QPoint & cursorPos)
{
	if (isMaximized()) {
		setCursor(Qt::ArrowCursor);  // 🔹 窗口最大化时强制鼠标箭头
		return;
	}

	checkResizeDirection(cursorPos);

	if (resizeDirection == (Qt::LeftEdge | Qt::TopEdge) || resizeDirection == (Qt::RightEdge | Qt::BottomEdge)) {
		setCursor(Qt::SizeFDiagCursor);
	}
	else if (resizeDirection == (Qt::RightEdge | Qt::TopEdge) || resizeDirection == (Qt::LeftEdge | Qt::BottomEdge)) {
		setCursor(Qt::SizeBDiagCursor);
	}
	else if (resizeDirection & (Qt::LeftEdge | Qt::RightEdge)) {
		setCursor(Qt::SizeHorCursor);
	}
	else if (resizeDirection & (Qt::TopEdge | Qt::BottomEdge)) {
		setCursor(Qt::SizeVerCursor);
	}
	else {
		setCursor(Qt::ArrowCursor);
	}
}

void BinoularCamerasDebug::enableMouseTrackingForAll(QWidget * parent)
{
	if (!parent) return;

	parent->setMouseTracking(true);

	// 遍历所有子控件
	foreach(QObject *obj, parent->children()) {
		QWidget *widget = qobject_cast<QWidget *>(obj);
		if (widget) {
			enableMouseTrackingForAll(widget);  // 递归调用
		}
	}
}


void BinoularCamerasDebug::updateCamreaPage()
{
	QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
	if (clickedButton == ui.btn_leftCamera) {
		ui.stackedWidget_2->setCurrentIndex(0);
	}
	else {
		ui.stackedWidget_2->setCurrentIndex(1);
	}
}

void BinoularCamerasDebug::setDebugNextPage()
{
	int curIndex = ui.stackedWidget->currentIndex();
	ui.stackedWidget->setCurrentIndex(curIndex + 1);
	if (curIndex == 0) {
		ui.btn_install->setChecked(true);
	}
	else if (curIndex == 1) {
		ui.btn_biaoding->setChecked(true);
	}
	else if (curIndex == 2) {
		ui.btn_study->setChecked(true);
	}
	else if (curIndex == 3) {
		ui.btn_yjset->setChecked(true);
	}
	else if (curIndex == 4) {
	}
}

void BinoularCamerasDebug::SaveInstall()
{
	if (ui.lineEdit_left2ground->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Left camera height cannot be empty。");
		return;
	}
	else if (ui.lineEdit_left2glassL->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Distance from the left camera to the left windshield edge cannot be empty。");
		return;
	}
	else if (ui.lineEdit_left2bxg->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Distance from the left camera to the bumper cannot be empty。");
		return;
	}
	else if (ui.lineEdit_left2glassR->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Distance from the left camera to the right windshield edge cannot be empty。");
		return;
	}
	else if (ui.lineEdit_car2ground->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Vehicle-front ground clearance cannot be empty。");
		return;
	}
	else if (ui.lineEdit_carqljj->text().isEmpty()) {
		QMessageBox::information(this, "Information", "Front-wheel spacing cannot be empty。");
		return;
	}

	setDebugBtnValidated(ui.btn_install, true);
	setDebugNextPage();
}

void BinoularCamerasDebug::SaveConfig()
{
	setDebugBtnValidated(ui.btn_cfg, true);
	setDebugNextPage();
}

void BinoularCamerasDebug::SaveBiaoding()
{
	b_biaodingFinish = true;
	setDebugBtnValidated(ui.btn_biaoding, true);
	setDebugNextPage();
}

void BinoularCamerasDebug::btn_takePhoto_clicked()
{
	if (curBiaodingDistance == 5.0) {
		ui.checkBox_yp5m->setChecked(true);
		curBiaodingDistance = 10.0;
		ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);
	}
	else if (curBiaodingDistance == 10.0)
	{
		ui.checkBox_yp10m->setChecked(true);
		curBiaodingDistance = 15.0;
		ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);
	}
	else {
		ui.checkBox_yp15m->setChecked(true);
		ui.btn_takePhoto->hide();
		ui.btnSave_biaoding->show();
	}
}


void BinoularCamerasDebug::updateMainPage()
{
	QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
	if (clickedButton == ui.mainBtn_debug) {
		ui.stackedWidget_3->setCurrentWidget(ui.mainDebug);
	} else if (clickedButton == ui.mainBtn_study) {
		ui.stackedWidget_3->setCurrentWidget(ui.mainStudy);
	}
}

void BinoularCamerasDebug::updateStudyMainPage()
{
	QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
	if (clickedButton == ui.btn_leftCamera_study) {
		ui.stackedWidget_4->setCurrentWidget(ui.page_leftCamera);
	}
	else if (clickedButton == ui.btn_rightCamera_study) {
		ui.stackedWidget_4->setCurrentWidget(ui.page_rightCamera);
	}
	else if (clickedButton == ui.btn_disparity) {
		ui.stackedWidget_4->setCurrentWidget(ui.page_disparity);
	}
	else if (clickedButton == ui.btn_acquisition) {
		/*ui.stackedWidget_4->setCurrentWidget(ui.page_acquisition);*/
	}
}

void BinoularCamerasDebug::loadBiaodingImage(const QString& lin)
{
}

void BinoularCamerasDebug::updateDebugStudyPage()
{
	QPushButton *clickedButton = qobject_cast<QPushButton*>(sender());
	if (clickedButton == ui.btn_autoStudy) {
		ui.stackedWidget_study->setCurrentWidget(ui.page_autoStudy);
	}
	else if (clickedButton == ui.btn_groundStudy) {
		ui.stackedWidget_study->setCurrentWidget(ui.page_groundStudy);
		if (camera->requestStereoCameraParameters(g_cameraBiaodingParams)) {
			qDebug() << "Stereo camera parameters request succeed...........";
		}
		else {
			qDebug() << "Stereo camera parameters request failed!!!!!!!!!!!!";
		}
		if (camera->requestRotationMatrix(g_rotationMatrix)) {
			qDebug() << "Rotation Matrix request succeed...........";
		}
		else {
			qDebug() << "Rotation Matrix request failed!!!!!!!!!!!!";
		}
		groundBiaoding = true;
	}
	else if (clickedButton == ui.btn_targetStudy) {
		ui.stackedWidget_study->setCurrentWidget(ui.page_targetStudy);
	}
	else if (clickedButton == ui.btn_toolStudy) {
		ui.stackedWidget_study->setCurrentWidget(ui.page_toolStudy);
	}
	startSudy = true;
}

void BinoularCamerasDebug::returnMainStudyPage()
{
	ui.stackedWidget_study->setCurrentWidget(ui.page_mainStudy);
	startSudy = false;
}

void BinoularCamerasDebug::debugStudyNextStep()
{
	if (!ui.btn_toolStudy->isChecked() && ui.btn_autoStudy->isChecked() && ui.btn_targetStudy->isChecked() && ui.btn_groundStudy->isChecked()) {
		QMessageBox::warning(this, "Information", QString("Please select a learning method！"));
		return;
	}
    if (ui.btn_toolStudy->isChecked()) {

	}
	else if (ui.btn_autoStudy->isChecked()) {
		setDebugBtnValidated(ui.btn_study, true);
		setDebugNextPage();
	}
	else if (ui.btn_targetStudy->isChecked()) { 
		ui.stackedWidget_study->setCurrentWidget(ui.page_targetStudy);
		ui.btn_nextStep->hide();
		ui.btn_prevStep->show();
		ui.btn_startStudy->show();
		ui.btn_groundFinish->hide();
		ui.wgt_upDown->hide();
		ui.wgt_leftRight->hide();

		targetStudy = true;
	}
	else if (ui.btn_groundStudy->isChecked()) {
		ui.stackedWidget_study->setCurrentWidget(ui.page_groundStudy);
		ui.stackedWidget_groundStudy->setCurrentWidget(ui.page_drawGround);
		ui.btn_nextStep->hide();
		ui.btn_prevStep->show();
		ui.btn_startStudy->show();
		ui.btn_groundFinish->hide();
		ui.wgt_upDown->show();
		ui.wgt_leftRight->hide();
		groundBiaoding = true;
	}
}

void BinoularCamerasDebug::debugStudyPrevStep()
{
	ui.stackedWidget_study->setCurrentWidget(ui.page_mainStudy);
	ui.btn_nextStep->show();
	ui.btn_prevStep->hide();
	ui.btn_startStudy->hide();
	ui.btn_groundFinish->hide();

	ui.wgt_upDown->hide();
	ui.wgt_leftRight->hide();
}

void BinoularCamerasDebug::startDebugStudy()
{
	// 判断是靶标学习还是地面标定
	if (ui.stackedWidget_study->currentWidget() == ui.page_targetStudy) {

		// 需要判断 是否检测到靶标
		if (b_checkTarget) {
			setDebugBtnValidated(ui.btn_study, true);
			setDebugNextPage();
		}
		else {
			QMessageBox::warning(this, "Information",
				QString("Target learning failed，Check the target placement。"));
			return;
		}
	}
	else if (ui.stackedWidget_study->currentWidget() == ui.page_groundStudy) {
		ui.stackedWidget_groundStudy->setCurrentWidget(ui.page_drawCarLine);
		ui.wgt_upDown->hide();
		ui.wgt_leftRight->show();
		ui.btn_startStudy->hide();
		ui.btn_groundFinish->show();
	}
	drawCarLine = true;
}

void BinoularCamerasDebug::slotDebugBtnClicked()
{
	auto* b = qobject_cast<QToolButton*>(sender());
	// 若已标记“验证通过”，先确认
	if (b->property("validated").toBool()) {
		//auto ret = QMessageBox::question(
		//	this, tr("提示"),
		//	tr("该项已验证通过，继续将清除通过标记。是否继续？"),
		//	QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

		//if (ret != QMessageBox::Yes) {
		//	// 取消：彻底不改变状态 —— 把“自动切换”的选中状态复原
		//	if (currentBtn_ && currentBtn_ != b) {
		//		QSignalBlocker blk1(b), blk2(currentBtn_);
		//		b->setChecked(false);          // 取消本次被选中
		//		currentBtn_->setChecked(true); // 还原原先选中的按钮
		//	}
		//	return; // 不执行后续动作
		//}

		//// 确定：清除通过标记（QSS 变回非绿）
		//setDebugBtnValidated(b, false);
		//// 继续往下走，执行后续动作
	}
	//ui.btn_cfg, ui.btn_install, ui.btn_biaoding, ui.btn_study, ui.btn_yjset
	if (b == ui.btn_cfg) {
		ui.stackedWidget->setCurrentWidget(ui.page_config);
	}
	else if (b == ui.btn_install) {
		ui.stackedWidget->setCurrentWidget(ui.page_install);
	}
	else if (b == ui.btn_biaoding) {
		if (b_biaodingFinish) {
			QMessageBox box(this);
			box.setIcon(QMessageBox::Question);
			box.setWindowTitle(tr("Camera Calibration"));
			box.setText(tr("Camera calibration is complete，Run camera calibration again？"));
			box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

			// 手动改按钮文字（可选）
			box.setButtonText(QMessageBox::Ok, tr("Recalibrate"));
			box.setButtonText(QMessageBox::Cancel, tr("Cancel"));

			int ret = box.exec();
			if (ret == QMessageBox::Cancel) {
				return;
			}
			else {
				b_biaodingFinish = false;
				curBiaodingDistance = 5;
				ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);
				setDebugBtnValidated(ui.btn_biaoding, false);

				ui.checkBox_yp5m->setChecked(false);
				ui.checkBox_yp10m->setChecked(false);
				ui.checkBox_yp15m->setChecked(false);
				ui.checkBox_load5m->setChecked(false);
				ui.checkBox_load10m->setChecked(false);
				ui.checkBox_load15m->setChecked(false);
			}

		}
		ui.stackedWidget->setCurrentWidget(ui.page_biaoding);
	}
	else if (b == ui.btn_study) {
		if (b_studyFinish) {
			QMessageBox box(this);
			box.setIcon(QMessageBox::Question);
			box.setWindowTitle(tr("Pose Learning"));
			box.setText(tr("Pose learning is complete，Learn again？"));
			box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

			// 手动改按钮文字（可选）
			box.button(QMessageBox::Ok)->setText(QStringLiteral("Relearn"));
			box.button(QMessageBox::Cancel)->setText(QStringLiteral("Cancel"));

			int ret = box.exec();
			if (ret == QMessageBox::Cancel) {
				return;
			}
			b_studyFinish = false;
			setDebugBtnValidated(ui.btn_study, false);
		}
		ui.stackedWidget->setCurrentWidget(ui.page_study);
		ui.stackedWidget_study->setCurrentWidget(ui.page_mainStudy);
	}
	else if (b == ui.btn_yjset) {
		ui.stackedWidget->setCurrentWidget(ui.page_yjset);
	}
}

void BinoularCamerasDebug::slotFinishGroundStudy()
{
	setDebugBtnValidated(ui.btn_study, true);
	ui.mat_drawCarLine->sendLaneGeometry();
	setDebugNextPage();
}

void BinoularCamerasDebug::slotStudyPageChanged(int page)
{
	qDebug() << page;
	if (page == 0) {
		// 主页
		ui.btn_nextStep->show();
		ui.btn_prevStep->hide();
		ui.btn_startStudy->hide();
		ui.btn_groundFinish->hide();
		ui.wgt_upDown->hide();
		ui.wgt_leftRight->hide();
	}
	else if (page == 1) {
		// 
	}
	else if (page == 2) {

	}
	else if (page == 3) {
		// 靶标

	}
	else if (page == 4) {
		// 地面标定

	}
}

void BinoularCamerasDebug::slotGroundStudyPageChanged(int page)
{
	if (page == 0) {
		// page_drawGround
		ui.btn_nextStep->hide();
		ui.btn_prevStep->show();
		ui.btn_startStudy->show();
		ui.btn_groundFinish->hide();
		ui.wgt_upDown->show();
		ui.wgt_leftRight->hide();

	}
	else if (page == 1) {
		// page_drawCarLine
		ui.wgt_upDown->hide();
		ui.wgt_leftRight->show();
		ui.btn_startStudy->hide();
		ui.btn_groundFinish->show();
	}
}

void BinoularCamerasDebug::slotFinishDebug()
{
	// 判断其它四个按钮是否都是true
	if (!ui.btn_cfg->property("validated").toBool()) {
		QMessageBox::information(this, "Information", "Complete communication settings first。");
		return;
	}
	if (!ui.btn_install->property("validated").toBool()) {
		QMessageBox::information(this, "Information", "Complete camera installation first。");
		return;
	}
	if (!ui.btn_biaoding->property("validated").toBool()) {
		QMessageBox::information(this, "Information", "Complete camera calibration first。");
		return;
	}
	if (!ui.btn_study->property("validated").toBool()) {
		QMessageBox::information(this, "Information", "Complete pose learning first。");
		return;
	}
	setDebugBtnValidated(ui.btn_yjset, true);
	b_debugFinish = true;

	ui.mainBtn_study->setEnabled(true);

	QMetaObject::invokeMethod(ui.mainBtn_study, "click");
}

void BinoularCamerasDebug::slotSavePicture(bool checked)
{
	b_saveImages = checked;
	ui.btn_acquisition->setText(checked ? tr("Stop Capture") : tr("Image Capture"));
}

void BinoularCamerasDebug::slotSetShowNearestObs(bool checked)
{
	ui.matLeft_study->setObstaclesOnlyInsideLane(checked);
}

void BinoularCamerasDebug::slotSetShowDriveArea(bool checked)
{
	ui.matLeft_study->setUseExternalLaneGeometry(checked);
	ui.matLeft_study->setDrawLaneLines(checked);
}

void BinoularCamerasDebug::Biaoding5MNextStep(bool flag)
{
	curBiaodingDistance = 10.0;
	ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);
}

void BinoularCamerasDebug::Biaoding10MNextStep(bool flag)
{
	curBiaodingDistance = 15.0;
	ui.matLeft_debugBiaoding->setDistanceMeters(curBiaodingDistance);
}

void BinoularCamerasDebug::Biaoding15MNextStep(bool flag)
{
	b_biaodingFinish = true;
	setDebugBtnValidated(ui.btn_biaoding, true);
	setDebugNextPage();
}


void BinoularCamerasDebug::onFrameUpdate()
{
	frameMonitor->waitForFrames(); // 阻塞等待帧（如不想阻塞可移出）

	camera->invokeInLoopThread([this] {
		cv::Mat leftFrame = frameMonitor->getFrameMat(FrameId::CalibLeftCamera);
		cv::Mat rightFrame = frameMonitor->getFrameMat(FrameId::RightCamera);
		cv::Mat disparity = frameMonitor->getFrameMat(FrameId::Disparity);
		uint32_t curSpeed = frameMonitor->getSpeed();
		QVector<ObstacleDet> dets = frameMonitor->getObstacleDetVec();
		QSize sourceSize = frameMonitor->getSrcSize();
		ui.speedmeter->setSpeed(curSpeed);
		if (!leftFrame.empty()) {
			if (b_saveImages) {
				QString path =  QCoreApplication::applicationDirPath() + "/Recorded_Images/" + QDateTime::currentDateTime().toString("yyyyMMdd") + "/left_images";
				saveImage(leftFrame, path);
			}

			cv::Mat imgsss = leftFrame.clone();
			cv::Mat imgsss1 = leftFrame.clone();
			cv::Mat imgDrawLine = leftFrame.clone();
			cv::Mat targetStudyFrame = leftFrame.clone();
			ui.matLeft_debugInstall->setFrame(leftFrame);
			ui.matLeft_debugBiaoding->setFrame(leftFrame);
			ui.matLeft_study->setFrame(leftFrame);
			ui.matLeft_study->setObstacleDetections(dets, sourceSize);
			ui.mat_drawTargetStudy->setFrame(leftFrame);

			ui.mat_drawGround->setFrame(leftFrame);
			ui.mat_drawCarLine->setFrame(leftFrame);
			CameraSpec cam{ 1280, 720, 38.0, 21.0 };    // HFOV=38°, VFOV=21°（imgW/H 不用也无妨）
			BoardSpec  bd;                             // 默认 0.20m x 0.80m, 2x8
			double distance_m = 10.0;                  // 距离（单位：米）

			// 结果输出变量（可选）
			cv::Rect detRect, roiRect;
			double score = 0.0;

			bool centered = detectBoardAtDistance(
				leftFrame,           // in/out：会在图上画黄色框
				curBiaodingDistance,
				cam, bd,
				1.2,             // searchScale：在 1.2w × 1.2h 内搜索
				0.60,            // score 阈值
				&detRect,        // 可为 nullptr
				&roiRect,        // 可为 nullptr
				&score           // 可为 nullptr
			);
			//cv::imshow("test", leftFrame);
			if (score >= 0.6) {
				updateTakePhotoBtnStatus(true);
				ui.matLeft_debugBiaoding->setOverlayColor(MatView::OverlayColor::Green);
			}
			else {
				updateTakePhotoBtnStatus(false);
				ui.matLeft_debugBiaoding->setOverlayColor(MatView::OverlayColor::Red);
			}
			if (groundBiaoding) {
				if (camera->requestStereoCameraParameters(g_cameraBiaodingParams)) {
					qDebug() << "Stereo camera parameters request succeed...........";
				}
				else {
					qDebug() << "Stereo camera parameters request failed!!!!!!!!!!!!";
				}
				if (camera->requestRotationMatrix(g_rotationMatrix)) {
					qDebug() << "Rotation Matrix request succeed...........";
				}
				else {
					qDebug() << "Rotation Matrix request failed!!!!!!!!!!!!";
				}

				auto& m = g_rotationMatrix.real3DToImage;
				qDebug("M: %.6f %.6f %.6f %.6f | %.6f %.6f %.6f %.6f | %.6f %.6f %.6f %.6f",
					m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11]);

				float Znear = 4.5f;          // 近端 4 m
				float Zfar = 35.0f;         // 远端 25 m
				float Wnear = std::max(1.8f, g_cameraInfoParams.carWidth * 0.9f); // 近端半宽可略窄
				float Wfar = std::max(2.8f, g_cameraInfoParams.carWidth * 1.3f); // 远端稍放宽
				// worldYMode：0 表示地面 Yw=0；1 表示按文档“Yw=相机安装高度”的口径。二选一按你的矩阵口径来。
				drawGroundRegion(imgsss, g_rotationMatrix, g_cameraInfoParams, Znear, Zfar, Wnear, Wfar, /*worldYMode=*/0);
				// 显示或保存
				//cv::imshow("region", imgsss);

				ui.mat_drawGround->setDrawGroundBox(true);
				ui.mat_drawGround->setGroundRegion(
					/*Znear*/ 4.5f,
					/*Zfar */ 35.0f,
					/*Wnear*/ std::max(1.8f, g_cameraInfoParams.carWidth * 0.9f),
					/*Wfar */ std::max(2.8f, g_cameraInfoParams.carWidth * 1.3f),
					/*worldYSign*/ +1,  // 你现场验证过用 +H 正确
					/*thickness*/ 2,
					/*color*/ QColor(0, 255, 0)
				);

			}
			if (drawCarLine) {
				ui.mat_drawCarLine->setDrawLaneLines(true);
				ui.mat_drawCarLine->setLaneParams(/*useCarWidth*/ true, /*laneWidth*/ 3.5f,
					/*maxDist*/ 35.0f, /*thickness*/ 2, /*color*/ QColor(255, 0, 0));

				drawLaneLines(imgDrawLine, g_rotationMatrix, g_cameraInfoParams, 35);
				//cv::imshow("CarLine", imgDrawLine);
			}

			if (targetStudy) {
				cv::Rect detRect1, searchRoi;
				double score1 = 0.0;
				BoardSpec  bdH; // 横置
				bdH.physW_m = 0.80; bdH.physH_m = 0.20;
				bdH.cols = 8;       bdH.rows = 2;
				bool ok = detectBoardAtDistanceHorizontal(
					targetStudyFrame,
					5.0,           // 距离 Z = 5m
					cam,
					bdH,
					2.0,           // searchScale：ROI 放大倍数，推荐 1.8~2.2
					0.35,           // scoreThresh：匹配分数阈值
					&detRect1,      // 输出检测矩形
					&searchRoi,    // 输出 ROI
					&score1         // 输出分数
				);
				if (score1 > 0.65) {
					b_checkTarget = true;
				}
				else {
					b_checkTarget = false;
				}
				//cv::imshow("targetStudy", targetStudyFrame);
			}

		}

		if (!rightFrame.empty()) {
			if (b_saveImages) {
				QString path = QCoreApplication::applicationDirPath() + "/Recorded_Images/" + QDateTime::currentDateTime().toString("yyyyMMdd") + "/right_images";
				saveImage(rightFrame, path);
			}
			ui.matRight_debugInstall->setFrame(rightFrame);
			ui.matRight_study->setFrame(rightFrame);
		}

		if (!disparity.empty()) {
			if (b_saveImages) {
				QString path = QCoreApplication::applicationDirPath() + "/Recorded_Images/" + QDateTime::currentDateTime().toString("yyyyMMdd") + "/disparity_images";
				saveImage(disparity, path);
			}
			ui.mat_disparity->setFrame(disparity); // .copy() 确保数据生命周期
		}
		});
}