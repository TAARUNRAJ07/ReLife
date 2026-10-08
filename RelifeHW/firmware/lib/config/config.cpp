#include "config.h"

RigConfig::RigConfig() {}

bool RigConfig::init() {
    return true;
}

const char* RigConfig::getCalibrationVersion() {
    return "v1.0.0";
}

float RigConfig::getVoltageDividerRatio() {
    return 1.0f;
}

float RigConfig::getShuntResistanceOhms() {
    return 0.010f; // 10 mOhm draft
}

float RigConfig::getOvervoltageLimitV() {
    return 4.25f;
}

float RigConfig::getUndervoltageLimitV() {
    return 2.50f;
}

float RigConfig::getOvertempLimitC() {
    return 60.0f;
}

float RigConfig::getMaxDischargeCurrentA() {
    return 10.0f;
}
