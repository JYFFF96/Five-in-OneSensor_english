#include "mainwindow.h"
 // QT_CHARTS_USE_NAMESPACE    // 引入命名空间，必须放在ui_widget.h前
#include "ui_mainwindow.h"
#include <QPolarChart>
#include "QValueAxis"
#include <QLineSeries>
#include <QtSerialPort/QSerialPortInfo>
#include <QDebug>
#include <QDoubleValidator>
#include <QDir>
#include <QDateTime>
#include <QFileDialog>
#include <QWebChannel>
#include <QWebEngineSettings>
#include <QProcess>
#include <QMessageBox>
#include "imagedialog.h"

    QMap<QString, QSerialPort::StopBits> stopBitsList = { {"1", QSerialPort::OneStop}, {"1.5", QSerialPort::OneAndHalfStop}, {"2", QSerialPort::TwoStop}};
QMap<QString, QSerialPort::DataBits> dataBitsList = {{"5", QSerialPort::Data5}, {"6", QSerialPort::Data6}, {"7", QSerialPort::Data7}, {"8", QSerialPort::Data8}};
QMap<QString, QSerialPort::Parity> parityList = {{"None", QSerialPort::NoParity}, {"Even", QSerialPort::EvenParity}, {"Odd", QSerialPort::OddParity}};
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QIcon icon(":/images/logo.png");
    this->setWindowIcon(icon);
    setWindowTitle("Integrated Navigation Debugger");
    showMaximized();
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    refreshSerialPortTimer = new QTimer(this);
    connect(refreshSerialPortTimer, &QTimer::timeout, this, &MainWindow::UpdateSerialPort);
    refreshSerialPortTimer->start(1000);

    initSatelliteInfo();

    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);

    connect(this, &MainWindow::sendYaw, ui->widget_compass, &CompassWidget::updateAngle);
    connect(this, &MainWindow::sendRoll, ui->widget_arc, &ArcWidget::updateRoll);
    connect(this, &MainWindow::sendPitch, ui->widget_arc, &ArcWidget::updatePitch);

    savePath = QDir::currentPath();
    savePath = savePath + "/data";
    ui->lineedit_savePath->setText(savePath);

    initBaiduOnlineMap();

    satelliteCounter = new SatelliteCounter(this);
    connect(satelliteCounter, &SatelliteCounter::sendVisualSatelliteList, this, &MainWindow::showVisualSatelliteList);


    // 设置当前时间
    ui->startTime->setDateTime(QDateTime::currentDateTime());

    // 设置最小日期为 2000-01-01
    ui->startTime->setMinimumDate(QDate(2000, 1, 1));

    ui->stackedWidget->setCurrentIndex(0);
    ui->stackedWidget_2->setCurrentIndex(0);

    initTittleBar();
    QCustomPlot *customPlot = this->ui->cnrPlot;
    customPlot->setAttribute(Qt::WA_TranslucentBackground);  // 让 QWidget 透明
    customPlot->setBackground(Qt::transparent);             // 设置 QCustomPlot 透明
    customPlot->axisRect()->setBackground(Qt::transparent); // 设置坐标区域背景透明
    customPlot->xAxis->setBasePen(QPen(Qt::white));
    customPlot->yAxis->setBasePen(QPen(Qt::white));

    // 设置 X 轴 和 Y 轴 刻度线颜色
    customPlot->xAxis->setTickPen(QPen(Qt::white));
    customPlot->yAxis->setTickPen(QPen(Qt::white));

    // 设置 X 轴 和 Y 轴 刻度标签颜色
    customPlot->xAxis->setTickLabelColor(Qt::white);
    customPlot->yAxis->setTickLabelColor(Qt::white);

    enableMouseTrackingForAll(this);
    // ui->widget_28->setStyleSheet("border: 1px solid red;");
    simulator = new TrajectorySimulator();
    connect(simulator, &TrajectorySimulator::sendCurrentPos, this, &MainWindow::slotSendCurrentPos);
    connect(simulator, &TrajectorySimulator::sendCurrentCarInfo, this, &MainWindow::slotReceviedCarInfo);

    fastCmdDlg = new FastCommandDialog;
    connect(fastCmdDlg, &FastCommandDialog::sendCommand, this, &MainWindow::slotSendFastCmd);
    fastCmdDlg->hide();
}

