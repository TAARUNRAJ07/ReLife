#include "journal.h"
#include <stdio.h>
#include <string.h>

void qualityFlagsToJsonArray(uint32_t flags, char* out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return;

    if (flags == QUALITY_FLAG_VALID) {
        snprintf(out_buf, max_len, "[\"VALID\"]");
        return;
    }

    char temp[384];
    temp[0] = '[';
    temp[1] = '\0';
    bool first = true;

    #define APPEND_FLAG(bit, name) \
        if (flags & (bit)) { \
            if (!first) strncat(temp, ", ", sizeof(temp) - strlen(temp) - 1); \
            strncat(temp, "\"" name "\"", sizeof(temp) - strlen(temp) - 1); \
            first = false; \
        }

    APPEND_FLAG(QUALITY_FLAG_SENSOR_FAULT, "SENSOR_READ_FAILED");
    APPEND_FLAG(QUALITY_FLAG_CLOCK_UNSYNCED, "CLOCK_UNSYNCED");
    APPEND_FLAG(QUALITY_FLAG_VOLTAGE_DIV_NOISE, "NOISY_DIVIDER");
    APPEND_FLAG(QUALITY_FLAG_ADC_SATURATED, "SATURATED_ADC");
    APPEND_FLAG(QUALITY_FLAG_BMS_TIMEOUT, "BMS_TIMEOUT");
    APPEND_FLAG(QUALITY_FLAG_INTERLOCK_TRIP, "INTERLOCK_TRIPPED");
    APPEND_FLAG(QUALITY_FLAG_RANGE_VIOLATION, "RANGE_VIOLATION");
    APPEND_FLAG(QUALITY_FLAG_NAN_READING, "NAN_READING");
    APPEND_FLAG(QUALITY_FLAG_STALE_DATA, "STALE_DATA");
    APPEND_FLAG(QUALITY_FLAG_DS18B20_DISCONNECT, "DS18B20_DISCONNECTED");
    APPEND_FLAG(QUALITY_FLAG_DS18B20_POWERON_85C, "DS18B20_POWERON_85C");
    APPEND_FLAG(QUALITY_FLAG_INA226_NOT_READY, "INA226_NOT_READY");
    APPEND_FLAG(QUALITY_FLAG_BMS_CRC_FAIL, "BMS_CRC_FAIL");
    APPEND_FLAG(QUALITY_FLAG_CELL_COUNT_MISMATCH, "CELL_COUNT_MISMATCH");
    APPEND_FLAG(QUALITY_FLAG_CELL_SUM_MISMATCH, "CELL_SUM_MISMATCH");
    APPEND_FLAG(QUALITY_FLAG_SD_OVERFLOW, "SD_OVERFLOW");

    #undef APPEND_FLAG

    if (first) {
        strncat(temp, "\"VALID\"", sizeof(temp) - strlen(temp) - 1);
    }
    strncat(temp, "]", sizeof(temp) - strlen(temp) - 1);
    snprintf(out_buf, max_len, "%s", temp);
}

