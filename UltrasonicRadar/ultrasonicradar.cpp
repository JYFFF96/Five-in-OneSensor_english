#include "ultrasonicradar.h"
#include "ui_ultrasonicradar.h"
#include "QChart"
#include <QMessageBox>


extern bool  IsOpenFlag;
constexpr int BIAODING_WUCHA = 5;
UltrasonicRadar::UltrasonicRadar(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::UltrasonicRadar)
{
    ui->setupUi(this);
    // this->showMaximized();
    QIcon icon(":/images/logo.png");
    this->setWindowIcon(icon);
    this->setWindowTitle("Ultrasonic Radar Debugger");
    this->setWindowFlags(Qt::FramelessWindowHint); // 隐藏标题栏
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->centralWidget()->setLayout(ui->verticalLayout_6);

    initUI();
    initTittleBar();
    initWidgets(ui->wgt_dataShow, 800, this->height()-30);
    initWidgets(ui->wgt_connet, 200, 220);
    initWidgets(ui->wgt_scale, 180, 130);
    initWidgets(ui->wgt_radarBD, 220, 250);
    initWidgets(ui->wgt_dataSave, 320, 180);

    connect(ui->btn_closeScale, &QPushButton::clicked, this, &UltrasonicRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeRadarBD, &QPushButton::clicked, this, &UltrasonicRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeConnect, &QPushButton::clicked, this, &UltrasonicRadar::wgtCloseBtnClicked);
    connect(ui->btn_closeDataSave, &QPushButton::clicked, this, &UltrasonicRadar::wgtCloseBtnClicked);

    connect(ui->customPlot, &myCustomPlot::sendRange, this, &UltrasonicRadar::updateAxisRange);
    canThread = new CANThread;
    connect(canThread, &CANThread::send1data, this, &UltrasonicRadar::receiveData);
    connect(canThread, &CANThread::sendSignal, this, &UltrasonicRadar::receiveSignal);

    sendTimer = new QTimer(this);
    connect(sendTimer, &QTimer::timeout, this, &UltrasonicRadar::sendData2Dev);


    enableMouseTrackingForAll(this);
}

UltrasonicRadar::~UltrasonicRadar()
{
    delete ui;
}

void UltrasonicRadar::resizeEvent(QResizeEvent *event)
{
    updateDataShowGeo();
}

void UltrasonicRadar::enableMouseTrackingForAll(QWidget *parent)
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

void UltrasonicRadar::mousePressEvent(QMouseEvent *event)
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

void UltrasonicRadar::mouseMoveEvent(QMouseEvent *event)
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

void UltrasonicRadar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
    isDragging = false;
    isResizing = false;
    resizeDirection = Qt::Edges();
}


void UltrasonicRadar::mouseDoubleClickEvent(QMouseEvent *event)
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
void UltrasonicRadar::checkResizeDirection(const QPoint &cursorPos)
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
void UltrasonicRadar::updateCursorShape(const QPoint &cursorPos)
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
void UltrasonicRadar::resizeWindow(const QPoint &delta)
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

void UltrasonicRadar::initWidgets(QWidget *wgt, int w, int h)
{
    wgt->setParent(this);
    wgt->setGeometry(0,30,w,h);
    wgt->move(-wgt->width()-5, 30);
        wgtShowMap.insert(wgt, false);
}

void UltrasonicRadar::updateDataShowGeo()
{
    // initWidgets(ui->wgt_dataShow, 800, this->height()-30);
    // ui->wgt_dataShow->setParent(this);
    ui->wgt_dataShow->setGeometry(ui->wgt_dataShow->x(),30,800,this->height()-30);
    if (ui->wgt_dataShow->x() > 0) {
        // 设置变化
        ui->wgt_dataShow->setGeometry(5,30,800,this->height()-30);
    }
}

bool UltrasonicRadar::isOutCurrentWgt(QWidget *wgt)
{
    if (wgt->x() > 0) {
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
        return true;
    }

    return false;
}