void MainWindow::enableMouseTrackingForAll(QWidget *parent)
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

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initTittleBar()
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

void MainWindow::setupButtons()
{
    // 这里假设 ui->xxx 是 Qt 设计师里定义的按钮
    buttons = { ui->btn_tsbd, ui->btn_fzdata};

    // 初始化状态
    foreach (QPushButton *btn , buttons) {
        if (btn != ui->btn_tsbd) {
        buttonStates[btn] = false;  // 初始状态：未选中
        setButtonStyle(btn, false); // 默认透明背景
        } else {
            buttonStates[btn] = true;  // 初始状态：未选中
            setButtonStyle(btn, true); // 默认透明背景
        }

        // 绑定点击事件
        connect(btn, &QPushButton::clicked, this, [this, btn]() {
            toggleButton(btn);
        });
    }
}

void MainWindow::setButtonStyle(QPushButton *btn, bool isSelected) {
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

void MainWindow::toggleButton(QPushButton *clickedBtn)
{
    // 取消所有按钮的选中状态
    foreach (QPushButton *btn, buttons) {
        buttonStates[btn] = false;
        setButtonStyle(btn, false);
    }


        buttonStates[clickedBtn] = true;
        setButtonStyle(clickedBtn, true);


    if (clickedBtn == ui->btn_tsbd) {
        ui->stackedWidget->setCurrentIndex(0);
    } else if (clickedBtn == ui->btn_fzdata) {
        ui->stackedWidget->setCurrentIndex(1);
    }
}

void MainWindow::parseSentence(QByteArray data)
{


    if (data.startsWith("$GINS")) {
        // 组合导航数据
        QList<QByteArray> imuList = data.split(',');
        if (imuList.length() < 19) {
            return;
        }
        ui->INS_roll->setText(imuList.at(10));
        ui->arc_roll->setText(imuList.at(10));
        emit sendRoll(imuList.at(10).toDouble());
        ui->INS_pitch->setText(imuList.at(9));
        ui->arc_pitch->setText(imuList.at(10));
        emit sendPitch(imuList.at(9).toDouble());
        ui->INS_yaw->setText(imuList.at(10));
        ui->compass_yaw->setText(imuList.at(11));
        emit sendYaw(imuList.at(11).toDouble());
        ui->INS_time->setText(imuList.at(2));
        ui->INS_week->setText(imuList.at(1));
        ui->INS_lon->setText(imuList.at(4));
        ui->INS_lat->setText(imuList.at(3));
        ui->INS_height->setText(imuList.at(5));
        ui->INS_vn->setText(imuList.at(7));
        ui->INS_ve->setText(imuList.at(6));
        ui->INS_vd->setText(imuList.at(8));
        Postion mypos;
        mypos.height = imuList.at(5).toDouble()/1000; // 千米
        mypos.latitude = imuList.at(3).toDouble();
        mypos.longitude = imuList.at(4).toDouble();
        mypos.time = QDateTime::currentDateTime();
        satelliteCounter->StartParseData(mypos);
    } else if (data.startsWith("$GRIMU")) {
        // IMU数据
        QList<QByteArray> imuList = data.split(',');
        if (imuList.length() < 11) {
            return;
        }
        ui->IMU_gpsWeek->setText(imuList.at(1));
        ui->IMU_gpsSecond->setText(imuList.at(2));
        ui->IMU_X_angularSpeed->setText(imuList.at(4));
        ui->IMU_Y_angularSpeed->setText(imuList.at(5));
        ui->IMU_Z_angularSpeed->setText(imuList.at(6));
        ui->IMU_X_acceleration->setText(imuList.at(7));
        ui->IMU_Y_acceleration->setText(imuList.at(8));
        ui->IMU_Z_acceleration->setText(imuList.at(9));

        ui->INS_X_angularSpeed->setText(imuList.at(4));
        ui->INS_Y_angularSpeed->setText(imuList.at(5));
        ui->INS_Z_angularSpeed->setText(imuList.at(6));
        ui->INS_X_acceleration->setText(imuList.at(7));
        ui->INS_Y_acceleration->setText(imuList.at(8));
        ui->INS_Z_acceleration->setText(imuList.at(9));

        ui->IMU_temperature->setText(imuList.at(10).split('*').at(0));

    }
}


void MainWindow::initSatelliteInfo()
{

}

void MainWindow::updatePortList(QStringList list)
{
    ui->serialPort->clear();
    // 添加到串口列表中
    foreach (const QString port, list) {
        ui->serialPort->addItem(port);
    }
}

void MainWindow::saveDatatoFile(QByteArray data)
{
    data.remove(data.length()-2,1);
    QDir dir;
    if (!dir.exists(savePath)) {
        if (!dir.mkpath(savePath)) {  // 递归创建目录
            qDebug() << "Failed to create directory:" << savePath;
            return;
        }
    }
    QString fileName = savePath + "/" + QDateTime::currentDateTime().toString("yyyyMMddHH") + ".txt";
    QFile file(fileName);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Cannot open file for writing:" << fileName;
        return;
    }

    QTextStream out(&file);
    out << data;
    file.close();
}

