#pragma once

#include "relife_types.h"
#include <stddef.h>

/**
 * @brief SSD1306 OLED Status Display Subsystem
 * Displays real-time cell voltage, current, temperature, interlock status,
 * SD health, and buffer overflow warnings.
 */
class RigDisplay {
public:
    RigDisplay();
    bool init();
    void update(const TelemetryRecord_t* record, bool sd_healthy, size_t buffer_usage);
    void showWarning(const char* warning);
    void showFatalError(const char* message);

private:
    char last_warning_[32];
    bool warning_active_;
};
