#ifndef SATELLITEINFO_H
#define SATELLITEINFO_H

#include <QWidget>
#include "qchartview.h"

namespace Ui {
class SatelliteInfo;
}

class SatelliteInfo : public QWidget
{
    Q_OBJECT

public:
    explicit SatelliteInfo(QWidget *parent = nullptr);
    ~SatelliteInfo();

private:
    Ui::SatelliteInfo *ui;
};

#endif // SATELLITEINFO_H