void MainWindow::initBaiduOnlineMap()
{
    QString htmlPath = QCoreApplication::applicationDirPath() + "/config/";
    QString htmlFile = htmlPath + "index.html";
    /* 创建一个与网页交互的通道 */
    ui->mapView->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    ui->mapView->setUrl(htmlFile);
    QWebChannel *webChannel = new QWebChannel(ui->mapView->page());
    bridge = new MapBridge;
    connect(bridge, &MapBridge::sendPoints, this, &MainWindow::receiveCoordinatePoints);
    webChannel->registerObject("bridge", bridge);
    ui->mapView->page()->setWebChannel(webChannel);
    ui->mapView->page()->runJavaScript("document.body.style.overflow = 'hidden';");
}

QList<SatelliteGsvData> MainWindow::analyzeGsvStatements(QString nmeaGSV)
{
    QList<SatelliteGsvData> satellites;
    QStringList lines = nmeaGSV.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QStringList parts = line.split(',');

        if (parts.size() < 4 || !parts[0].startsWith("$")) {
            qDebug() << "Invalid GSV sentence: " << line;
            continue;
        }

        // 获取卫星系统类型
        QString systemPrefix = parts[0].mid(1, 2); // 例如 "GP", "GL", "GB", "GA", "QZ"
        QString systemName;
        if (systemPrefix == "GP") systemName = "GPS";
        else if (systemPrefix == "GL") systemName = "GLONASS";
        else if (systemPrefix == "GB") systemName = "BeiDou";
        else if (systemPrefix == "GA") systemName = "Galileo";
        else if (systemPrefix == "QZ") systemName = "QZSS";
        else {
            qDebug() << "Unknown satellite system: " << systemPrefix;
            continue;
        }

        int totalMessages = parts[1].toInt();   // 总GSV语句数
        int messageNumber = parts[2].toInt();   // 当前GSV语句编号
        int totalSatellites = parts[3].toInt(); // 可见卫星总数

        // qDebug() << "卫星系统:" << systemName
        //          << " 总GSV条数:" << totalMessages
        //          << ", 当前编号:" << messageNumber
        //          << ", 总卫星数:" << totalSatellites;

        // 解析卫星数据
        for (int i = 4; i + 3 < parts.size(); i += 4) {
            SatelliteGsvData sat;
            sat.name = systemName + " " + parts[i];   // 卫星类型 + 编号
            sat.elevation = parts[i + 1].toDouble(); // 俯仰角
            sat.azimuth = parts[i + 2].toDouble();   // 方位角
            sat.snr = (parts[i + 3].isEmpty()) ? -1 : parts[i + 3].toInt(); // 信噪比，若无则设为 -1
            satellites.append(sat);

            // qDebug() << "卫星:" << sat.name << " 仰角:" << sat.elevation
            //          << " 方位角:" << sat.azimuth << " 信噪比:" << (sat.snr == -1 ? "N/A" : QString::number(sat.snr));
        }
    }

    return satellites;
}

