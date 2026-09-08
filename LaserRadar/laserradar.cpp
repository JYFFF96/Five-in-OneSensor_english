#include "laserradar.h"
#include "ui_laserradar.h"
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QMenu>
#include "exporter.h"
#include <QThread>
#include <QFileDialog>
#include <QMessageBox>
#include "ReplayWindow.h"
#include <QtConcurrent/QtConcurrent>
#include <QSvgWidget>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <Eigen/Dense>
LaserRadar::LaserRadar(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LaserRadar)
{
    ui->setupUi(this);
    QIcon icon(":/images/logo.png");
    this->setWindowIcon(icon);
    this->setWindowTitle("LiDAR Debugger");
    this->setWindowFlags(Qt::FramelessWindowHint); // 隐藏标题栏
    // this->setAttribute(Qt::WA_TranslucentBackground);
    initUI();
    initTittleBar();
    initWidgets(ui->wgt_dataShow, 800, this->height()-30);
    initWidgets(ui->wgt_basicSetting, 600, 280);
    initWidgets(ui->wgt_colorSetting, 220, 180);
    initWidgets(ui->wgt_radarBD, 380, 200);
    initWidgets(ui->wgt_dylb, 400, 240);
    initWidgets(ui->wgt_obs, 450, 240);

    connect(ui->btn_closeBasicSetting, &QPushButton::clicked, this, &LaserRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeRadarBD, &QPushButton::clicked, this, &LaserRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeColorSetting, &QPushButton::clicked, this, &LaserRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeObs, &QPushButton::clicked, this, &LaserRadar::wgtCloseBtnClicked);
    connect(ui->btn_closesdylb, &QPushButton::clicked, this, &LaserRadar::wgtCloseBtnClicked);

    // 首页
    connect(ui->btnFirst, &QPushButton::clicked, this, [=]() {
        model->setCurrentPage(0);
        ui->spinPage->setValue(model->currentPageIndex() + 1);
        ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
    });

    // 上一页
    connect(ui->btnPrev, &QPushButton::clicked, this, [=]() {
        int page = model->currentPageIndex();
        if (page > 0) {
            model->setCurrentPage(page - 1);
            ui->spinPage->setValue(model->currentPageIndex() + 1);
            ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
        }
    });

    // 下一页
    connect(ui->btnNext, &QPushButton::clicked, this, [=]() {
        int page = model->currentPageIndex();
        if (page < model->pageCount() - 1) {
            model->setCurrentPage(page + 1);
            ui->spinPage->setValue(model->currentPageIndex() + 1);
            ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
        }
    });

    // 尾页
    connect(ui->btnLast, &QPushButton::clicked, this, [=]() {
        if (model->pageCount() > 0) {
            model->setCurrentPage(model->pageCount() - 1);
            ui->spinPage->setValue(model->currentPageIndex() + 1);
            ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
        }
    });

    // 跳转按钮
    connect(ui->btnGoto, &QPushButton::clicked, this, [=]() {
        int targetPage = ui->spinPage->value() - 1;
        if (targetPage >= 0 && targetPage < model->pageCount()) {
            model->setCurrentPage(targetPage);
            ui->spinPage->setValue(model->currentPageIndex() + 1);
            ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
        }
    });

    connect(ui->lineEdit_fileter, &QLineEdit::editingFinished, this, &LaserRadar::updateObsParams);
    connect(ui->lineEdit_dis, &QLineEdit::editingFinished, this, &LaserRadar::updateObsParams);
    connect(ui->lineEdit_min_points, &QLineEdit::editingFinished, this, &LaserRadar::updateObsParams);
    connect(ui->lineEdit_max_points, &QLineEdit::editingFinished, this, &LaserRadar::updateObsParams);


    this->centralWidget()->setLayout(ui->verticalLayout_6);
    enableMouseTrackingForAll(this);
}

LaserRadar::~LaserRadar()
{
    delete ui;
}

void LaserRadar::resizeEvent(QResizeEvent *event)
{
    ui->wgt_dataShow->setGeometry(ui->wgt_dataShow->x(),30,800,this->height()-30);
    if (ui->wgt_dataShow->x() > 0) {
        // 设置变化
        ui->wgt_dataShow->setGeometry(5,30,800,this->height()-30);
    }
}

