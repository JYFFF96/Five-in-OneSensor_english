#include "mmwaveradar.h"
#include "ui_mmwaveradar.h"
extern bool  IsOpenFlag;

constexpr double BIAODING_WUCHA_DISTANCE = 0.2;
constexpr double BIAODING_WUCHA_ANGLE = 2.0;
mmWaveRadar::mmWaveRadar(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::mmWaveRadar)
{
    ui->setupUi(this);
    QIcon icon(":/images/logo.png");
    this->setWindowIcon(icon);
    this->setWindowTitle("Millimeter-Wave Radar Debugger");
    this->setWindowFlags(Qt::FramelessWindowHint); // 隐藏标题栏
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->centralWidget()->setLayout(ui->verticalLayout_6);
    initTittleBar();
    initWidgets(ui->wgt_dataShow, 800, this->height()-30);
    initWidgets(ui->wgt_connet, 200, 220);
    initWidgets(ui->wgt_scale, 180, 160);
    initWidgets(ui->wgt_radarBD, 220, 300);
    initWidgets(ui->wgt_dataSave, 320, 180);
    initWidgets(ui->wgt_obstacleSet, 380, 160);
    initWidgets(ui->wgt_paramSet, 300, 330);
    initWidgets(ui->wgt_targetSet, 240, 255);
    initTableview();
    initUI();
    enableMouseTrackingForAll(this);
    qRegisterMetaType<ObstacleInfo>("ObstacleInfo");

    canThread = new CANThread;
    connect(canThread, &CANThread::send1data, this, &mmWaveRadar::receiveData);
    connect(canThread, &CANThread::sendSignal, this, &mmWaveRadar::receiveSignal);

    sendTimer = new QTimer(this);
    connect(sendTimer, &QTimer::timeout, this, &mmWaveRadar::sendData2Dev);

    ui->btn_paramSet->hide();
}

mmWaveRadar::~mmWaveRadar()
{
    delete ui;
}


void mmWaveRadar::initUI()
{
    QDoubleValidator *validator = new QDoubleValidator(0.0, 999.0, 2, this);
    validator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineEdit_xAxisRange->setValidator(validator);
    ui->lineEdit_yAxisRange->setValidator(validator);
    QDoubleValidator *angleRangeValidator = new QDoubleValidator(0, 180, 2, this);
    angleRangeValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineEdit_angleRange->setValidator(angleRangeValidator);

    QIntValidator *validator1 = new QIntValidator(1, INT_MAX, this); // 只允许输入 1 到最大正整数
    ui->lineEdit_sendCycle->setValidator(validator1);
    ui->lineEdit_sendTimes->setValidator(validator1);
    setupHexInput(ui->lineEdit_frameID, 8);
    dataFilter = setupHexInput(ui->lineEdit_data, 8);

    QIntValidator *validator2 = new QIntValidator(1, 9999999, this);
    ui->lineEdit_saveLine->setValidator(validator2);

    ui->checkBox_1mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_3mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_5mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_453mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_455mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_fu453mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_fu455mbd->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    connect(ui->customPlot, &myCustomPlot::sendRange, this, &mmWaveRadar::updatePlotRange);
    connect(ui->btn_closeScale, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeRadarBD, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeConnect, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeDataSave, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);

    connect(ui->btn_closeTargetSet, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeParamSet, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeObstacleSet, &QPushButton::clicked, this, &mmWaveRadar::wgtCloseBtnClicked);

    // 初始化 targetCheckboxes（不包含 checkBox_allTarget）
    targetCheckboxes = {
        ui->checkBox_movingTarget,
        ui->checkBox_stationaryTarget,
        ui->checkBox_oncomingTarget,
        ui->checkBox_crossingLeftTarget,
        ui->checkBox_crossingRightTarget,
        ui->checkBox_stoppedTarget,
        ui->checkBox_unknownTarget
    };
    // 连接信号与槽
    connect(ui->checkBox_allTarget, &QCheckBox::stateChanged,
            this, &mmWaveRadar::onAllTargetStateChanged);

    for (QCheckBox* cb : targetCheckboxes) {
        connect(cb, &QCheckBox::stateChanged,
                this, &mmWaveRadar::onIndividualTargetStateChanged);
    }
    connect(ui->checkBox_allType, &QCheckBox::stateChanged, this, &mmWaveRadar::onAllTypeCheckBoxChanged);
    connect(ui->checkBox_carType, &QCheckBox::stateChanged, this, &mmWaveRadar::onIndividualTypeCheckBoxChanged);
    connect(ui->checkBox_pointType, &QCheckBox::stateChanged, this, &mmWaveRadar::onIndividualTypeCheckBoxChanged);
    ui->lineEdit_savaPath->setText(QCoreApplication::applicationDirPath() + "/data");
    connect(ui->checkBox_bin, &QCheckBox::toggled, model, &CanModel::setSaveBinary);
    connect(ui->checkBox_txt, &QCheckBox::toggled, model, &CanModel::setSaveTxt);
    connect(ui->checkBox_asc, &QCheckBox::toggled, model, &CanModel::setSaveAsc);
    connect(ui->checkBox_blf, &QCheckBox::toggled, model, &CanModel::setSaveBlf);
    connect(ui->checkBox_excel, &QCheckBox::toggled, model, &CanModel::setSaveExcel);
}