void MainWindow::updateCnrPlot(QList<SatelliteGsvData> data)
{
    QCustomPlot *customPlot = this->ui->cnrPlot;
    // customPlot->legend->setVisible(true); // 设置图例可见
    customPlot->clearPlottables();  // 移除所有绘图对象
    customPlot->clearItems();       // 移除所有自定义项（如图例、文本）
    // 准备数据
    QVector<double> ticks;
    QVector<QString> labels;

    for (int i = 0; i < data.size(); ++i) {
        ticks.append(i + 1); // x 轴为 1, 2, 3, ...
        labels.append(data[i].name.split(" ").last()); // 只显示编号
    }

    // **创建多个 QCPBars，每个条形单独设置颜色**
    for (int i = 0; i < data.size(); ++i) {
        QCPBars *bar = new QCPBars(customPlot->xAxis, customPlot->yAxis);
        bar->setWidth(0.5);
        bar->setData(QVector<double>{ticks[i]}, QVector<double>{(double)data[i].snr});
        bar->setBrush(getSatelliteColor(data[i].name)); // 设定颜色

        // **在柱状图上方显示 SNR 值**
        QCPItemText *textLabel = new QCPItemText(customPlot);
        textLabel->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        textLabel->position->setCoords(ticks[i], data[i].snr); // 稍微高于柱子顶部
        textLabel->setText(QString::number(data[i].snr));
        textLabel->setFont(QFont("Arial", 10));
        textLabel->setColor(Qt::white);
    }

    // 设置 X 轴
    QSharedPointer<QCPAxisTickerText> textTicker(new QCPAxisTickerText);
    for (int i = 0; i < ticks.size(); ++i) {
        textTicker->addTick(ticks[i], labels[i]); // 只显示编号
    }
    customPlot->xAxis->setTicker(textTicker);
    customPlot->xAxis->setTickLabelRotation(90);
    customPlot->xAxis->setRange(0, ticks.size() + 1);
    customPlot->xAxis->setSubTicks(false);

    // 设置 Y 轴
    customPlot->yAxis->setRange(0, 100);
    // customPlot->yAxis->setLabel("SNR 值");
    customPlot->replot();
}

void MainWindow::updateDataViewFromCarInfo(carPostionInfo carInfo)
{
    int day = carInfo.date.mid(0, 2).toInt();
    int month = carInfo.date.mid(2, 2).toInt();
    int year = carInfo.date.mid(4, 2).toInt() + 2000; // 假设 2000 年以后

    QDate date(year, month, day);
    ui->dataView_date->setText(date.toString("yyyy-MM-dd"));

    int hour = carInfo.time.mid(0, 2).toInt();
    int minute = carInfo.time.mid(2, 2).toInt();
    int second = carInfo.time.mid(4, 2).toInt();

    QTime time(hour, minute, second);
    ui->dataView_time->setText(time.toString("HH:mm:ss"));
    ui->dataView_lat->setText(QString("%1").arg(carInfo.latitude));
    ui->dataView_lon->setText(QString("%1").arg(carInfo.longitude));
    ui->dataView_hight->setText(QString("%1").arg(carInfo.altitude));
    ui->dataView_speed->setText(QString("%1").arg(carInfo.speed));
    ui->dataView_angle->setText(QString("%1").arg(carInfo.heading));
}