void LaserRadar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *clickedWidget = childAt(event->pos());

        // 仅当鼠标点击 wgt_top 时，才允许拖动窗口
        bool canDrag = (clickedWidget == ui->wgt_top);

        if (clickedWidget && clickedWidget->inherits("QToolButton")) {
            return;  // 点击的是按钮，不执行拖动或缩放
        }

        mousePressed = true;
        lastMousePos = event->globalPosition().toPoint();
        isDragging = false;
        isResizing = false;
        resizeDirection = Qt::Edges();

        if (isMaximized()) {
            if (canDrag) {
                isDragging = true;  // 仅允许拖动，不调整大小
                normalGeometry = geometry();
            }
        } else {
            checkResizeDirection(event->pos());
            if (resizeDirection != Qt::Edges()) {
                isResizing = true;  // 进入调整大小模式
            } else if (canDrag) {
                isDragging = true;  // 进入拖动模式
            }
        }
    }
}

void LaserRadar::mouseMoveEvent(QMouseEvent *event)
{
    if (!mousePressed) {
        if (!isMaximized()) {  // 🔹 窗口最大化时不更新鼠标光标
            updateCursorShape(event->pos());
        } else {
            setCursor(Qt::ArrowCursor);
        }
        return;
    }

    QPoint delta = event->globalPosition().toPoint() - lastMousePos;

    if (isMaximized() && isDragging) {
        // 退出最大化，并调整窗口位置
        showNormal();
        maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        move(event->globalPosition().toPoint() - QPoint(width() / 2, 10));
    } else if (isResizing && !isMaximized()) {  // 🔹 窗口最大化时不能调整大小
        resizeWindow(delta);
    } else if (isDragging) {  // 只有在 isDragging 允许时才拖动窗口
        move(pos() + delta);
    }

    lastMousePos = event->globalPosition().toPoint();
}

void LaserRadar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
    isDragging = false;
    isResizing = false;
    resizeDirection = Qt::Edges();
}

void LaserRadar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *clickedWidget = childAt(event->pos());

        // 仅当鼠标点击 wgt_top 时，才允许拖动窗口
        if (clickedWidget == ui->wgt_top) {
            if (isMaximized()) {
                showNormal();
                maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
            } else {
                showMaximized();
                maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
            }

        }
    }
}