void mmWaveRadar::initWidgets(QWidget *wgt, int w, int h)
{
    wgt->setParent(this);
    wgt->setGeometry(0,30,w,h);
    wgt->move(-wgt->width()-5, 30);
    wgtShowMap.insert(wgt, false);
}

void mmWaveRadar::setupButtons()
{
    // 这里假设 ui->xxx 是 Qt 设计师里定义的按钮
    buttons = { ui->btn_connet, ui->btn_scale, ui->btn_radarBD,
               ui->btn_commProtocol, ui->btn_dataSave, ui->btn_obstacleSet, ui->btn_paramSet, ui->btn_targetSet};

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

void mmWaveRadar::setButtonStyle(QPushButton *btn, bool isSelected) {
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

void mmWaveRadar::toggleButton(QPushButton *clickedBtn)
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

    QWidget *showWgt;
    if (clickedBtn == ui->btn_connet) {
        showWgt = ui->wgt_connet;
    } else if (clickedBtn == ui->btn_scale) {
        showWgt = ui->wgt_scale;
    } else if (clickedBtn == ui->btn_radarBD) {
        showWgt = ui->wgt_radarBD;
    } else if (clickedBtn == ui->btn_commProtocol) {
        showWgt = ui->wgt_dataShow;
    } else if (clickedBtn == ui->btn_dataSave) {
        showWgt = ui->wgt_dataSave;
    } else if (clickedBtn == ui->btn_obstacleSet) {
        showWgt = ui->wgt_obstacleSet;
    } else if (clickedBtn == ui->btn_paramSet) {
        showWgt = ui->wgt_paramSet;
    } else if (clickedBtn == ui->btn_targetSet) {
        showWgt = ui->wgt_targetSet;
    }
    showWgtAnimation(showWgt);
}

void mmWaveRadar::enableMouseTrackingForAll(QWidget *parent)
{
    if (!parent) return;

    parent->setMouseTracking(true);

    // 遍历所有子控件
    foreach (QObject *obj, parent->children()) {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget) {
            enableMouseTrackingForAll(widget);  // 递归调用
        }
    }
}

void mmWaveRadar::resizeEvent(QResizeEvent *event)
{
    ui->wgt_dataShow->setGeometry(ui->wgt_dataShow->x(),30,800,this->height()-30);
    if (ui->wgt_dataShow->x() > 0) {
        // 设置变化
        ui->wgt_dataShow->setGeometry(5,30,800,this->height()-30);
    }
}

void mmWaveRadar::mousePressEvent(QMouseEvent *event)
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

void mmWaveRadar::mouseMoveEvent(QMouseEvent *event)
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

void mmWaveRadar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
    isDragging = false;
    isResizing = false;
    resizeDirection = Qt::Edges();
}

void mmWaveRadar::mouseDoubleClickEvent(QMouseEvent *event)
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

