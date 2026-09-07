#include "common.h"


struct RadarStatus {
    uint8_t nvmReadStatus;
    uint8_t nvmWriteStatus;
    uint16_t maxDistanceCfg;
    uint8_t sensorID;
    uint8_t sortIndex;
    uint8_t radarPowerCfg;
    uint8_t outputTypeCfg;
    uint8_t canBaudRate;
    uint8_t rcsThreshold;
    uint8_t calibrationEnabled;
};

RadarStatus parseRadarStatus(const uint8_t data[8]) {
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits = (bits << 8) | data[i];
    }

    RadarStatus status;
    status.nvmReadStatus       = (bits >> (63 - 6)) & 0x1;
    status.nvmWriteStatus      = (bits >> (63 - 7)) & 0x1;
    status.maxDistanceCfg      = ((bits >> (63 - 31)) & 0x3FF) * 2;
    status.sensorID            = (bits >> (63 - 34)) & 0x7;
    status.sortIndex           = (bits >> (63 - 38)) & 0x7;
    status.radarPowerCfg       = (bits >> (63 - 41)) & 0x7;
    status.outputTypeCfg       = (bits >> (63 - 43)) & 0x3;
    status.canBaudRate         = (bits >> (63 - 55)) & 0x7;
    status.rcsThreshold        = (bits >> (63 - 60)) & 0x7;
    status.calibrationEnabled  = (bits >> (63 - 63)) & 0x3;

    return status;
}

QString radarStatusToString(const RadarStatus& s) {
    QString read  = s.nvmReadStatus == 0 ? "Failed" : "Succeeded";
    QString write = s.nvmWriteStatus == 0 ? "Failed" : "Succeeded";

    QString sort;
    switch (s.sortIndex) {
    case 0: sort = "None"; break;
    case 1: sort = "By Distance"; break;
    case 2: sort = "By RCS"; break;
    default: sort = "Unknown"; break;
    }

    QString power;
    switch (s.radarPowerCfg) {
    case 0: power = "Standard"; break;
    case 1: power = "Low Power"; break;
    default: power = "Unknown"; break;
    }

    QString output;
    switch (s.outputTypeCfg) {
    case 0: output = "No Output"; break;
    case 1: output = "Object Output"; break;
    case 2: output = "Raw Radar Data"; break;
    default: output = "Unknown"; break;
    }

    QString baud;
    switch (s.canBaudRate) {
    case 0: baud = "500Kbps"; break;
    case 1: baud = "250Kbps"; break;
    case 2: baud = "125Kbps"; break;
    default: baud = "Unknown"; break;
    }

    QString rcs;
    switch (s.rcsThreshold) {
    case 0: rcs = "Standard Sensitivity"; break;
    case 1: rcs = "High Sensitivity"; break;
    default: rcs = "Unknown"; break;
    }

    QString calib;
    switch (s.calibrationEnabled) {
    case 0: calib = "Off"; break;
    case 1: calib = "Automatic"; break;
    case 2: calib = "Initial Recovery"; break;
    default: calib = "Unknown"; break;
    }

    return QString("NVMRead Status:%1, NVMWrite Status:%2, Maximum Range:%3m, RadarID:%4, Sort Index:%5, Radar Power:%6, Output Type:%7, CANBaud Rate:%8, RCSSensitivity:%9, Calibration Status:%10")
        .arg(read)
        .arg(write)
        .arg(s.maxDistanceCfg)
        .arg(s.sensorID)
        .arg(sort)
        .arg(power)
        .arg(output)
        .arg(baud)
        .arg(rcs)
        .arg(calib);
}

QString getStateString(uint8_t dynProp) {
    switch (dynProp) {
    case 0x0: return "moving";
    case 0x1: return "stationary";
    case 0x2: return "oncoming";
    case 0x3: return "crossing left";
    case 0x4: return "crossing right";
    case 0x5: return "unknown";
    case 0x6: return "stopped";
    default: return "unknown";
    }
}

QString getClassString(uint8_t objClass) {
    switch (objClass) {
    case 0x0: return "point";
    case 0x1: return "car";
    default: return "unknown";
    }
}

QString parseBbstacleData(VCI_CAN_OBJ data)
{
    QString text;
    if (data.ID == 0x060b) {
    uint8_t id = data.Data[0];
    double distLong = ((data.Data[1] * 32 + (data.Data[2] >> 3)) * 0.2) - 500;

    // 目标横向距离
    double distLat = (((data.Data[2] & 0x07) * 256 + data.Data[3]) * 0.2) - 204.6;

    // 目标纵向速度
    double vrelLong = ((data.Data[4] * 4 + (data.Data[5] >> 6)) * 0.25) - 128;

    // 目标横向速度
    double vrelLat = (((data.Data[5] & 0x3F) * 8 + (data.Data[6] >> 5)) * 0.25) - 64;

    // 目标动态属性
    uint8_t dynProp = data.Data[6] & 0x07;
    uint8_t objClass1 = (data.Data[6] >> 3) & 0x03;  // (0x18 >> 3)
    // RCS
    uint8_t rcs = (data.Data[7] * 0.5) - 64;

    QString state = getStateString(dynProp);
    QString objClass = getClassString(objClass1);
    text = QString("TargetID:%1,Longitudinal Distance:%2m,Lateral Distance:%3m,Longitudinal Velocity:%4m/s,Lateral Velocity:%5m/s,Dynamic Property%6,Type:%7，RCS:%8")
                       .arg(id)
                       .arg(distLong)
                       .arg(distLat)
                       .arg(vrelLong)
                       .arg(vrelLat)
                       .arg(state)
                       .arg(objClass)
                       .arg(rcs);
    } else if (data.ID == 0x060a) {
        uint8_t objCount = data.Data[0];
        uint16_t measCount = (static_cast<uint16_t>(data.Data[1]) << 8) | data.Data[2];
        uint8_t interfaceVersion = (data.Data[3] >> 4) & 0x0F;
        text = QString("Target Count:%1,Measurement Cycle Count:%2,Interface Version:%3")
                   .arg(objCount)
                   .arg(measCount)
                   .arg(interfaceVersion);
    } else if (data.ID == 0x0201) {
        uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) {
            bits = (bits << 8) | data.Data[i];
        }

        RadarStatus status;
        status.nvmReadStatus       = (bits >> (63 - 6)) & 0x1;
        status.nvmWriteStatus      = (bits >> (63 - 7)) & 0x1;
        status.maxDistanceCfg      = ((bits >> (63 - 31)) & 0x3FF) * 2;
        status.sensorID            = (bits >> (63 - 34)) & 0x7;
        status.sortIndex           = (bits >> (63 - 38)) & 0x7;
        status.radarPowerCfg       = (bits >> (63 - 41)) & 0x7;
        status.outputTypeCfg       = (bits >> (63 - 43)) & 0x3;
        status.canBaudRate         = (bits >> (63 - 55)) & 0x7;
        status.rcsThreshold        = (bits >> (63 - 60)) & 0x7;
        status.calibrationEnabled  = (bits >> (63 - 63)) & 0x3;
        text = radarStatusToString(status);
    } else if (data.ID == 0x700) {
        uint8_t MajorRelease = data.Data[0];
        uint8_t MinorRelease = data.Data[1];
        uint8_t PatchLevel = data.Data[2];
        text = QString("Software Major Version:%1,Software Minor Version:%2,Software Patch Version:%3")
                   .arg(MajorRelease)
                   .arg(MinorRelease)
                   .arg(PatchLevel);
    }


    return text;
}
