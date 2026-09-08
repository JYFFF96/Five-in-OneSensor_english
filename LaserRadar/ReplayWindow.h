
#ifndef REPLAYWINDOW_H
#define REPLAYWINDOW_H

#include <QWidget>
#include "PlaybackController.h"
#include "simplepointcloudview.h"

namespace Ui {
class ReplayWindow;
}

class ReplayWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ReplayWindow(QWidget *parent = nullptr);
    ~ReplayWindow();

    bool loadPCAP(const QString &filePath);

private slots:
    void onFrameReady(const QList<PointXYZI> &cloud, int frameIdx);
    void onPlayPauseClicked();
    void onSliderMoved(int value);
    void onLoopToggled(bool checked);

private:
    Ui::ReplayWindow *ui;
    PlaybackController *controller;
    bool isPlaying = false;
};

#endif // REPLAYWINDOW_H