void UltrasonicRadar::showWgtAnimation(QWidget *wgt)
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

void UltrasonicRadar::parseData(VCI_CAN_OBJ data)
{
    if (data.ID == 0x0611) {
        int radar1dis = ((data.Data[0] >> 4) * 1000) + ((data.Data[0] & 0x0F) * 100) + ((data.Data[1] >> 4) * 10) + (data.Data[1] & 0x0F);
        int radar2dis = ((data.Data[2] >> 4) * 1000) + ((data.Data[2] & 0x0F) * 100) + ((data.Data[3] >> 4) * 10) + (data.Data[3] & 0x0F);
        int radar3dis = ((data.Data[4] >> 4) * 1000) + ((data.Data[4] & 0x0F) * 100) + ((data.Data[5] >> 4) * 10) + (data.Data[5] & 0x0F);
        int radar4dis = ((data.Data[6] >> 4) * 1000) + ((data.Data[6] & 0x0F) * 100) + ((data.Data[7] >> 4) * 10) + (data.Data[7] & 0x0F);
        ui->customPlot->UpdateObstaclesDistance(radar1dis/1000.0, radar2dis/1000.0, radar3dis/1000.0, radar4dis/1000.0);


        if (isBding) {
            switch (currentBdRadar.first) {
            case 1:{
                if (radar1dis >= (currentBdRadar.second*1000 - BIAODING_WUCHA) && radar1dis <= (currentBdRadar.second*1000 + BIAODING_WUCHA)) {
                    ui->customPlot->setArcColor(Qt::green);
                    if (currentBdRadar.second == 1) {
                        ui->checkBox_1radar1m->setChecked(true);
                    } else if (currentBdRadar.second == 3) {
                        ui->checkBox_1radar3m->setChecked(true);
                    }
                } else {
                        ui->customPlot->setArcColor(Qt::red);
                }
                break;
            }
            case 2:{
                if (radar2dis >= currentBdRadar.second*1000 - BIAODING_WUCHA && radar2dis <= currentBdRadar.second*1000 + BIAODING_WUCHA) {
                    ui->customPlot->setArcColor(Qt::green);
                    if (currentBdRadar.second == 1) {
                        ui->checkBox_2radar1m->setChecked(true);
                    } else if (currentBdRadar.second == 3) {
                        ui->checkBox_2radar3m->setChecked(true);
                    }
                } else {
                    ui->customPlot->setArcColor(Qt::red);
                }
                break;
            }
            case 3:{
                if (radar3dis >= currentBdRadar.second*1000 - BIAODING_WUCHA && radar3dis <= currentBdRadar.second*1000 + BIAODING_WUCHA) {
                    ui->customPlot->setArcColor(Qt::green);
                    if (currentBdRadar.second == 1) {
                        ui->checkBox_3radar1m->setChecked(true);
                    } else if (currentBdRadar.second == 3) {
                        ui->checkBox_3radar3m->setChecked(true);
                    }
                } else {
                    ui->customPlot->setArcColor(Qt::red);
                }
                break;
            }
            case 4:{
                if (radar4dis >= currentBdRadar.second*1000 - BIAODING_WUCHA && radar4dis <= currentBdRadar.second*1000 + BIAODING_WUCHA) {
                    ui->customPlot->setArcColor(Qt::green);
                    ui->customPlot->startTimer(true);
                    if (currentBdRadar.second == 1) {
                        ui->checkBox_4radar1m->setChecked(true);
                    } else if (currentBdRadar.second == 3) {
                        ui->checkBox_4radar3m->setChecked(true);
                    }
                } else {
                    ui->customPlot->setArcColor(Qt::red);
                }
                break;
            }
            default:
                break;
            }
        }
    }

}

