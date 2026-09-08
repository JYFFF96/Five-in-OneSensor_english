#include "exportframedialog.h"
#include "ui_exportframedialog.h"
#include <QButtonGroup>
#include <QFileDialog>

ExportFrameDialog::ExportFrameDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportFrameDialog)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);

    // 1. 帧选择互斥
    QButtonGroup* frameGroup = new QButtonGroup(this);
    frameGroup->addButton(ui->radioCurrent);
    frameGroup->addButton(ui->radioAll);
    frameGroup->addButton(ui->radioRange);
    frameGroup->setExclusive(true);

    // 2. 保存方式互斥
    QButtonGroup* saveGroup = new QButtonGroup(this);
    saveGroup->addButton(ui->radioSingle);
    saveGroup->addButton(ui->radioMulti);
    saveGroup->setExclusive(true);

    ui->groupBox_save->setEnabled(false);

    // 3. spinBox 设置范围 0~1
    ui->spinStart->setRange(0, 1);
    ui->spinEnd->setRange(0, 100);
    ui->spinStart->setValue(0);
    ui->spinEnd->setValue(1);

    // 4. 保证 start ≤ end
    connect(ui->spinStart, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int start){
        int end = ui->spinEnd->value();
        if (start > end) {
            ui->spinStart->setValue(end);
        }
    });

    connect(ui->spinEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int end){
        int start = ui->spinStart->value();
        if (end < start) {
            ui->spinEnd->setValue(start);
        }
    });

    connect(ui->radioCurrent, &QRadioButton::toggled, this, &ExportFrameDialog::updateSaveGroupVisibility);
    connect(ui->radioAll, &QRadioButton::toggled, this, &ExportFrameDialog::updateSaveGroupVisibility);
    connect(ui->radioRange, &QRadioButton::toggled, this, &ExportFrameDialog::updateSaveGroupVisibility);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ExportFrameDialog::onDialogAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ExportFrameDialog::reject);
}

ExportFrameDialog::~ExportFrameDialog()
{
    delete ui;
}

void ExportFrameDialog::SetExportType(ExportType type)
{
    currentExportType = type;
    // if (type == ExportType::CSV || type == ExportType::PCAP) {
    //     ui->groupBox_frame->setVisible(true);
    //     ui->groupBox_save->setVisible(false);
    // } else if (type == ExportType::LAS || type == ExportType::PCD) {
    //     ui->groupBox_frame->setVisible(true);
    //     ui->groupBox_save->setVisible(true);
    // } else {
    //     ui->groupBox_frame->setVisible(true);
    //     ui->groupBox_save->setVisible(true);
    // }
}

FrameType ExportFrameDialog::getFrameMode() const
{
    if (ui->radioCurrent->isChecked()) return currentFrame;
    if (ui->radioAll->isChecked()) return allFrames;
    if (ui->radioRange->isChecked()) return rangeFrame;
    return UnknownFrame;
}

int ExportFrameDialog::getStartFrame() const
{
    return ui->spinStart->value();
}

int ExportFrameDialog::getEndFrame() const
{
    return ui->spinEnd->value();
}

FileType ExportFrameDialog::getSaveMode() const
{
    if (ui->radioSingle->isChecked()) return singleFile;
    if (ui->radioMulti->isChecked()) return perFrameFile;
    return UnknownFile;
}

ExportParams ExportFrameDialog::getExportParams() const
{
    ExportParams params;
    params.exportType = this->currentExportType;               // 添加保存类型变量
    params.frameType = getFrameMode();
    params.startFrame = getStartFrame();
    params.endFrame = getEndFrame();
    params.fileType = getSaveMode();
    return params;
}

void ExportFrameDialog::onDialogAccepted()
{
    ExportParams params = getExportParams();

    QString filter;
    QString path;
    if (params.exportType == ExportType::CSV) filter = "CSV File (*.csv)";
    else if (params.exportType == ExportType::PCAP) filter = "PCAP File (*.pcap)";
    else if (params.exportType == ExportType::LAS) filter = "LAS File (*.las)";
    else if (params.exportType == ExportType::PCD) filter = "PCD File (*.pcd)";
    else filter = "All Files (*)";


    if (params.fileType == singleFile) {
        path = QFileDialog::getSaveFileName(this, "Select Export File", "", filter);
    } else {
        path = QFileDialog::getExistingDirectory(this, "Select Export Directory");
    }

    if (path.isEmpty()) return;  // 用户取消选择路径

    params.filePath = path;

    emit signalExportParamsReady(params);  // 发出信号给主窗口
    accept();  // 关闭窗口
}

void ExportFrameDialog::updateSaveGroupVisibility()
{
    if (ui->radioCurrent->isChecked()) {
        ui->groupBox_save->setEnabled(false);
        ui->radioSingle->setChecked(true);
    } else {
        ui->groupBox_save->setEnabled(true);
    }
}
