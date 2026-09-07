#ifndef COMMON_H
#define COMMON_H
#include "stdint.h"
#include <qstring.h>
#include "ECanVci.h"
#include "ControlCAN.h"

struct showTargetState {
    bool showAllTarget = true;
    bool showMovingTarget = true;
    bool showStationaryTarget = true;
    bool showOncomingTarget = true;
    bool showCrossingLeftTarget = true;
    bool showCrossingRightTarget = true;
    bool showStoppedTarget = true;
    bool showUnknownTarget = true;
};

struct showTargetType {
    bool showAllType = true;
    bool showPointType = true;
    bool showCarType = true;
};

struct LabelDisplayOptions {
    bool showAngle = true;
    bool showRange = true;
    bool showSpeed = true;
    bool showRcs = true;
    bool showState = true;
    bool showClass = true;
};

typedef struct {
    uint8_t id;
    double distLong;
    double distLat;
    double vrelLong;
    double vrelLat;
    uint8_t dynProp;
    uint8_t objClass;       // 2bit
    double   rcs;
    double distance;
    double angle;
    double speed;
} ObstacleInfo;

QString parseBbstacleData(VCI_CAN_OBJ data);


#endif // COMMON_H