void MainWindow::updateDataViewFromGsa(QString gsa)
{
    if (!gsa.startsWith("$GPGSA,")) {
        return;
    }

    // 去掉校验和部分
    int starIndex = gsa.indexOf('*');
    if (starIndex == -1) {
        return; // 无效语句
    }

    QString dataPart = gsa.mid(0, starIndex); // 去掉校验和部分
    QStringList fields = dataPart.split(',');

    if (fields.size() < 9) {
        return; // 字段不足
    }
    ui->dataView_dwMode->setText(fields[1] = "A" ? "Automatic" : "Manual" );
    ui->dataView_wzjd->setText(fields[4]);
    ui->dataView_spjd->setText(fields[5]);
    ui->dataView_czjd->setText(fields[6]);
}

void MainWindow::updateDataViewFromGst(QString gst)
{
    GSTData gstData;

    // 检查 NMEA 语句起始符号
    if (!gst.startsWith("$GPGST,")) {
        return;
    }

    // 去掉校验和部分
    int starIndex = gst.indexOf('*');
    if (starIndex == -1) {
        return; // 无效语句
    }

    QString dataPart = gst.mid(0, starIndex); // 去掉校验和部分
    QStringList fields = dataPart.split(',');

    if (fields.size() < 9) {
        return; // 字段不足
    }

    // 解析各个字段
    gstData.utcTime = fields[1]; // UTC 时间
    gstData.rangeRMS = fields[2].toDouble();
    gstData.stdMajor = fields[3].toDouble();
    gstData.stdMinor = fields[4].toDouble();
    gstData.orient = fields[5].toDouble();
    gstData.stdLat = fields[6].toDouble();
    gstData.stdLon = fields[7].toDouble();
    gstData.stdAlt = fields[8].toDouble();

    ui->dataView_stdLat->setText(fields[6]);
    ui->dataView_stdLon->setText(fields[7]);
    ui->dataView_stdAlt->setText(fields[8]);

}

void MainWindow::UpdateSerialPort()
{
    QStringList list;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        list.append(info.portName());
    }
    list.sort();
    if (portList.isEmpty()) {
        portList = list;
        updatePortList(portList);
    } else {
        if (portList != list) {
            portList = list;
            updatePortList(portList);
        }
    }
}


void MainWindow::on_btn_connect_clicked()
{
    QString port = ui->serialPort->currentText();
    int rate = ui->baudRate->currentText().toInt();
    if (serial->isOpen()) {
        serial->close();
        ui->btn_connect->setText("Connect");
    } else {
        serial->setPortName(ui->serialPort->currentText());
        serial->setBaudRate(ui->baudRate->currentText().toInt());
        serial->setStopBits(stopBitsList.value(ui->stopBits->currentText()));
        serial->setDataBits(dataBitsList.value(ui->dataBits->currentText()));
        serial->setParity(parityList.value(ui->parity->currentText()));
        serial->setFlowControl(QSerialPort::NoFlowControl);

        if (serial->open(QIODevice::ReadWrite)) {
            ui->btn_connect->setText("Disconnect");
        } else {
            QString error = serial->errorString();
            qDebug() << "open failed:" << serial->errorString();
        }
    }
}

