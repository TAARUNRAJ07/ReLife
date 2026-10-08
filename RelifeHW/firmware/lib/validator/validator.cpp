#include "validator.h"
#include <string.h>
#include <math.h>

TelemetryValidator::TelemetryValidator(BatteryLimitsConfig_t config) : limits_(config) {}

void TelemetryValidator::setConfig(const BatteryLimitsConfig_t& config) {
    limits_ = config;
}

const BatteryLimitsConfig_t& TelemetryValidator::getConfig() const {
    return limits_;
}

uint32_t TelemetryValidator::validateRecord(TelemetryRecord_t* record, uint32_t current_time_ms) {
    if (!record) return QUALITY_FLAG_SENSOR_FAULT;

    uint32_t flags = record->quality_flags;

    // 1. Check Battery ID format (^RL-BAT-[0-9]{4}$)
    if (!checkBatteryId(record->battery_id)) {
        flags |= QUALITY_FLAG_SENSOR_FAULT;
    }

    // 2. Stale data check (> 3 s old)
    if (current_time_ms > 0 && record->uptime_ms > 0) {
        if ((current_time_ms > record->uptime_ms) && ((current_time_ms - record->uptime_ms) > limits_.max_sample_age_ms)) {
            flags |= QUALITY_FLAG_STALE_DATA;
        }
    }

    // 3. INA226 conversion ready check
    if (record->pack_voltage_v.error_code == SENSOR_ERR_CONVERSION_NOT_READY ||
        record->current_a.error_code == SENSOR_ERR_CONVERSION_NOT_READY) {
        flags |= QUALITY_FLAG_INA226_NOT_READY;
        record->pack_voltage_v.ok = false;
        record->current_a.ok = false;
    }

    // 4. BMS CRC failure check
    if (record->bms_soc_pct.error_code == SENSOR_ERR_CRC_MISMATCH ||
        record->bms_soh_pct.error_code == SENSOR_ERR_CRC_MISMATCH) {
        flags |= QUALITY_FLAG_BMS_CRC_FAIL;
        record->bms_soc_pct.ok = false;
        record->bms_soh_pct.ok = false;
    }

    // 5. NaN check across all floating-point readings
    if (isnan(record->pack_voltage_v.value)) {
        flags |= QUALITY_FLAG_NAN_READING;
        record->pack_voltage_v.ok = false;
    }
    if (isnan(record->current_a.value)) {
        flags |= QUALITY_FLAG_NAN_READING;
        record->current_a.ok = false;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if (isnan(record->cell_voltages_v[i].value)) {
            flags |= QUALITY_FLAG_NAN_READING;
            record->cell_voltages_v[i].ok = false;
        }
        if (isnan(record->temps_c[i].value)) {
            flags |= QUALITY_FLAG_NAN_READING;
            record->temps_c[i].ok = false;
        }
    }

    // 6. DS18B20 disconnect (-127 C) and uninitialized power-on reset (+85.0 C)
    for (uint8_t i = 0; i < 4; i++) {
        float t = record->temps_c[i].value;
        if (record->temps_c[i].error_code == SENSOR_ERR_DISCONNECTED || (t <= -126.9f && t >= -127.1f)) {
            flags |= QUALITY_FLAG_DS18B20_DISCONNECT;
            record->temps_c[i].ok = false;
        }
        if (record->temps_c[i].error_code == SENSOR_ERR_POWERON_RESET_85C || (t >= 84.99f && t <= 85.01f)) {
            flags |= QUALITY_FLAG_DS18B20_POWERON_85C;
            record->temps_c[i].ok = false;
        }
    }

    // 7. Range checks against config
    // Pack voltage range
    if (record->pack_voltage_v.ok) {
        if (record->pack_voltage_v.value < limits_.min_pack_voltage_v ||
            record->pack_voltage_v.value > limits_.max_pack_voltage_v) {
            flags |= QUALITY_FLAG_RANGE_VIOLATION;
            record->pack_voltage_v.ok = false;
        }
    } else {
        flags |= QUALITY_FLAG_SENSOR_FAULT;
    }

    // Current range
    if (record->current_a.ok) {
        if (record->current_a.value > limits_.max_discharge_current_a ||
            record->current_a.value < limits_.max_charge_current_a) {
            flags |= QUALITY_FLAG_RANGE_VIOLATION;
            record->current_a.ok = false;
        }
    } else {
        flags |= QUALITY_FLAG_SENSOR_FAULT;
    }

    // Cell voltages range (for active cells up to cell_count)
    uint8_t cells_to_check = record->cell_count > 0 ? record->cell_count : limits_.expected_cell_count;
    if (cells_to_check > 4) cells_to_check = 4;
    for (uint8_t i = 0; i < cells_to_check; i++) {
        if (record->cell_voltages_v[i].ok) {
            if (record->cell_voltages_v[i].value < limits_.min_cell_voltage_v ||
                record->cell_voltages_v[i].value > limits_.max_cell_voltage_v) {
                flags |= QUALITY_FLAG_RANGE_VIOLATION;
                record->cell_voltages_v[i].ok = false;
            }
        }
    }

    // Temperature range
    for (uint8_t i = 0; i < 4; i++) {
        if (record->temps_c[i].ok) {
            if (record->temps_c[i].value < limits_.min_temp_c ||
                record->temps_c[i].value > limits_.max_temp_c) {
                flags |= QUALITY_FLAG_RANGE_VIOLATION;
                record->temps_c[i].ok = false;
            }
        }
    }

    // 8. Cell count mismatch
    if (record->cell_count != limits_.expected_cell_count) {
        flags |= QUALITY_FLAG_CELL_COUNT_MISMATCH;
    }

    // 9. Sum of cells versus pack voltage (|Vpack - sum(Vcell)| > max_cell_sum_delta_v)
    if (record->pack_voltage_v.ok) {
        float cell_sum = 0.0f;
        bool all_cells_valid = true;
        for (uint8_t i = 0; i < cells_to_check; i++) {
            if (record->cell_voltages_v[i].ok) {
                cell_sum += record->cell_voltages_v[i].value;
            } else {
                all_cells_valid = false;
                break;
            }
        }
        if (all_cells_valid && cells_to_check > 0) {
            float delta = fabsf(record->pack_voltage_v.value - cell_sum);
            if (delta > limits_.max_cell_sum_delta_v) {
                flags |= QUALITY_FLAG_CELL_SUM_MISMATCH;
                flags |= QUALITY_FLAG_VOLTAGE_DIV_NOISE;
            }
        }
    }

    record->quality_flags = flags;
    return flags;
}

bool TelemetryValidator::checkBatteryId(const char* battery_id) {
    if (!battery_id) return false;
    // Check ^RL-BAT-[0-9]{4}$ format (11 characters)
    if (strlen(battery_id) != 11) return false;
    if (strncmp(battery_id, "RL-BAT-", 7) != 0) return false;
    for (int i = 7; i < 11; i++) {
        if (battery_id[i] < '0' || battery_id[i] > '9') return false;
    }
    return true;
}