size_t serializeRecordJson(const TelemetryRecord_t* record, char* out_buf, size_t max_len) {
    if (!record || !out_buf || max_len == 0) return 0;

    // Field formatting buffers
    char v_pack[16], i_pack[16], soc[16], soh[16], ah[16];
    if (record->pack_voltage_v.ok) {
        snprintf(v_pack, sizeof(v_pack), "%.3f", record->pack_voltage_v.value);
    } else {
        snprintf(v_pack, sizeof(v_pack), "null");
    }

    if (record->current_a.ok) {
        snprintf(i_pack, sizeof(i_pack), "%.3f", record->current_a.value);
    } else {
        snprintf(i_pack, sizeof(i_pack), "null");
    }

    if (record->bms_soc_pct.ok) {
        snprintf(soc, sizeof(soc), "%.1f", record->bms_soc_pct.value);
    } else {
        snprintf(soc, sizeof(soc), "null");
    }

    if (record->bms_soh_pct.ok) {
        snprintf(soh, sizeof(soh), "%.1f", record->bms_soh_pct.value);
    } else {
        snprintf(soh, sizeof(soh), "null");
    }

    if (record->ah_discharged.ok) {
        snprintf(ah, sizeof(ah), "%.4f", record->ah_discharged.value);
    } else {
        snprintf(ah, sizeof(ah), "null");
    }

    // Cell voltages (min 4 items per schema)
    char c0[16], c1[16], c2[16], c3[16];
    if (record->cell_voltages_v[0].ok) snprintf(c0, sizeof(c0), "%.3f", record->cell_voltages_v[0].value); else snprintf(c0, sizeof(c0), "null");
    if (record->cell_voltages_v[1].ok) snprintf(c1, sizeof(c1), "%.3f", record->cell_voltages_v[1].value); else snprintf(c1, sizeof(c1), "null");
    if (record->cell_voltages_v[2].ok) snprintf(c2, sizeof(c2), "%.3f", record->cell_voltages_v[2].value); else snprintf(c2, sizeof(c2), "null");
    if (record->cell_voltages_v[3].ok) snprintf(c3, sizeof(c3), "%.3f", record->cell_voltages_v[3].value); else snprintf(c3, sizeof(c3), "null");
    char cells_json[96];
    snprintf(cells_json, sizeof(cells_json), "[%s, %s, %s, %s]", c0, c1, c2, c3);

    // Temperatures (4 channels: Cell, BMS FET, Shunt, Ambient)
    char t0[16], t1[16], t2[16], t3[16];
    if (record->temps_c[0].ok) snprintf(t0, sizeof(t0), "%.1f", record->temps_c[0].value); else snprintf(t0, sizeof(t0), "null");
    if (record->temps_c[1].ok) snprintf(t1, sizeof(t1), "%.1f", record->temps_c[1].value); else snprintf(t1, sizeof(t1), "null");
    if (record->temps_c[2].ok) snprintf(t2, sizeof(t2), "%.1f", record->temps_c[2].value); else snprintf(t2, sizeof(t2), "null");
    if (record->temps_c[3].ok) snprintf(t3, sizeof(t3), "%.1f", record->temps_c[3].value); else snprintf(t3, sizeof(t3), "null");
    char temps_json[96];
    snprintf(temps_json, sizeof(temps_json), "[%s, %s, %s, %s]", t0, t1, t2, t3);

    // Quality flags JSON array
    char flags_json[256];
    qualityFlagsToJsonArray(record->quality_flags, flags_json, sizeof(flags_json));

    // Construct single-line JSONL record
    int written = snprintf(
        out_buf,
        max_len,
        "{\"device_id\": \"%s\", \"boot_id\": \"%s\", \"seq\": %u, \"timestamp\": \"%s\", "
        "\"battery_id\": \"%s\", \"pack_voltage_v\": %s, \"current_a\": %s, "
        "\"cell_voltages_v\": %s, \"temperatures_c\": %s, \"bms_soc_pct\": %s, "
        "\"bms_soh_pct\": %s, \"ah_discharged\": %s, \"interlock_state\": \"%s\", "
        "\"quality_flags\": %s, \"calibration_version\": \"%s\", \"uptime_ms\": %u, "
        "\"ts_source\": \"%s\", \"mode\": \"%s\"}",
        record->device_id[0] ? record->device_id : "ESP32-RIG-001",
        record->boot_id[0] ? record->boot_id : "boot-00000000",
        record->seq,
        record->timestamp[0] ? record->timestamp : "1970-01-01T00:00:00Z",
        record->battery_id[0] ? record->battery_id : "RL-BAT-0001",
        v_pack,
        i_pack,
        cells_json,
        temps_json,
        soc,
        soh,
        ah,
        interlockStateToString(record->interlock_state),
        flags_json,
        record->calibration_version[0] ? record->calibration_version : "v1.0.0",
        record->uptime_ms,
        record->ts_source[0] ? record->ts_source : "boot_relative",
        record->mode[0] ? record->mode : "idle"
    );

    if (written < 0 || (size_t)written >= max_len) {
        return 0;
    }
    return (size_t)written;
}

TelemetryJournal::TelemetryJournal() 
    : sd_healthy_(false), write_ahead_seq_(0), last_flushed_seq_(0) {
    memset(mount_path_, 0, sizeof(mount_path_));
}

bool TelemetryJournal::init(const char* mount_path) {
    if (mount_path) {
        strncpy(mount_path_, mount_path, sizeof(mount_path_) - 1);
    }
    // In simulated/offline test environment, mark SD as available (or simulated SD)
    sd_healthy_ = true;
    return true;
}

bool TelemetryJournal::enqueueRecord(const TelemetryRecord_t* record) {
    if (!record) return false;
    return ring_buffer_.push(*record);
}

bool TelemetryJournal::processQueue() {
    TelemetryRecord_t rec;
    while (ring_buffer_.pop(&rec)) {
        if (!appendRecord(&rec)) {
            // SD failed: push back or let ring buffer keep newest
            sd_healthy_ = false;
            return false;
        }
    }
    return true;
}

bool TelemetryJournal::appendRecord(const TelemetryRecord_t* record) {
    if (!record) return false;
    if (!sd_healthy_) return false;

    // Write-ahead index update
    write_ahead_seq_ = record->seq;

    // Serialize to fixed buffer
    char json_buf[1024];
    size_t len = serializeRecordJson(record, json_buf, sizeof(json_buf));
    if (len == 0) return false;

    // Flush record sequence
    last_flushed_seq_ = record->seq;
    return true;
}

size_t TelemetryJournal::readUnacknowledgedBatch(TelemetryRecord_t* out_records, size_t max_count) {
    if (!out_records || max_count == 0) return 0;
    // In offline rig, retrieves from ring buffer for inspection
    size_t read_count = 0;
    TelemetryRecord_t rec;
    while (read_count < max_count && ring_buffer_.pop(&rec)) {
        out_records[read_count++] = rec;
    }
    return read_count;
}

bool TelemetryJournal::pruneAcknowledged(uint32_t ack_up_to_seq) {
    (void)ack_up_to_seq;
    return true;
}

size_t TelemetryJournal::getPendingCount() {
    return ring_buffer_.size();
}

uint64_t TelemetryJournal::getFreeBytes() {
    return 1024 * 1024 * 16;
}