void MainWindow::readSerialData()
{
    QByteArray data = serial->readAll();
    qDebug() << data;
    rxBuf.append(data);
    const int MAX_BUF = 128 * 1024;
    if (rxBuf.size() > MAX_BUF)
        rxBuf = rxBuf.right(16 * 1024);

    while (true) {
        int end = rxBuf.indexOf("\r\n");
        if (end < 0) break; // 不完整，等更多数据

        QByteArray line = rxBuf.left(end); // 不含 "\r\n"
        rxBuf.remove(0, end + 2);

        line = line.trimmed();
        if (line.isEmpty()) continue;
        qDebug() << line;
        saveDatatoFile(line + "\r\n");
        // 打印到界面
        if (b_pause) {
            hisData += (line + "\r\n");
        } else {
            ui->showMsgEdit->appendPlainText(line);
        }
        if (line.startsWith('$')) {
            // NMEA/数据语句
            parseSentence(line + "\r\n");
        } else if (line.startsWith('<') && line.endsWith('>')) {
            // 控制应答语句
            if (!sendMsgList.isEmpty()) {
                QString str = sendMsgList.at(0);
                sendMsgList.removeAt(0);
                QByteArray data1 = str.toUtf8(); // 将字符串转换为字节数组
                qint64 bytesWritten = serial->write(data1);
                if (bytesWritten == -1) {
                    qDebug() << "Failed to write data to serial port!";
                } else {
                    qDebug() << "Data sent:" << data1;
                }
            }
        } else {
            // 垃圾/日志/其他：可记录或忽略
            // qDebug() << "Unknown line:" << line;
        }
    }
}

void MainWindow::on_btn_clear_clicked()
{
    ui->showMsgEdit->clear();
}

void MainWindow::on_btn_pause_clicked()
{
    if (b_pause) {
        if (hisData.endsWith("\r\n")) {
            hisData.chop(2);
        }
        ui->showMsgEdit->appendPlainText(hisData);
        hisData.clear();
        ui->btn_pause->setText("Pause");
    } else {
        ui->btn_pause->setText("Resume");
    }
    b_pause = !b_pause;
}

void MainWindow::on_btn_biaoding_clicked()
{
    if (!serial->isOpen()) {
        QMessageBox::warning(this, "Warning", "Connect the device first！");
        return;
    }
    // 主天线杆臂值(x,y,z):
    QString leverMsg = QString("$HSET,LEVER1,%1,%2,%3\r\n").arg(ui->lever1_x->value()).arg(ui->lever1_y->value()).arg(ui->lever1_z->value());
    QByteArray data = leverMsg.toUtf8(); // 将字符串转换为字节数组
    qint64 bytesWritten = serial->write(data);
    if (bytesWritten == -1) {
        qDebug() << "Failed to write data to serial port!";
    } else {
        qDebug() << "Data sent:" << data;
    }
    QString lever2Msg = QString("$HSET,LEVER2,%1,%2,%3\r\n").arg(ui->lever2_x->value()).arg(ui->lever2_y->value()).arg(ui->lever2_z->value());
    sendMsgList.append(lever2Msg);
    // // 后轴中心杆臂值(x,y,z):
    QString vbMsg = QString("$HSET,VB,%1,%2,%3\r\n").arg(ui->vb_x->value()).arg(ui->vb_y->value()).arg(ui->vb_z->value());
    sendMsgList.append(vbMsg);
    // // 后轮轮距(ws):
    QString wsMsg = QString("$HSET,WS,%1\r\n").arg(ui->ws->value());
    sendMsgList.append(wsMsg);

}


void MainWindow::on_btn_send_clicked()
{
    if (!serial->isOpen()) {
        return;
    }
    QString msg = ui->plainTextEdit->toPlainText()+"\r\n";
    QByteArray data = msg.toUtf8(); // 将字符串转换为字节数组
    qint64 bytesWritten = serial->write(data);
    if (bytesWritten == -1) {
        qDebug() << "Failed to write data to serial port!";
    } else {
        qDebug() << "Data sent:" << data;
    }
}


void MainWindow::on_btn_savePath_clicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Select Storage Path", savePath);
    if (!folderPath.isEmpty()) {
        savePath = folderPath;
        ui->lineedit_savePath->setText(savePath);
    } else {

    }
}

