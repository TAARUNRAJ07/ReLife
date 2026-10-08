#include "display.h"
#include <stdio.h>
#include <string.h>

#ifndef UNIT_TEST
#include <Arduino.h>
#endif

RigDisplay::RigDisplay() : warning_active_(false) {
    memset(last_warning_, 0, sizeof(last_warning_));
}

bool RigDisplay::init() {
#ifndef UNIT_TEST
    Serial.println("[OLED] SSD1306 Display Initialized at I2C 0x3C (Simulated/Ready)");
#endif
    return true;
}

void RigDisplay::showWarning(const char* warning) {
    if (warning) {
        strncpy(last_warning_, warning, sizeof(last_warning_) - 1);
        warning_active_ = true;
#ifndef UNIT_TEST
        Serial.printf("[OLED_WARN] *** DISPLAY WARNING: %s ***\n", last_warning_);
#endif
    }
}

void RigDisplay::update(const TelemetryRecord_t* record, bool sd_healthy, size_t buffer_usage) {
    if (!record) return;

#ifndef UNIT_TEST
    // Formatted multi-line status display (simulating 128x64 OLED lines)
    // Line 1: V: 3.72V  I: 1.00A
    // Line 2: T: 25.4C  Mode: disch
    // Line 3: Interlock: ARMED
    // Line 4: SD: OK (Buf: 2/32) or SD: FAULT (Buf: 32/32 OVERFLOW!)
    char line[128];
    snprintf(line, sizeof(line),
        "[OLED] V:%.2fV I:%.2fA T:%.1fC | ILOCK:%s | SD:%s (Buf:%u) %s",
        record->pack_voltage_v.ok ? record->pack_voltage_v.value : 0.0f,
        record->current_a.ok ? record->current_a.value : 0.0f,
        record->temps_c[0].ok ? record->temps_c[0].value : 0.0f,
        interlockStateToString(record->interlock_state),
        sd_healthy ? "OK" : "ERR",
        (unsigned int)buffer_usage,
        warning_active_ ? last_warning_ : ""
    );
    // Print debug status periodically on Serial if needed
#endif
}

void RigDisplay::showFatalError(const char* message) {
    if (message) {
        strncpy(last_warning_, message, sizeof(last_warning_) - 1);
        warning_active_ = true;
#ifndef UNIT_TEST
        Serial.printf("[OLED_FATAL] FATAL: %s\n", message);
#endif
    }
}
