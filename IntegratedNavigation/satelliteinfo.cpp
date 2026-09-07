#include "satelliteinfo.h"
#include <QPolarChart>
#include <QScatterSeries>
#include <qdebug.h>
#include <qvalueaxis.h>
// QT_CHARTS_USE_NAMESPACE    // 引入命名空间，必须放在ui_widget.h前
#include "ui_satelliteinfo.h"
#include "QChartView"
SatelliteInfo::SatelliteInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SatelliteInfo)
{
    ui->setupUi(this);
}

SatelliteInfo::~SatelliteInfo()
{
    delete ui;
}