void UltrasonicRadar::on_btn_connet_clicked()
{
    showWgtAnimation(ui->wgt_connet);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void UltrasonicRadar::on_btn_scale_clicked()
{
    showWgtAnimation(ui->wgt_scale);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void UltrasonicRadar::on_btn_startRadar_clicked()
{

}


void UltrasonicRadar::on_btn_radarBD_clicked()
{
    showWgtAnimation(ui->wgt_radarBD);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void UltrasonicRadar::on_btn_commProtocol_clicked()
{
    showWgtAnimation(ui->wgt_dataShow);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    btn->setChecked(btn->isChecked());
}


void UltrasonicRadar::on_btn_dataSave_clicked()
{
    showWgtAnimation(ui->wgt_dataSave);
    QPushButton *btn = qobject_cast<QPushButton*>(sender()); // 转换为 QPushButton
    bool s = btn->isChecked();
    btn->setChecked(s);
}

void UltrasonicRadar::wgtCloseBtnClicked()
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
        ui->customPlot->deleteArc();
    }
}

void UltrasonicRadar::initUI()
{
    setupButtons();


    QDoubleValidator *validator = new QDoubleValidator(0.0, 500.0, 2, this);
    validator->setNotation(QDoubleValidator::StandardNotation);
    ui->lineEdit_xAxisRange->setValidator(validator);
    ui->lineEdit_yAxisRange->setValidator(validator);
    QIntValidator *validator1 = new QIntValidator(1, INT_MAX, this); // 只允许输入 1 到最大正整数
    ui->lineEdit_sendCycle->setValidator(validator1);
    ui->lineEdit_sendTimes->setValidator(validator1);
    setupHexInput(ui->lineEdit_frameID, 8);
    dataFilter = setupHexInput(ui->lineEdit_data, 8);

    QIntValidator *validator2 = new QIntValidator(1, 9999999, this);
    ui->lineEdit_saveLine->setValidator(validator2);

    // 初始化模型
    model = new CanModel(this);
    connect(model, &CanModel::newDataAdded, this, &UltrasonicRadar::scrollToBottom);
    connect(model, &CanModel::saveFinished, this, &UltrasonicRadar::saveFileFinished);
    ui->tableView->setModel(model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // 允许手动调整列宽

    ui->tableView->setColumnWidth(0, 50);
    ui->tableView->setColumnWidth(1, 60);
    ui->tableView->setColumnWidth(2, 60);
    ui->tableView->setColumnWidth(3, 60);
    ui->tableView->setColumnWidth(4, 60);
    ui->tableView->setColumnWidth(5, 155);
    ui->tableView->setColumnWidth(6, 440);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);  // 让最后一列填充剩余空间
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 让表头充满表格
    ui->tableView->verticalHeader()->setVisible(false);  // 隐藏行表头
    ui->tableView->setCornerButtonEnabled(false);       // 彻底隐藏左上角空白
    // 标定界面 checkBox不可点击
    ui->checkBox_1radar1m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_1radar3m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_2radar1m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_2radar3m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_3radar1m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_3radar3m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_4radar1m->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->checkBox_4radar3m->setAttribute(Qt::WA_TransparentForMouseEvents, true);



    ui->lineEdit_savaPath->setText(QCoreApplication::applicationDirPath() + "/data");
    connect(ui->checkBox_bin, &QCheckBox::toggled, model, &CanModel::setSaveBinary);
    connect(ui->checkBox_txt, &QCheckBox::toggled, model, &CanModel::setSaveTxt);
    connect(ui->checkBox_asc, &QCheckBox::toggled, model, &CanModel::setSaveAsc);
    connect(ui->checkBox_blf, &QCheckBox::toggled, model, &CanModel::setSaveBlf);
    connect(ui->checkBox_excel, &QCheckBox::toggled, model, &CanModel::setSaveExcel);
}

void UltrasonicRadar::initTittleBar()
{
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


void UltrasonicRadar::on_btn_setRange_clicked()
{
    double x = ui->lineEdit_xAxisRange->text().toDouble();
    double y = ui->lineEdit_yAxisRange->text().toDouble();
    ui->customPlot->xAxis->setRange(-x, x);
    ui->customPlot->yAxis->setRange(-y, y);
    ui->customPlot->replot();

    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    QWidget *wgt = btn->parentWidget();
    showWgtAnimation(wgt);
}

void UltrasonicRadar::receiveMsg(QString str)
{

}

void UltrasonicRadar::receiveData(VCI_CAN_OBJ data)
{
    if (isPaused) {
        // 暂停状态，把数据存入缓存
        bufferedData.append(qMakePair(CanMsgType::Receive, data));
    } else {
        // 直接更新表格
        model->appendData(CanMsgType::Receive, data);
    }
    parseData(data);
}

void UltrasonicRadar::receiveSignal(bool flag, VCI_CAN_OBJ data)
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

void UltrasonicRadar::on_btn_connectDev_clicked()
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
            ui->btn_connectDev->setText("Disconnect");//
            QString str = "0601";
            str.replace(" ", ""); // 删除空格
            bool ok;
            psend.ID = str.toUInt(&ok, 16);
            psend.SendType = 0;
            psend.RemoteFlag = 0;
            psend.ExternFlag = 0;
            psend.DataLen = 8;

            QString text = "B1100F0000000000"; // 去除空格并转换为大写
            text.replace(" ", ""); // 删除空格
            QByteArray byteArray = QByteArray::fromHex(text.toUtf8()); // 转换为 QByteArray
            int length = qMin(byteArray.size(), 8); // 限制最大 8 字节
            memcpy(psend.Data, byteArray.constData(), length);
            canThread->TransmitCANThread(psend);
            mconnect=true;
        }
    }
    else
    {
        canThread->closeDevice();
        canThread->stop();//停止子线程
        ui->btn_connectDev->setText("Connect");//
        mconnect=false;
        ui->customPlot->deltePCurve();
    }
}


