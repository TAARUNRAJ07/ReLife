#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "relife_types.h"

typedef struct {
    float min_pack_voltage_v;
    float max_pack_voltage_v;
    float min_cell_voltage_v;
    float max_cell_voltage_v;
    float max_discharge_current_a;
    float max_charge_current_a;
    float min_temp_c;
    float max_temp_c;
    uint8_t expected_cell_count;
    float max_cell_sum_delta_v;
    uint32_t max_sample_age_ms;
} BatteryLimitsConfig_t;

/**
 * Default limits matching rules/battery_limits.yaml (1S 3.7V cell)
 */
static inline BatteryLimitsConfig_t defaultBatteryLimits(void) {
    BatteryLimitsConfig_t c;
    c.min_pack_voltage_v = 2.50f;
    c.max_pack_voltage_v = 4.35f;
    c.min_cell_voltage_v = 2.50f;
    c.max_cell_voltage_v = 4.35f;
    c.max_discharge_current_a = 3.50f;
    c.max_charge_current_a = -2.00f;
    c.min_temp_c = -10.0f;
    c.max_temp_c = 55.0f;
    c.expected_cell_count = 1;
    c.max_cell_sum_delta_v = 0.100f; // 100 mV
    c.max_sample_age_ms = 3000;      // 3.0 seconds
    return c;
}

class TelemetryValidator {
public:
    TelemetryValidator(BatteryLimitsConfig_t config = defaultBatteryLimits());
    
    void setConfig(const BatteryLimitsConfig_t& config);
    const BatteryLimitsConfig_t& getConfig() const;
    
    uint32_t validateRecord(TelemetryRecord_t* record, uint32_t current_time_ms = 0);
    bool checkBatteryId(const char* battery_id);

private:
    BatteryLimitsConfig_t limits_;
};