// 🟢 计算鼠标位置，判断是否进入调整大小模式
void mmWaveRadar::checkResizeDirection(const QPoint &cursorPos)
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
void mmWaveRadar::updateCursorShape(const QPoint &cursorPos)
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
void mmWaveRadar::resizeWindow(const QPoint &delta)
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

void mmWaveRadar::initTittleBar()
{
    setupButtons();
    QHBoxLayout *layout = new QHBoxLayout(ui->wgt_top);
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
        } else {
            showMaximized();
            maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
        }
    });
    connect(closeButton, &QToolButton::clicked, this, &QMainWindow::close);
    layout->addWidget(minButton);
    layout->addWidget(maxButton);
    layout->addWidget(closeButton);
    ui->wgt_top->layout()->addItem(layout);
}

void mmWaveRadar::showWgtAnimation(QWidget *wgt)
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

void mmWaveRadar::initTableview()
{
    // 初始化模型
    model = new CanModel(this);
    connect(model, &CanModel::newDataAdded, this, &mmWaveRadar::scrollToBottom);
    connect(model, &CanModel::saveFinished, this, &mmWaveRadar::saveFileFinished);
    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // 允许手动调整列宽

    ui->tableView->setColumnWidth(0, 50);
    ui->tableView->setColumnWidth(1, 60);
    ui->tableView->setColumnWidth(2, 60);
    ui->tableView->setColumnWidth(3, 60);
    ui->tableView->setColumnWidth(4, 60);
    ui->tableView->setColumnWidth(5, 155);
    ui->tableView->setColumnWidth(6, 620);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);  // 让最后一列填充剩余空间
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 让表头充满表格
    ui->tableView->verticalHeader()->setVisible(false);  // 隐藏行表头
    ui->tableView->setCornerButtonEnabled(false);       // 彻底隐藏左上角空白
    auto* view  = ui->tableView;
    // QObject::connect(model, &QAbstractItemModel::rowsInserted, view, [view](const QModelIndex&, int, int){
    //     auto* vs = view->verticalScrollBar();
    //     bool wasAtBottom = (vs->value() == vs->maximum()); // 追加前是否在底
    //     if (wasAtBottom) {
    //         // 让它继续“贴底”
    //         QTimer::singleShot(0, view, [view]{ view->scrollToBottom(); });
    //     }
    // });
}

void mmWaveRadar::parseData(VCI_CAN_OBJ data)
{
    if (data.ID == 0x060a) {
        obsCount = data.Data[0];
        obsList.clear();
    } else if (data.ID == 0x060b) {
        ObstacleInfo obj;

        // 目标 ID
        obj.id = data.Data[0];

        // 目标纵向距离‘
        obj.distLong = ((data.Data[1] * 32 + (data.Data[2] >> 3)) * 0.2) - 500;

        // 目标横向距离
        obj.distLat = (((data.Data[2] & 0x07) * 256 + data.Data[3]) * 0.2) - 204.6;

        // 目标纵向速度
        obj.vrelLong = ((data.Data[4] * 4 + (data.Data[5] >> 6)) * 0.25) - 128;

        // 目标横向速度
        obj.vrelLat = (((data.Data[5] & 0x3F) * 8 + (data.Data[6] >> 5)) * 0.25) - 64;

        // 目标动态属性
        obj.dynProp = data.Data[6] & 0x07;
        obj.objClass = (data.Data[6] >> 3) & 0x03;  // (0x18 >> 3)
        // RCS
        obj.rcs = (data.Data[7] * 0.5) - 64;

        // 距离 角度 速度
        obj.distance = sqrt(obj.distLong* obj.distLong+ obj.distLat*obj.distLat);
        double angle_rad = atan2(obj.distLat, obj.distLong);
        obj.angle = angle_rad * 180.0 / M_PI;
        // double v =
        obj.speed = obj.vrelLong * std::cos(angle_rad) + obj.vrelLat * std::sin(angle_rad);
        // qDebug() << "Frame ID:" << data.ID;
        // qDebug() << "ID:" << obj.id
        //          << " distLong:" << obj.distLong
        //          << " distLat:" << obj.distLat
        //          << " angle:" << obj.angle
        //          << " speed:" << obj.speed;
        obsList.append(obj);
        // qDebug() << "=== After append ===";
        // for (int i = 0; i < obsList.size(); ++i) {
        //     const auto& o = obsList[i];
        //     qDebug() << "Index:" << i
        //              << "id:" << o.id
        //              << "distLong:" << o.distLong
        //              << "distLat:" << o.distLat
        //              << "angle:" << o.angle
        //              << "speed:" << o.speed;
        // }

        // 是否标定中
        if (isBding) {
            if (obj.distance >= currentBDDistance - BIAODING_WUCHA_DISTANCE && obj.distance <= currentBDDistance + BIAODING_WUCHA_DISTANCE) {
                if (obj.angle >= currentBDAngle - BIAODING_WUCHA_ANGLE && obj.angle <= currentBDAngle + BIAODING_WUCHA_ANGLE) {
                    ui->customPlot->setBiaodingColor(Qt::green);
                    this->currentBDCheckBox->setChecked(true);
                }
            }
        }

        if (obsList.count() == obsCount) {
            // 绘制到界面中
            ui->customPlot->drawObstaclesWithText(obsList);
        }
    }
}