void LaserRadar::initUI()
{
    setupButtons();
    colorBar = new ColorBarWidget(this);
    colorBar->hide();
    connect(ui->wgt_gradientEditor, &GradientEditorWidget::stopsChanged, this, [=](const QVector<QPair<double, QColor>> &stops){
        // 把 stops 传回 ColorBarWidget
        colorBar->setColorStops(stops);
    });
    connect(colorBar, &ColorBarWidget::hideWin, this, [=]{
        ui->checkBox->setChecked(false);
    });
    ui->pointCloud->setColorBarWidget(colorBar);
    udp = new UdpReceiver(this);
    // 点云原始数据（PointXYZI）实时更新 → 自动滤波
    connect(udp, &UdpReceiver::dataUpdated, this, &LaserRadar::onPointDataUpdated);

    // 点云表格用 PointData
    connect(udp, &UdpReceiver::pointDataUpdated, this, &LaserRadar::updatePointData);
    qRegisterMetaType<ExportParams>("ExportParams");
    connect(this, &LaserRadar::signalExportToUDP, udp, &UdpReceiver::onExportRequested);
    connect(udp, &UdpReceiver::exportFinished, this, &LaserRadar::onExportFinished);
    // Exporter* exporter = new Exporter;
    // QThread*  expThread = new QThread;

    // exporter->moveToThread(expThread);
    // connect(expThread, &QThread::finished,
    //         exporter,   &QObject::deleteLater);   // 线程结束时销毁 Exporter
    // expThread->start();

    // // 使用...
    // connect(udp, &UdpReceiver::exportFrameReady,  exporter,    &Exporter::enqueueFrame, Qt::QueuedConnection);
    // 初始化模型
    model = new PointDataPageModel(this);
    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // 允许手动调整列宽

    ui->tableView->setColumnWidth(0, 50);
    ui->tableView->setColumnWidth(1, 120);
    ui->tableView->setColumnWidth(2, 50);
    ui->tableView->setColumnWidth(3, 80);
    ui->tableView->setColumnWidth(4, 80);
    ui->tableView->setColumnWidth(5, 80);
    ui->tableView->setColumnWidth(6, 80);
    ui->tableView->setColumnWidth(7, 80);
    // ui->tableView->setColumnWidth(8, 440);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);  // 让最后一列填充剩余空间
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 让表头充满表格
    ui->tableView->verticalHeader()->setVisible(false);  // 隐藏行表头
    ui->tableView->setCornerButtonEnabled(false);       // 彻底隐藏左上角空白


    connect(ui->btnApplyFilters, &QPushButton::clicked, this, [=]() {
        updateFilterSettings();
        enableFilter = true;
        onPointDataUpdated(originalPoints);
    });

    connect(ui->btnClearFilters, &QPushButton::clicked, this, [=]() {
        if (filterManager)
            filterManager->setFilterList({});

        enableFilter = false;

        qDebug() << "Filters cleared.";

        // 恢复显示原始点云
        onPointDataUpdated(originalPoints);
    });

    // 初始化滤波器管理器
    filterManager = new PointCloudFilterManager(this);
    enableFilter = false;


    actionRecord = new QAction("Record", this);
    exportSubMenu = new QMenu("Export", this);
    actionStopExport = new QAction("Stop Export", this);
    QAction* actionCSV = new QAction("CSV", this);
    QAction* actionLAS = new QAction("LAS", this);
    QAction* actionPCD = new QAction("PCD", this);
    QAction* actionPCAP = new QAction("PCAP", this);

    exportSubMenu->addAction(actionCSV);
    exportSubMenu->addAction(actionLAS);
    exportSubMenu->addAction(actionPCD);
    exportSubMenu->addAction(actionPCAP);
    actionStopExport->setVisible(false);

    auto v = new QDoubleValidator(0.00, 10000.00, 2, this); // 最小、最大、小数位数
    v->setNotation(QDoubleValidator::StandardNotation);   // 禁止科学计数法
    // 如需固定用“.”做小数点： v->setLocale(QLocale::c());  // 可选
    ui->lineEdit_x->setValidator(v);
    ui->lineEdit_y->setValidator(v);
    ui->lineEdit_z->setValidator(v);


    QRegularExpression rx(
        R"(^(?:(?:[0-9]|[1-9]\d|[1-2]\d{2}|3[0-5]\d)(?:\.\d{1,2})?|360(?:\.0{1,2})?)$)"
        );
    auto v1 = new QRegularExpressionValidator(rx, this);

    for (QLineEdit* le : {ui->lineEdit_pitch, ui->lineEdit_roll, ui->lineEdit_yaw}) {
        le->setValidator(v1);
        le->setMaxLength(6);                        // 最长如 "360.00"
        le->setInputMethodHints(Qt::ImhFormattedNumbersOnly);
    }
    // lineEdit_stop_x lineEdit_stop_y lineEdit_stop_z lineEdit_range_x lineEdit_range_y lineEdit_range_z
        QRegularExpression rxInt19(R"(^(?:[1-9]\d{0,4})$)"); // 1..99999
    auto vInt19 = new QRegularExpressionValidator(rxInt19, this);

    // 批量设置
    for (QLineEdit* le : { ui->lineEdit_stop_x, ui->lineEdit_stop_y, ui->lineEdit_stop_z, ui->lineEdit_range_x, ui->lineEdit_range_y, ui->lineEdit_range_z }) {
        le->setValidator(vInt19);
        le->setMaxLength(5);              // 最多 5 位
    }
    QRegularExpression rxGrid(R"(^(?:0\.1[0-9]|0\.[2-4][0-9]|0\.50)$)");
    auto vGrid = new QRegularExpressionValidator(rxGrid, this);
    ui->lineEdit_fileter->setValidator(vGrid);
    QRegularExpression rxEps(R"(^(?:0\.(?:[3-9]\d?)|1(?:\.0{1,2})?)$)");
    auto vEpx = new QRegularExpressionValidator(rxEps, this);
    ui->lineEdit_dis->setValidator(vEpx);
    QRegularExpression rxMin(R"(^(?:[2-9]\d|1\d{2}|200)$)");
    ui->lineEdit_min_points->setValidator(new QRegularExpressionValidator(rxMin, this));
    ui->lineEdit_min_points->setMaxLength(3);
    QRegularExpression rxMax(R"(^(?:[1-9]\d{2}|[1-4]\d{3}|5000)$)");
    ui->lineEdit_max_points->setValidator(new QRegularExpressionValidator(rxMax, this));
    ui->lineEdit_max_points->setMaxLength(4);


    connect(actionCSV,  &QAction::triggered, this, [=]() { openExprotFrameDlg(CSV); });
    connect(actionLAS,  &QAction::triggered, this, [=]() { openExprotFrameDlg(LAS); });
    connect(actionPCD,  &QAction::triggered, this, [=]() { openExprotFrameDlg(PCD); });
    connect(actionPCAP, &QAction::triggered, this, [=]() { openExprotFrameDlg(PCAP); });
    connect(actionStopExport, &QAction::triggered, this, &LaserRadar::onStopExport);
    connect(ui->checkBox_zhitong, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_tisu, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_tongji, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_banjing, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_gaosi, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_shuangbian, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
    connect(ui->checkBox_pinlv, &QCheckBox::clicked, this, &LaserRadar::updateLBQpage);
}

void LaserRadar::initTittleBar()
{
    QLayout *layout = ui->wgt_top->layout();
    layout->setSpacing(5);
    minButton = new QToolButton(ui->wgt_top);
    maxButton = new QToolButton(ui->wgt_top);
    closeButton = new QToolButton(ui->wgt_top);

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
    QString minButtonStyle =(
        "QToolButton { background: transparent; border: none; color: white; padding-bottom: 10px; }"
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
        } else {
            showMaximized();
            maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        }
    });
    connect(closeButton, &QToolButton::clicked, this, &QMainWindow::close);
    layout->addWidget(minButton);
    layout->addWidget(maxButton);
    layout->addWidget(closeButton);
    // ui->wgt_top->layout()->addItem(layout);
}