void UltrasonicRadar::on_btn_clearTable_clicked()
{
    model->clearData();
}


void UltrasonicRadar::on_btn_pause_clicked()
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


void UltrasonicRadar::on_checkBox_parseData_toggled(bool checked)
{
    model->updateParsing(checked); // 触发数据更新
}

void UltrasonicRadar::scrollToBottom()
{
    QScrollBar *scrollBar = ui->tableView->verticalScrollBar();

    // 只有当滚动条在底部时才滚动
    if (scrollBar->value() == scrollBar->maximum()) {
        ui->tableView->scrollToBottom();
    }
}

void UltrasonicRadar::updateAxisRange(double xRange, double yRange)
{
    ui->lineEdit_xAxisRange->setText(QString::number(xRange, 'f', 2));
    ui->lineEdit_yAxisRange->setText(QString::number(yRange, 'f', 2));

}

void UltrasonicRadar::sendData2Dev()
{
    currentTimes++;
    if (currentTimes > sendTimes){
        sendTimer->stop();
        currentTimes = 0;
        return;
    }
    canThread->TransmitCANThread(psend);
}


void UltrasonicRadar::on_pushButton_send_clicked()
{
    if(!mconnect) {
        QMessageBox::warning(this, "Warning", "Connect the device first！");
        return;
    }
    if (ui->lineEdit_data->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter data！");
        return;
    }
    if (ui->lineEdit_frameID->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the Frame IDID！");
        return;
    }
    if (ui->lineEdit_sendTimes->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the send count！");
        return;
    }
    if (ui->lineEdit_sendTimes->text() != "1" && ui->lineEdit_sendCycle->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the send interval！");
        return;
    }
    if (!isDataLengthValid(ui->lineEdit_data, ui->comboBox_dataLen)) {
        QMessageBox::warning(this, "Warning", "Data length does not match the configured length！");
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

bool UltrasonicRadar::isDataLengthValid(QLineEdit *lineEdit, QComboBox *comboBox) {
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

    qDebug() << "Input bytes:" << dataLength << " Selected bytes:" << selectedLength;

    return dataLength == selectedLength;
}


void UltrasonicRadar::on_comboBox_dataLen_currentIndexChanged(int index)
{
    dataFilter->setMaxBytes(index+1);
    ui->lineEdit_data->update();
}

HexInputFilter* UltrasonicRadar::setupHexInput(QLineEdit *lineEdit, int maxBytes) {
    HexInputFilter* filter = new HexInputFilter(maxBytes, lineEdit);
    lineEdit->installEventFilter(filter);
    return filter;
}

void UltrasonicRadar::on_btn_radar1bd_clicked()
{
    int dis = ui->comboBox_radar1->currentText().remove(QRegularExpression("[A-Za-z]")).toUInt();
    ui->customPlot->startBiaoding(1, dis);
    currentBdRadar.first = 1;
    currentBdRadar.second = dis;
    isBding = true;
    // changedColor = false;
}


void UltrasonicRadar::on_btn_radar2bd_clicked()
{
    int dis = ui->comboBox_radar2->currentText().remove(QRegularExpression("[A-Za-z]")).toUInt();
    ui->customPlot->startBiaoding(2, dis);
    currentBdRadar.first = 2;
    currentBdRadar.second = dis;
    isBding = true;
    // changedColor = false;
}


void UltrasonicRadar::on_btn_radar3bd_clicked()
{
    int dis = ui->comboBox_radar3->currentText().remove(QRegularExpression("[A-Za-z]")).toUInt();
    ui->customPlot->startBiaoding(3, dis);
    currentBdRadar.first = 3;
    currentBdRadar.second = dis;
    isBding = true;
    // changedColor = false;
}


void UltrasonicRadar::on_btn_radar4bd_clicked()
{
    int dis = ui->comboBox_radar4->currentText().remove(QRegularExpression("[A-Za-z]")).toUInt();
    ui->customPlot->startBiaoding(4, dis);
    currentBdRadar.first = 4;
    currentBdRadar.second = dis;
    isBding = true;
    // changedColor = false;
    // ui->customPlot->deleteArc();
}


void UltrasonicRadar::on_pushButton_stop_clicked()
{
    if (sendTimer->isActive()) {
        sendTimer->stop();
    }
}


void UltrasonicRadar::on_pushButton_22_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Storage Path", ui->lineEdit_savaPath->text());
    if (!folderPath.isEmpty()) {
        ui->lineEdit_savaPath->setText(folderPath);
    }
}


void UltrasonicRadar::on_lineEdit_savaPath_textChanged(const QString &arg1)
{
    // model->enableAutoSave(ui->checkBox_savaData->isChecked(), arg1);
}


void UltrasonicRadar::on_checkBox_savaData_toggled(bool checked)
{
    model->enableAutoSave(checked, ui->lineEdit_savaPath->text());
    ui->lineEdit_saveLine->setReadOnly(checked);
    ui->pushButton_22->setDisabled(checked);
}


// "QPushButton { border: none; background: url(:/images/button_checked.png); }"
//     "QPushButton:checked { background: url(:/images/Top_checked.png); }";
void UltrasonicRadar::setupButtons()
{
    // 这里假设 ui->xxx 是 Qt 设计师里定义的按钮
    buttons = { ui->btn_connet, ui->btn_scale, ui->btn_radarBD,
               ui->btn_commProtocol, ui->btn_dataSave };

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

void UltrasonicRadar::toggleButton(QPushButton *clickedBtn)
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

void UltrasonicRadar::setButtonStyle(QPushButton *btn, bool isSelected) {
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

void UltrasonicRadar::on_lineEdit_saveLine_textChanged(const QString &arg1)
{
    model->saveSaveLine(arg1.toUInt());
}

void UltrasonicRadar::saveFileFinished()
{
    // ui->checkBox_savaData->setChecked(false);
}


void UltrasonicRadar::on_btn_comDoc_clicked()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config/F40-16TR4B-CAN  User Manual V20.pdf"; // 你的 PDF 路径
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}


void UltrasonicRadar::on_lineEdit_xAxisRange_textChanged(const QString &arg1)
{

    if (arg1.isEmpty()) return;

    double xMax = arg1.toDouble();
    double xMin = -xMax; // 保持对称
    double aspectRatio = (double)ui->customPlot->axisRect()->width() / ui->customPlot->axisRect()->height();

    double yMax = xMax / aspectRatio;
    double yMin = -yMax;

    // 更新 Y 轴 LineEdit
    ui->lineEdit_yAxisRange->blockSignals(true);
    ui->lineEdit_yAxisRange->setText(QString("%1").arg(yMax));
    ui->lineEdit_yAxisRange->blockSignals(false);
}


void UltrasonicRadar::on_lineEdit_yAxisRange_textEdited(const QString &arg1)
{
    if (arg1.isEmpty()) return;

    double yMax = arg1.toDouble();
    double yMin = -yMax;
    double aspectRatio = (double)ui->customPlot->axisRect()->width() / ui->customPlot->axisRect()->height();

    double xMax = yMax * aspectRatio;
    double xMin = -xMax;

    // 更新 X 轴 LineEdit
    ui->lineEdit_xAxisRange->blockSignals(true);
    ui->lineEdit_xAxisRange->setText(QString("%1").arg(xMax));
    ui->lineEdit_xAxisRange->blockSignals(false);
}

double UltrasonicRadar::adjustYAxisRange(QCustomPlot *customPlot, double xMin, double xMax)
{
    // 计算 X 轴范围
    double xRange = xMax - xMin;

    // 获取 QCustomPlot 的绘图区宽高比
    double aspectRatio = (double)customPlot->axisRect()->width() / customPlot->axisRect()->height();

    // 计算 Y 轴范围，使得 X:Y = 1:1
    double yRange = xRange / aspectRatio;

    // 获取 Y 轴的中心
    double yCenter = customPlot->yAxis->range().center();

    // 设置 X 轴范围
    // customPlot->xAxis->setRange(xMin, xMax);

    return (yCenter + yRange / 2);
    // // 设置 Y 轴范围（居中）
    // customPlot->yAxis->setRange(yCenter - yRange / 2, yCenter + yRange / 2);

    // // 重新绘制
    // customPlot->replot();
}

double UltrasonicRadar::adjustXAxisRange(QCustomPlot *customPlot, double yMin, double yMax)
{
    // 计算 Y 轴范围
    double yRange = yMax - yMin;

    // 获取 QCustomPlot 的绘图区宽高比
    double aspectRatio = (double)customPlot->axisRect()->width() / customPlot->axisRect()->height();

    // 计算 X 轴范围，使得 X:Y = 1:1
    double xRange = yRange * aspectRatio;

    // 获取 X 轴的中心
    double xCenter = customPlot->xAxis->range().center();

    return (xCenter + xRange / 2);
    // // 设置 Y 轴范围
    // customPlot->yAxis->setRange(yMin, yMax);

    // // 设置 X 轴范围（居中）
    // customPlot->xAxis->setRange(xCenter - xRange / 2, xCenter + xRange / 2);

    // // 重新绘制
    // customPlot->replot();
}

void UltrasonicRadar::on_pushButton_23_clicked()
{
    if (ui->lineEdit_saveLine->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the number of rows to save！");
        return;
    }
    if (!ui->checkBox_bin->isChecked() &&!ui->checkBox_excel->isChecked() &&!ui->checkBox_txt->isChecked() &&!ui->checkBox_asc->isChecked() &&!ui->checkBox_blf->isChecked()) {
        QMessageBox::warning(this, "Warning", "Select at least one file format！");
        return;
    }
    model->enableAutoSave(true, ui->lineEdit_savaPath->text());
    // ui->lineEdit_saveLine->setReadOnly(true);
    // ui->pushButton_22->setDisabled(true);
    // 界面滑出
    toggleButton(ui->btn_dataSave);
    showWgtAnimation(ui->wgt_dataSave);
    // ui->btn_dataSave->setChecked(true);
}