void mmWaveRadar::on_btn_setRange_clicked()
{
    double x = ui->lineEdit_xAxisRange->text().toDouble();
    double y = ui->lineEdit_yAxisRange->text().toDouble();
    double angle = ui->lineEdit_angleRange->text().toInt();
    ui->customPlot->updatePlot(x, y, angle);
}

void mmWaveRadar::updatePlotRange(double xRange, double yRange)
{
    ui->lineEdit_xAxisRange->setText(QString::number(xRange, 'f', 2));
    ui->lineEdit_yAxisRange->setText(QString::number(yRange, 'f', 2));
}

void mmWaveRadar::wgtCloseBtnClicked()
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

    if (btn == ui->btn_closeRadarBD) {
        ui->customPlot->stopBiaoding();
    }

}

void mmWaveRadar::receiveData(VCI_CAN_OBJ data)
{
    if (isPaused) {
        // 暂停状态，把数据存入缓存
        bufferedData.append(qMakePair(CanMsgType::Receive, data));
    } else {
        // 直接更新表格
        model->appendData(CanMsgType::Receive, data);
    }
    // 解析 在界面显示
    parseData(data);
}



void mmWaveRadar::on_btn_connectDev_clicked()
{
    if (mconnect==false)
    {
        int devType = ui->comboBox_devType->currentIndex()+3;
        int devIndex = ui->comboBox_devIndex->currentIndex();
        int canIndex = ui->comboBox_canIndex->currentIndex();
        QString str = ui->comboBox_baud->currentText();
        str.remove(QRegularExpression("[^0-9]"));  // 删除所有非数字字符
        int mbaud =str.toUInt();
        int mmode =ui->comboBox_mode->currentIndex();
        bool bb = canThread->openDevice(devType, devIndex, mbaud, canIndex, mmode);



        if(!bb)//启动设备失败
        {
            QMessageBox msgBox;
            msgBox.setText("Failed to open device!");
        }
        else//启动设备成功
        {
            canThread->start();//启动子线程//间接调用了run()函数//即接收数据
            ui->customPlot->startPulse();
            ui->btn_connectDev->setText("Disconnect");//
            mconnect=true;
        }
    }
    else
    {
        ui->customPlot->stopPulse();
        canThread->closeDevice();
        canThread->stop();//停止子线程
        ui->btn_connectDev->setText("Connect");//
        mconnect=false;
        ui->customPlot->clearObstacles();
    }
}

void mmWaveRadar::scrollToBottom()
{
    QScrollBar *scrollBar = ui->tableView->verticalScrollBar();

    // 只有当滚动条在底部时才滚动
    if (scrollBar->value() == scrollBar->maximum()) {
        ui->tableView->scrollToBottom();
    }
}

void mmWaveRadar::saveFileFinished()
{

}