// 🟢 计算鼠标位置，判断是否进入调整大小模式
void LaserRadar::checkResizeDirection(const QPoint &cursorPos)
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

// 🟢 更新鼠标光标形状（调整窗口大小时）
void LaserRadar::updateCursorShape(const QPoint &cursorPos)
{
    if (isMaximized()) {
        setCursor(Qt::ArrowCursor);  // 🔹 窗口最大化时强制鼠标箭头
        return;
    }

    checkResizeDirection(cursorPos);

    if (resizeDirection == (Qt::LeftEdge | Qt::TopEdge) || resizeDirection == (Qt::RightEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (resizeDirection == (Qt::RightEdge | Qt::TopEdge) || resizeDirection == (Qt::LeftEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeBDiagCursor);
    } else if (resizeDirection & (Qt::LeftEdge | Qt::RightEdge)) {
        setCursor(Qt::SizeHorCursor);
    } else if (resizeDirection & (Qt::TopEdge | Qt::BottomEdge)) {
        setCursor(Qt::SizeVerCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

// 🟢 调整窗口大小
void LaserRadar::resizeWindow(const QPoint &delta)
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

void LaserRadar::enableMouseTrackingForAll(QWidget *parent)
{
    if (!parent) return;
    if (parent != ui->pointCloud)
    parent->setMouseTracking(true);

    // 遍历所有子控件
    foreach (QObject *obj, parent->children()) {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget) {
            enableMouseTrackingForAll(widget);  // 递归调用
        }
    }
}

void LaserRadar::on_checkBox_toggled(bool checked)
{
    if (checked)
    {
        if (!initColorBar) {
            colorBar->setParent(this); // 设置父窗口
            colorBar->move(50, 50);    // 初始位置
            colorBar->resize(150, 300);
            colorBar->show();
            colorBar->raise();
            initColorBar = true;
        } else {
            colorBar->show();
        }
    }
    else
    {
        colorBar->hide();
    }
}


void LaserRadar::on_comboBox_currentIndexChanged(int index)
{
    ui->stackedWidget_dylb->setCurrentIndex(index);
}

void LaserRadar::initWidgets(QWidget *wgt, int w, int h)
{
    wgt->raise();
    wgt->setParent(this);
    wgt->setGeometry(0,30,w,h);
    wgt->move(-wgt->width()-5, 30);
    wgtShowMap.insert(wgt, false);
}

void LaserRadar::wgtCloseBtnClicked()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    QWidget *wgt = btn->parentWidget();
    QPropertyAnimation *animation = new QPropertyAnimation(wgt, "geometry");
    animation->setDuration(300);  // 动画持续时间（毫秒）
    animation->setStartValue(wgt->geometry());  // 当前窗口位置
    animation->setEndValue(QRect(-wgt->width()-5, 30, wgt->width(), wgt->height()));  // 目标位置
    animation->setEasingCurve(QEasingCurve::OutQuad);  // 使动画更平滑
    wgtShowMap[wgt] = false;

    connect(animation, &QPropertyAnimation::finished,  [=]() {
        animation->deleteLater();
    });

    animation->start();

    foreach (QPushButton *btn, buttons) {
        buttonStates[btn] = false;
        setButtonStyle(btn, false);
    }


}

void LaserRadar::setupButtons()
{
    // 这里假设 ui->xxx 是 Qt 设计师里定义的按钮
    buttons = { ui->btn_basicSetting, ui->btn_start, ui->btn_colorSetting,
               ui->btn_radarBD, ui->btn_obs, ui->btn_dataShow, ui->btn_dylb, ui->btn_import, ui->btn_export };

    // 初始化状态
    foreach (QPushButton *btn , buttons) {
        buttonStates[btn] = false;  // 初始状态：未选中
        setButtonStyle(btn, false); // 默认透明背景

        // 绑定点击事件
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            toggleButton(btn);
        });
    }
}

void LaserRadar::toggleButton(QPushButton *clickedBtn)
{
    bool isSelected = buttonStates[clickedBtn];

    // 取消所有按钮的选中状态
    foreach (QPushButton *btn, buttons) {
        buttonStates[btn] = false;
        setButtonStyle(btn, false);
    }

    // 如果当前按钮原本是未选中，则选中它
    if (!isSelected) {
        buttonStates[clickedBtn] = true;
        setButtonStyle(clickedBtn, true);
    }
}

void LaserRadar::setButtonStyle(QPushButton *btn, bool isSelected) {
    if (isSelected) {
        btn->setStyleSheet(
            "QPushButton { border-image: url(:/images/Top_checked.png); }"
            "QPushButton:hover { border-image: url(:/images/Top_checked.png); }"
            );
    } else {
        btn->setStyleSheet(
            "QPushButton { border: none; background: transparent; }"
            "QPushButton:hover { border-image: url(:/images/button_default.png); }"
            );
    }
}

void LaserRadar::showWgtAnimation(QWidget *wgt)
{
    QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
    // 存储动画指针，方便后续清理
    QList<QPropertyAnimation *> animations;
    QWidget *currentWgt = nullptr;
    for (auto it = wgtShowMap.begin(); it != wgtShowMap.end(); ++it) {
        if (it.value()) {
            currentWgt = it.key();
        }
    }

    // 旧界面滑出
    if (currentWgt) {
        QPropertyAnimation *animation = new QPropertyAnimation(currentWgt, "geometry");
        animation->setDuration(300);  // 动画持续时间（毫秒）
        animation->setStartValue(currentWgt->geometry());  // 当前窗口位置
        animation->setEndValue(QRect(-currentWgt->width()-5, 30, currentWgt->width(), currentWgt->height()));  // 目标位置
        animation->setEasingCurve(QEasingCurve::OutQuad);  // 使动画更平滑
        group->addAnimation(animation);
        animations.append(animation);  // 记录动画指针
        wgtShowMap[currentWgt] = false;
    }

    // 新界面滑入
    if (currentWgt != wgt) {
        QPropertyAnimation *animIn = new QPropertyAnimation(wgt, "geometry");
        animIn->setDuration(500);
        animIn->setStartValue(wgt->geometry());
        animIn->setEndValue(QRect(5, 30, wgt->width(), wgt->height()));
        group->addAnimation(animIn);
        animations.append(animIn);  // 记录动画指针

        wgtShowMap[wgt] = true;
    }

    connect(group, &QParallelAnimationGroup::finished, [=]() {
        // 清理动画对象，避免内存泄漏
        for (auto *anim : animations) {
            anim->deleteLater();  // 安全删除动画对象
        }
        group->deleteLater();  // 删除动画组
    });

    group->start();

}

void LaserRadar::updateFilterSettings()
{
    if (!filterManager)
        return;

    QList<FilterSettings> filters;

    if (ui->checkBox_zhitong->isChecked()) {
        FilterSettings setting;
        setting.name = "Pass-Through Filter";
        setting.passX = ui->checkBox_passX->isChecked();
        setting.passY = ui->checkBox_passY->isChecked();
        setting.passZ = ui->checkBox_passZ->isChecked();

        setting.passXMin = ui->spinPassXMin->value();
        setting.passXMax = ui->spinPassXMax->value();
        setting.passYMin = ui->spinPassYMin->value();
        setting.passYMax = ui->spinPassYMax->value();
        setting.passZMin = ui->spinPassZMin->value();
        setting.passZMax = ui->spinPassZMax->value();
        filters.append(setting);
    }
    if (ui->checkBox_tisu->isChecked()) {
        FilterSettings setting;
        setting.name = "Voxel Filter";
        setting.voxelSize = ui->spinVoxelSize->value();
        filters.append(setting);
    }
    if (ui->checkBox_tongji->isChecked()) {
        FilterSettings setting;
        setting.name = "Statistical Filter";
        setting.statMeanK = ui->spinStatNeighbors->value();
        setting.statStddevMulThresh = ui->spinStatStddev->value();
        filters.append(setting);
    }
    if (ui->checkBox_banjing->isChecked()) {
        FilterSettings setting;
        setting.name = "Radius Filter";
        setting.radius = ui->spinRadius->value();
        setting.radiusMinNeighbors = ui->spinRadiusMinNeighbors->value();
        filters.append(setting);
    }
    if (ui->checkBox_gaosi->isChecked()) {
        FilterSettings setting;
        setting.name = "Gaussian Filter";
        setting.gaussianSigmaS = ui->spinGaussianSigmaS->value();
        filters.append(setting);
    }
    if (ui->checkBox_shuangbian->isChecked()) {
        FilterSettings setting;
        setting.name = "Bilateral Filter";
        setting.bilateralSigmaS = ui->spinBilateralSigmaS->value();
        setting.bilateralSigmaR = ui->spinBilateralSigmaR->value();
        filters.append(setting);
    }
    if (ui->checkBox_pinlv->isChecked()) {
        FilterSettings setting;
        setting.name = "Frequency Filter";
        setting.freqWindow = ui->spinFreqWindow->value();
        setting.freqThreshold = ui->spinFreqThreshold->value();
        filters.append(setting);
    }

    filterManager->setFilterList(filters);

    qDebug() << "Filter list updated, count=" << filters.count();
}

void LaserRadar::updateExportSatatus(bool exporting)
{
    actionStopExport->setVisible(exporting);
    exportSubMenu->menuAction()->setVisible(!exporting);
}

void LaserRadar::on_btn_basicSetting_clicked()
{
    showWgtAnimation(ui->wgt_basicSetting);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}

void LaserRadar::on_btn_start_clicked()
{
    bool s = ui->btn_start->isChecked();
    if (s) {
        quint16 dataPort = ui->lineEdit_msopPort->text().toUInt();
        quint16 devInfoPort = ui->lineEdit_difopPort->text().toUInt();
        udp->start(dataPort, devInfoPort);
        ui->btn_start->setText("Off");
    } else {
        udp->closeConnection();
        ui->btn_start->setText("On");
        ui->pointCloud->clearPointCloud();
    }
    ui->btn_start->setChecked(ui->btn_start->isChecked());
}


void LaserRadar::on_btn_colorSetting_clicked()
{
    showWgtAnimation(ui->wgt_colorSetting);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void LaserRadar::on_btn_radarBD_clicked()
{
    showWgtAnimation(ui->wgt_radarBD);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void LaserRadar::on_btn_obs_clicked()
{
    showWgtAnimation(ui->wgt_obs);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void LaserRadar::on_btn_dataShow_clicked()
{
    showWgtAnimation(ui->wgt_dataShow);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void LaserRadar::on_btn_dylb_clicked()
{
    showWgtAnimation(ui->wgt_dylb);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void LaserRadar::on_btn_import_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "SelectPCAPFile", "", "PCAP Files (*.pcap)");
    if (filePath.isEmpty())
        return;

    auto *replayWin = new ReplayWindow(); // 建议不要传 this，否则关闭会影响主窗口
    replayWin->setAttribute(Qt::WA_DeleteOnClose);
    replayWin->resize(960, 600);

    if (!replayWin->loadPCAP(filePath)) {
        QMessageBox::warning(this, "Error", "Unable to load the PCAP file. The format may be invalid or contain no valid data!");
        replayWin->deleteLater();  // 更安全释放方式
        return;
    }

    replayWin->show();
}


void LaserRadar::on_btn_export_clicked()
{
    QMenu* mainMenu = new QMenu(this);

    mainMenu->addAction(actionRecord);
    mainMenu->addMenu(exportSubMenu);
    mainMenu->addAction(actionStopExport);

    // 弹出菜单在按钮下方
    QPoint pos = ui->btn_export->mapToGlobal(QPoint(0, ui->btn_export->height()));
    mainMenu->exec(pos);
}

void LaserRadar::updatePointData(QList<PointData> pointDataList)
{
    // model->addPage(pointDataList);
    model->addData(pointDataList);

    ui->spinPage->setRange(1, qMax(1, model->pageCount()));  // 保证合法
    ui->spinPage->setValue(model->currentPageIndex() + 1);
    ui->labelPage->setText(QString("/ %1").arg(model->pageCount()));
}

void LaserRadar::onPointDataUpdated(const QList<PointXYZI> &points)
{
    QList<PointXYZI> displayPoints = points;
    if (enableFilter && filterManager)
        displayPoints = filterManager->applyFilters(points);

    ui->pointCloud->updatePointCloud(displayPoints);

    if (!enableObstacleDetection) {
        return;
    }
}


void LaserRadar::on_pushButton_clicked()
{

}


void LaserRadar::on_spinPassXMax_valueChanged(double arg1)
{
    double min = ui->spinPassXMin->value();
    if (arg1 > min)
    {
        ui->spinPassXMax->setValue(arg1);
    } else {
        ui->spinPassXMax->setValue(min + 0.1);
    }
}


void LaserRadar::on_spinPassXMin_valueChanged(double arg1)
{
    double max = ui->spinPassXMax->value();
    if (arg1 < max)
    {
        ui->spinPassXMin->setValue(arg1);
    } else {
        ui->spinPassXMin->setValue(max - 0.1);
    }
}


void LaserRadar::on_spinPassYMax_valueChanged(double arg1)
{
    double min = ui->spinPassYMin->value();
    if (arg1 > min)
    {
        ui->spinPassYMax->setValue(arg1);
    } else {
        ui->spinPassYMax->setValue(min + 0.1);
    }
}


void LaserRadar::on_spinPassYMin_valueChanged(double arg1)
{
    double max = ui->spinPassYMax->value();
    if (arg1 < max)
    {
        ui->spinPassYMin->setValue(arg1);
    } else {
        ui->spinPassYMin->setValue(max - 0.1);
    }
}


void LaserRadar::on_spinPassZMax_valueChanged(double arg1)
{
    double min = ui->spinPassZMin->value();
    if (arg1 > min)
    {
        ui->spinPassZMax->setValue(arg1);
    } else {
        ui->spinPassZMax->setValue(min + 0.1);
    }
}


void LaserRadar::on_spinPassZMin_valueChanged(double arg1)
{
    double max = ui->spinPassZMax->value();
    if (arg1 < max)
    {
        ui->spinPassZMin->setValue(arg1);
    } else {
        ui->spinPassZMin->setValue(max - 0.1);
    }
}

void LaserRadar::openExprotFrameDlg(ExportType type)
{
    ExportFrameDialog dlg;
    connect(&dlg, &ExportFrameDialog::signalExportParamsReady,
                      this, &LaserRadar::onExportParamsReceived);
    dlg.SetExportType(type);
    dlg.exec();
}

void LaserRadar::onExportParamsReceived(const ExportParams &params)
{
    emit signalExportToUDP(params);  // 转发给 UDP
    updateExportSatatus(true);
}

void LaserRadar::onExportFinished()
{
    updateExportSatatus(false);
}

void LaserRadar::onStopExport()
{
    udp->stopExport();
    updateExportSatatus(false);
}


// void LaserRadar::on_checkBox_enable_toggled(bool checked)
// {
//     ui->checkBox_disable->setChecked(!checked);
// }


// void LaserRadar::on_checkBox_disable_toggled(bool checked)
// {
//     ui->checkBox_enable->setChecked(!checked);
// }


void LaserRadar::on_checkBox_enable_clicked()
{
    ui->pointCloud->drawObb = true;
    ui->checkBox_enable->setChecked(true);
    ui->checkBox_disable->setChecked(false);
}


void LaserRadar::on_checkBox_disable_clicked()
{
    ui->pointCloud->drawObb = false;
    ui->checkBox_disable->setChecked(true);
    ui->checkBox_enable->setChecked(false);
}


void LaserRadar::on_label_link_linkActivated(const QString &link)
{
    SvgDialog dlg(":/images/laser.svg");
    dlg.exec();
}


void LaserRadar::on_btn_connectDev_clicked()
{
    QMessageBox::information(this, "Basic Settings", "Settings saved.");
}


void LaserRadar::on_btn_connectDev_2_clicked()
{
    QMessageBox::information(this, "Radar Calibration", "Calibration succeeded.");
}


void LaserRadar::on_label_2_linkActivated(const QString &link)
{
    QString address = ui->lineEdit_deviceIP->text();  // 可替换成用户输入、配置文件等
    int port = 80; // 默认 HTTP 端口

    QTcpSocket socket;
    socket.setProxy(QNetworkProxy::NoProxy);
    socket.connectToHost(address, port);

    if (!socket.waitForConnected(1000)) {  // 最多等待 1 秒
    qDebug() << "Connection failed:" << socket.errorString();
        QMessageBox::warning(this, "Connection Failed", "Unable to connect to " + address + ",\nCheck the network or device power!");
        return;
    }

    QUrl url("http://" + address);
    BrowserDialog *dlg = new BrowserDialog(url, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void LaserRadar::updateLBQpage()
{
    QCheckBox *btn = qobject_cast<QCheckBox *>(sender());

    if (btn == ui->checkBox_zhitong) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pagePassThrough);
    } else if (btn == ui->checkBox_tisu) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageVoxelGrid);
    } else if (btn == ui->checkBox_tongji) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageStatistics);
    } else if (btn == ui->checkBox_banjing) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageRadius);
    } else if (btn == ui->checkBox_gaosi) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageGaussian);
    } else if (btn == ui->checkBox_shuangbian) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageBilateral);
    } else if (btn == ui->checkBox_pinlv) {
        ui->stackedWidgetParams->setCurrentWidget(ui->pageFrequency);
    }
}

void LaserRadar::updateObsParams()
{
    OBBParams params;
    params.voxel  = ui->lineEdit_fileter->text().toFloat();
    params.eps    = ui->lineEdit_dis->text().toFloat();
    params.minPts = ui->lineEdit_min_points->text().toInt();
    params.maxPts = ui->lineEdit_max_points->text().toInt();
    ui->pointCloud->setOBBParams(params);
}