void MainWindow::receiveCoordinatePoints(QVector<GPSPoint> points)
{
    simulator->stop();
    m_points = points;
    QString strLat, strLon;
    foreach (GPSPoint point, points) {
        strLat += QString("%1").arg(point.latitude) + ",";
        strLon += QString("%1").arg(point.longitude) + ",";
    }
    strLat.remove(strLat.length()-1,1);
    strLon.remove(strLon.length()-1,1);
    ui->point_lat->setText(strLat);
    ui->point_lon->setText(strLon);
}


void MainWindow::on_btn_start_clicked()
{
    if (!connectFlag) {
        QMessageBox::warning(this, "Warning", "Connect the device first！");
        return;
    }
    if (m_points.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Select a route first！");
        return;
    }
    if (ui->speed->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the travel speed！");
        return;
    }
    if (ui->height->text().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Enter the travel altitude！");
        return;
    }
    ui->postioningView->clearData();
    ;
    simulator->setTrajectory(m_points, ui->startTime->dateTime(), ui->speed->text().toDouble(), ui->height->text().toDouble());  // 速度 20 m/s
    simulator->updateSegment();

}

void MainWindow::slotSendCurrentPos(GPSPoint currentPos)
{
    QJsonObject json;
    json["lat"] = currentPos.latitude;
    json["lng"] = currentPos.longitude;
    QString message = QJsonDocument(json).toJson(QJsonDocument::Compact);
    bridge->sendMessageToJs(message);
}

void MainWindow::showVisualSatelliteList(QVector<SatelliteData> list)
{
    qDebug() << "Received list";
    // 开始绘制
    ui->polarPlot->setSatelliteData(list);
}

void MainWindow::slotReceviedCarInfo(carPostionInfo carInfo)
{
    // 生成NMEA语句
    QString rmc = generateRMC(carInfo.time, carInfo.latitude, carInfo.longitude, carInfo.speed, carInfo.heading, carInfo.date);
    QString vtg = generateVTG(carInfo.speed, carInfo.heading);
    QString gga = generateGGA(carInfo.time, carInfo.latitude, carInfo.longitude, carInfo.altitude);
    QString gsa = generateGSA();
    QString gsv = generateGSV();
    QString gll = generateGLL(carInfo.time, carInfo.latitude, carInfo.longitude);
    QString gst = generateGST(carInfo.time);
    // 输出所有语句
    ui->plainTextEdit_2->appendPlainText(rmc);
    qDebug() << ui->plainTextEdit->toPlainText();
    ui->operationLog->appendPlainText(rmc);
    ui->plainTextEdit_2->appendPlainText(vtg);
    ui->operationLog->appendPlainText(vtg);
    ui->plainTextEdit_2->appendPlainText(gga);
    ui->operationLog->appendPlainText(gga);
    ui->plainTextEdit_2->appendPlainText(gsa);
    ui->operationLog->appendPlainText(gsa);
    updateDataViewFromGsa(gsa);
    ui->plainTextEdit_2->appendPlainText(gsv);
    ui->operationLog->appendPlainText(gsv);
    QList<SatelliteGsvData> gsvList = analyzeGsvStatements(gsv);
    qDebug() << gsvList.count();
    // 绘制载噪比柱状图和极坐标系
    ui->simulatePolarPlot->setSatelliteData(gsvList);
    updateCnrPlot(gsvList);
    ui->postioningView->addCoordinate(carInfo.latitude, carInfo.longitude);
    ui->plainTextEdit_2->appendPlainText(gll);
    ui->operationLog->appendPlainText(gll);
    ui->plainTextEdit_2->appendPlainText(gst);
    ui->operationLog->appendPlainText(gst);
    updateDataViewFromCarInfo(carInfo);
    updateDataViewFromGst(gst);
}

void MainWindow::on_btn_nextPage_clicked()
{
    int currentIndex = ui->stackedWidget_2->currentIndex();
    int nextIndex = (currentIndex + 1) % ui->stackedWidget_2->count(); // 如果是最后一页，返回第一页
    ui->stackedWidget_2->setCurrentIndex(nextIndex);
}