void mmWaveRadar::on_btn_pause_clicked()
{
    if (isPaused) {
        model->appendBufferedData(bufferedData);
        bufferedData.clear();  // 清空缓存
        // 重新启用刷新
        ui->btn_pause->setText("Stop Refresh");
    } else {
        // 暂停刷新
        ui->btn_pause->setText("Start Refresh");
    }

    isPaused = !isPaused;  // 切换状态
}

void mmWaveRadar::receiveObstacleInfoList(QList<ObstacleInfo> obsList)
{
    ui->customPlot->drawObstaclesWithText(obsList);
}

void mmWaveRadar::receiveSignal(bool flag, VCI_CAN_OBJ data)
{
    if (isPaused) {
        // 暂停状态，把数据存入缓存
        if (flag) {
            bufferedData.append(qMakePair(CanMsgType::SendSuccess, data));
        } else {
            bufferedData.append(qMakePair(CanMsgType::SendFail, data));
        }
    } else {
        // 直接更新表格
        if (flag) {
            model->appendData(CanMsgType::SendSuccess, data);
        } else {
            model->appendData(CanMsgType::SendFail, data);
        }
    }
}


void mmWaveRadar::on_btn_clearTable_clicked()
{
    model->clearData();
}


void mmWaveRadar::on_checkBox_parseData_toggled(bool checked)
{
    model->updateParsing(checked); // 触发数据更新
}


void mmWaveRadar::on_btn_obstacleSetSure_clicked()
{
    LabelDisplayOptions options;
    options.showAngle = ui->checkBox_targetAngle->isChecked();
    options.showRange = ui->checkBox_targetRange->isChecked();
    options.showSpeed = ui->checkBox_targetV->isChecked();
    options.showRcs = ui->checkBox_targetRCS->isChecked();
    options.showState = ui->checkBox_targetState->isChecked();
    options.showClass = ui->checkBox_targetClass->isChecked();
    ui->customPlot->setLabelDisplayOptions(options);
}

void mmWaveRadar::onAllTargetStateChanged(int state)
{
    if (updatingCheckStates) return;
    updatingCheckStates = true;

    bool checked = (state == Qt::Checked);

    showTargetState targetState;
    // 修改结构体
    targetState.showAllTarget = checked;
    targetState.showMovingTarget = checked;
    targetState.showStationaryTarget = checked;
    targetState.showOncomingTarget = checked;
    targetState.showCrossingLeftTarget = checked;
    targetState.showCrossingRightTarget = checked;
    targetState.showStoppedTarget = checked;
    targetState.showUnknownTarget = checked;
    ui->customPlot->updateTargetState(targetState);

    for (QCheckBox* cb : targetCheckboxes) {
        cb->setChecked(checked);
    }

    updatingCheckStates = false;
}

void mmWaveRadar::onIndividualTargetStateChanged(int)
{
    if (updatingCheckStates) return;
    updatingCheckStates = true;

    showTargetState targetState;
    // 同步结构体状态
    targetState.showMovingTarget = ui->checkBox_movingTarget->isChecked();
    targetState.showStationaryTarget = ui->checkBox_stationaryTarget->isChecked();
    targetState.showOncomingTarget = ui->checkBox_oncomingTarget->isChecked();
    targetState.showCrossingLeftTarget = ui->checkBox_crossingLeftTarget->isChecked();
    targetState.showCrossingRightTarget = ui->checkBox_crossingRightTarget->isChecked();
    targetState.showStoppedTarget = ui->checkBox_stoppedTarget->isChecked();
    targetState.showUnknownTarget = ui->checkBox_unknownTarget->isChecked();

    ui->customPlot->updateTargetState(targetState);

    // 判断是否全选，更新全选框和结构体
    bool allChecked = targetState.showMovingTarget &&
                      targetState.showStationaryTarget &&
                      targetState.showOncomingTarget &&
                      targetState.showCrossingLeftTarget &&
                      targetState.showCrossingRightTarget &&
                      targetState.showStoppedTarget &&
                      targetState.showUnknownTarget;

    targetState.showAllTarget = allChecked;
    ui->checkBox_allTarget->setChecked(allChecked);

    updatingCheckStates = false;
}

