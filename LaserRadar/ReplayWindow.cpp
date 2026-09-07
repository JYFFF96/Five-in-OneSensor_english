
#include "ReplayWindow.h"
#include "ui_ReplayWindow.h"
#include <QFileDialog>
#include <QMessageBox>

ReplayWindow::ReplayWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReplayWindow),
    controller(new PlaybackController(this))
{
    ui->setupUi(this);

    connect(controller, &PlaybackController::frameReady, this, &ReplayWindow::onFrameReady);
    connect(ui->btnPlayPause, &QPushButton::clicked, this, &ReplayWindow::onPlayPauseClicked);
    connect(ui->sliderProgress, &QSlider::valueChanged, this, &ReplayWindow::onSliderMoved);
    connect(ui->btnLoop, &QPushButton::toggled, this, &ReplayWindow::onLoopToggled);
}

ReplayWindow::~ReplayWindow()
{
    delete ui;
}

bool ReplayWindow::loadPCAP(const QString &filePath)
{
    if (!controller->loadPCAP(filePath)) return false;
    ui->sliderProgress->setMaximum(controller->frameCount() - 1);
    controller->seekToFrame(0);
    return true;
}

void ReplayWindow::onFrameReady(const QList<PointXYZI> &cloud, int frameIdx)
{
    ui->spinFrame->setValue(frameIdx);
    ui->sliderProgress->setValue(frameIdx);
    ui->viewPointCloud->updatePointCloud(cloud);  // 你应在 SimplePointCloudView 中定义此接口
}

void ReplayWindow::onPlayPauseClicked()
{
    isPlaying = !isPlaying;
    controller->setSpeed(1.0);
    isPlaying ? controller->start() : controller->pause();
    ui->btnPlayPause->setText(isPlaying ? "⏸" : "▶");
}

void ReplayWindow::onSliderMoved(int value)
{
    controller->seekToFrame(value);
}

void ReplayWindow::onLoopToggled(bool checked)
{
    ui->btnLoop->setText(checked ? "Off" : "Loop");
    controller->setLoop(checked);
}