void MainWindow::on_btn_connect2_clicked()
{
    connectFlag = !connectFlag;  // 切换标志状态
    ui->btn_connect2->setText(connectFlag ? "Disconnect" : "Connect");
}


void MainWindow::on_btn_prevPage_clicked()
{
    int currentIndex = ui->stackedWidget_2->currentIndex();
    int pageCount = ui->stackedWidget_2->count();

    if (currentIndex == 0) {
        ui->stackedWidget_2->setCurrentIndex(pageCount - 1);  // 回到最后一页
    } else {
        ui->stackedWidget_2->setCurrentIndex(currentIndex - 1);
    }
}


void MainWindow::on_btn_openDoc_clicked()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config/YFZ-2018-5068 Integrated Navigation Module Configuration ProtocolA2-20230307.pdf"; // 你的 PDF 路径
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}


void MainWindow::on_ben_openExample_clicked()
{
    QString filePath = QCoreApplication::applicationDirPath() + "/config/Lever Arm Measurement Example.png"; // 你的图片路径
    // QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    ImageDialog::showModal(this, filePath);
}

// **事件过滤器：拦截 tabBar 的双击事件**
// bool MainWindow::eventFilter(QObject *obj, QEvent *event)
// {
//     if (obj == ui->tabWidget) {
//         QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
//         QPoint tabBarMousePos = mouseEvent->pos();
//         QTabBar *tabBar = ui->tabWidget->tabBar();

//         // 计算空白区域
//         int tabBarWidth = tabBar->width();
//         int fullWidth = ui->tabWidget->width();
//         int emptyX = tabBarWidth;
//         int emptyWidth = fullWidth - tabBarWidth;

//         QRect emptyArea(emptyX, 0, emptyWidth, tabBar->height());

//         // 判断是否点击了 TabBar 的空白区域
//         if (emptyArea.contains(tabBarMousePos)) {
//             if (event->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton) {
//                 qDebug() << "双击了 TabBar 右侧空白区域，切换窗口大小";
//                 if (isMaximized()) {
//                     showNormal();
//                     maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
//                 } else {
//                     showMaximized();
//                     maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
//                 }
//             }
//             if (event->type() == QEvent::MouseButtonPress && mouseEvent->button() == Qt::LeftButton) {
//                 if (this->isMaximized()) {
//                     // 先恢复窗口
//                     this->showNormal();
//                 }
//                 // 开始拖动窗口
//                 this->move(mouseEvent->globalPos() - this->rect().topLeft());
//             }
//             return true;

//         }
//     }
//     return QMainWindow::eventFilter(obj, event);
// }

void MainWindow::mousePressEvent(QMouseEvent *event)
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

void MainWindow::mouseMoveEvent(QMouseEvent *event)
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

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    mousePressed = false;
    isDragging = false;
    isResizing = false;
    resizeDirection = Qt::Edges();
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
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
void MainWindow::checkResizeDirection(const QPoint &cursorPos)
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
void MainWindow::updateCursorShape(const QPoint &cursorPos)
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
void MainWindow::resizeWindow(const QPoint &delta)
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

// **切换最大化/还原**
void MainWindow::toggleMaximizeRestore()
{
    if (isMaximized()) {
        showNormal();
        maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    } else {
        showMaximized();
        maxButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
    }
}


void MainWindow::on_pushButton_clicked()
{
    fastCmdDlg->show();
}

void MainWindow::slotSendFastCmd(const QString &commandContent)
{
    if (!serial->isOpen()) {
        return;
    }
    QString msg = commandContent+"\r\n";
    QByteArray data = msg.toUtf8(); // 将字符串转换为字节数组
    qint64 bytesWritten = serial->write(data);
    if (bytesWritten == -1) {
        qDebug() << "Failed to write data to serial port!";
    } else {
        qDebug() << "Data sent:" << data;
    }
}