void mmWaveRadar::onAllTypeCheckBoxChanged(int state)
{
    if (updatingCheckStates) return;
    updatingCheckStates = true;

    bool checked = (state == Qt::Checked);
    showTargetType targetType;
    // 更新结构体
    targetType.showAllType = checked;
    targetType.showCarType = checked;
    targetType.showPointType = checked;

    // 勾选 UI
    ui->checkBox_carType->setChecked(checked);
    ui->checkBox_pointType->setChecked(checked);

    updatingCheckStates = false;

    // 通知 customPlot
    ui->customPlot->updateTargetType(targetType);
}

void mmWaveRadar::onIndividualTypeCheckBoxChanged(int)
{
    if (updatingCheckStates) return;
    updatingCheckStates = true;

    showTargetType targetType;
    // 更新结构体
    targetType.showCarType = ui->checkBox_carType->isChecked();
    targetType.showPointType = ui->checkBox_pointType->isChecked();

    bool allChecked = targetType.showCarType && targetType.showPointType;
    targetType.showAllType = allChecked;
    ui->checkBox_allType->setChecked(allChecked);

    updatingCheckStates = false;

    // 通知 customPlot
    ui->customPlot->updateTargetType(targetType);
}

void mmWaveRadar::on_btn_1mbd_clicked()
{
    currentBDDistance = 4.0;
    currentBDAngle = 0;
    isBding = true;
    currentBDCheckBox = ui->checkBox_1mbd;
    ui->customPlot->startBiaoding(currentBDDistance,0);
}


void mmWaveRadar::on_btn_3mbd_clicked()
{
    currentBDDistance = 6.0;
    currentBDAngle = 0;
    isBding = true;
    currentBDCheckBox = ui->checkBox_3mbd;
    ui->customPlot->startBiaoding(currentBDDistance,0);
}


void mmWaveRadar::on_btn_5mbd_clicked()
{
    currentBDDistance = 8.0;
    currentBDAngle = 0;
    isBding = true;
    currentBDCheckBox = ui->checkBox_5mbd;
    ui->customPlot->startBiaoding(currentBDDistance,0);
}


void mmWaveRadar::on_btn_45bd_clicked()
{
    QString text = ui->comboBox_45bd->currentText();
    QRegularExpression re("\\d+");
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        QString numberStr = match.captured(0); // 提取到的第一个数字字符串
        currentBDDistance = numberStr.toDouble();        // 如果你需要整数
    }
    currentBDAngle = 45;
    isBding = true;
    if (currentBDDistance == 3) {
        currentBDCheckBox = ui->checkBox_453mbd;
    } else {
        currentBDCheckBox = ui->checkBox_455mbd;
    }
    ui->customPlot->startBiaoding(currentBDDistance,45);
}


void mmWaveRadar::on_btn_fu45bd_clicked()
{
    QString text = ui->comboBox_fu45bd->currentText();
    QRegularExpression re("\\d+");
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        QString numberStr = match.captured(0); // 提取到的第一个数字字符串
        currentBDDistance = numberStr.toDouble();        // 如果你需要整数
    }
    currentBDAngle = -45;
    isBding = true;
    if (currentBDDistance == 3) {
        currentBDCheckBox = ui->checkBox_fu453mbd;
    } else {
        currentBDCheckBox = ui->checkBox_fu455mbd;
    }
    ui->customPlot->startBiaoding(currentBDDistance,-45);
}


void mmWaveRadar::on_btn_comDoc_clicked()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config/MR76Millimeter-Wave Radar Communication ProtocolV1.2_20200307.pdf"; // 你的 PDF 路径
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

HexInputFilter* mmWaveRadar::setupHexInput(QLineEdit *lineEdit, int maxBytes)
{
    HexInputFilter* filter = new HexInputFilter(maxBytes, lineEdit);
    lineEdit->installEventFilter(filter);
    return filter;
}

