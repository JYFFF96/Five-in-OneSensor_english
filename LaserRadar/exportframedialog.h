#ifndef EXPORTFRAMEDIALOG_H
#define EXPORTFRAMEDIALOG_H

#include <QDialog>
#include "common.h"

namespace Ui {
class ExportFrameDialog;
}

class ExportFrameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportFrameDialog(QWidget *parent = nullptr);
    ~ExportFrameDialog();

    void SetExportType(ExportType type);

    FrameType getFrameMode() const;   // "当前帧" / "所有帧" / "范围"
    int getStartFrame() const;      // 起始帧
    int getEndFrame() const;        // 结束帧
    FileType getSaveMode() const;    // "单个文件" / "每帧文件"
    ExportParams getExportParams() const;
signals:
    void signalExportParamsReady(ExportParams params);
private slots:
    void onDialogAccepted();
    void updateSaveGroupVisibility();

private:
    Ui::ExportFrameDialog *ui;

    ExportType currentExportType;
};


#endif // EXPORTFRAMEDIALOG_H
