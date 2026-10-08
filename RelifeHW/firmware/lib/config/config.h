#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Calibration & Battery Limit Configuration Storage (NVS / EEPROM)
 */
class RigConfig {
public:
    RigConfig();
    bool init();
    const char* getCalibrationVersion();
    float getVoltageDividerRatio();
    float getShuntResistanceOhms();
    float getOvervoltageLimitV();
    float getUndervoltageLimitV();
    float getOvertempLimitC();
    float getMaxDischargeCurrentA();
};