bool mmWaveRadar::isDataLengthValid(QLineEdit *lineEdit, QComboBox *comboBox)
{
    QString text = lineEdit->text().simplified().toUpper();
    text.replace(" ", ""); // 删除空格

    // 16进制字符串检查（最多 16 个字符，即 8 字节）
    QRegularExpression hexRegex("^[0-9A-Fa-f]{1,16}$");
    if (!hexRegex.match(text).hasMatch()) {
        qDebug() << "Invalid input format";
        return false;
    }

    // 转换为 QByteArray 并计算字节数
    int dataLength = QByteArray::fromHex(text.toUtf8()).size();

    // 获取 QComboBox 选中的值
    int selectedLength = comboBox->currentText().toInt();
    return dataLength == selectedLength;
}

void mmWaveRadar::on_comboBox_dataLen_currentIndexChanged(int index)
{
    dataFilter->setMaxBytes(index+1);
    ui->lineEdit_data->update();
}


void mmWaveRadar::on_pushButton_send_clicked()
{
    if(!mconnect) {
        QMessageBox::warning(this, "Warning", "Connect the device first!");
        return;
    }
    if (ui->lineEdit_data->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter data!");
        return;
    }
    if (ui->lineEdit_frameID->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the frame ID!");
        return;
    }
    if (ui->lineEdit_sendTimes->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the send count!");
        return;
    }
    if (ui->lineEdit_sendTimes->text() != "1" && ui->lineEdit_sendCycle->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the send interval!");
        return;
    }
    if (!isDataLengthValid(ui->lineEdit_data, ui->comboBox_dataLen)) {
        QMessageBox::warning(this, "Warning", "Data length does not match the configured length!");
        return;
    }
    sendTimes = ui->lineEdit_sendTimes->text().toUInt();
    QString str = ui->lineEdit_frameID->text().simplified().toUpper();
    str.replace(" ", ""); // 删除空格
    bool ok;
    psend.ID = str.toUInt(&ok, 16);
    psend.SendType = ui->comboBox_sendType->currentIndex();
    psend.RemoteFlag = ui->comboBox_frameType->currentIndex();
    psend.ExternFlag = ui->comboBox_frameFormat->currentIndex();
    psend.DataLen = ui->comboBox_dataLen->currentIndex()+1;

    QString text = ui->lineEdit_data->text().simplified().toUpper(); // 去除空格并转换为大写
    text.replace(" ", ""); // 删除空格
    QByteArray byteArray = QByteArray::fromHex(text.toUtf8()); // 转换为 QByteArray
    int length = qMin(byteArray.size(), 8); // 限制最大 8 字节
    memcpy(psend.Data, byteArray.constData(), length);
    sendTimer->start(ui->lineEdit_sendCycle->text().toUInt());
}

void mmWaveRadar::sendData2Dev()
{
    currentTimes++;
    if (currentTimes > sendTimes){
        sendTimer->stop();
        currentTimes = 0;
        return;
    }
    canThread->TransmitCANThread(psend);
}


void mmWaveRadar::on_pushButton_stop_clicked()
{
    if (sendTimer->isActive()) {
        sendTimer->stop();
    }
}


void mmWaveRadar::on_pushButton_22_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Storage Path", ui->lineEdit_savaPath->text());
    if (!folderPath.isEmpty()) {
        ui->lineEdit_savaPath->setText(folderPath);
    }
}


void mmWaveRadar::on_pushButton_23_clicked()
{
    if (ui->lineEdit_saveLine->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the number of rows to save!");
        return;
    }
    if (!ui->checkBox_bin->isChecked() &&!ui->checkBox_excel->isChecked() &&!ui->checkBox_txt->isChecked() &&!ui->checkBox_asc->isChecked() &&!ui->checkBox_blf->isChecked()) {
        QMessageBox::warning(this, "Warning", "Select at least one file format!");
        return;
    }
    model->enableAutoSave(true, ui->lineEdit_savaPath->text());
    // ui->lineEdit_saveLine->setReadOnly(true);
    // ui->pushButton_22->setDisabled(true);
    // 界面滑出
    toggleButton(ui->btn_dataSave);
    // showWgtAnimation(ui->wgt_dataSave);
    // ui->btn_dataSave->setChecked(true);
}

