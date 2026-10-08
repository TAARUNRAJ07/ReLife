#include <unity.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "relife_types.h"
#include "validator.h"
#include "bms_uart.h"
#include "journal.h"
#include "ring_buffer.h"

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// 1. VALIDATOR TESTS
// ============================================================================

void test_validator_battery_id_format(void) {
    TelemetryValidator validator;

    // Valid ReLife Battery IDs (^RL-BAT-[0-9]{4}$)
    TEST_ASSERT_TRUE(validator.checkBatteryId("RL-BAT-0001"));
    TEST_ASSERT_TRUE(validator.checkBatteryId("RL-BAT-9999"));
    TEST_ASSERT_TRUE(validator.checkBatteryId("RL-BAT-0042"));

    // Invalid ReLife Battery IDs
    TEST_ASSERT_FALSE(validator.checkBatteryId("RL-BAT-42"));        // Too short
    TEST_ASSERT_FALSE(validator.checkBatteryId("RL-BAT-00421"));     // Too long
    TEST_ASSERT_FALSE(validator.checkBatteryId("RL-CELL-0001"));     // Wrong prefix
    TEST_ASSERT_FALSE(validator.checkBatteryId("rl-bat-0001"));      // Lowercase
    TEST_ASSERT_FALSE(validator.checkBatteryId("RL-BAT-ABCD"));      // Non-numeric suffix
    TEST_ASSERT_FALSE(validator.checkBatteryId(NULL));               // Null pointer
}

void test_validator_range_limits(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;

    // Baseline: Valid readings within 1S envelope (3.7V, 1.0A discharge, 25C)
    record.pack_voltage_v = (SensorReadFloat){3.70f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.00f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.70f, true, SENSOR_ERR_NONE};
    record.temps_c[0] = (SensorReadFloat){25.0f, true, SENSOR_ERR_NONE};
    
    uint32_t flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_EQUAL_UINT32(QUALITY_FLAG_VALID, flags);
    TEST_ASSERT_TRUE(record.pack_voltage_v.ok);

    // Over-voltage (> 4.35V for 1S)
    record.pack_voltage_v.value = 5.00f;
    flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_RANGE_VIOLATION, flags);
    TEST_ASSERT_FALSE(record.pack_voltage_v.ok);

    // Under-voltage (< 2.50V for 1S)
    record.pack_voltage_v.value = 2.10f;
    record.pack_voltage_v.ok = true;
    flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_RANGE_VIOLATION, flags);
    TEST_ASSERT_FALSE(record.pack_voltage_v.ok);

    // Over-temperature (> 55C)
    record.temps_c[0].value = 65.0f;
    record.temps_c[0].ok = true;
    flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_RANGE_VIOLATION, flags);
    TEST_ASSERT_FALSE(record.temps_c[0].ok);
}

void test_validator_nan_detection(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;

    record.pack_voltage_v = (SensorReadFloat){NAN, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.0f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};

    uint32_t flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_NAN_READING, flags);
    TEST_ASSERT_FALSE(record.pack_voltage_v.ok);
}

void test_validator_stale_data(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;
    record.pack_voltage_v = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.0f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};

    // Sample produced at uptime 1000 ms, current time is 5000 ms (> 3000 ms difference)
    record.uptime_ms = 1000;
    uint32_t current_time = 5000;

    uint32_t flags = validator.validateRecord(&record, current_time);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_STALE_DATA, flags);
}

void test_validator_ds18b20_disconnected_and_poweron_85c(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;
    record.pack_voltage_v = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.0f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};

    // Case 1: DS18B20 disconnected bus (-127.0 C)
    record.temps_c[0] = (SensorReadFloat){-127.0f, true, SENSOR_ERR_NONE};
    uint32_t flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_DS18B20_DISCONNECT, flags);
    TEST_ASSERT_FALSE(record.temps_c[0].ok);

    // Case 2: DS18B20 uninitialized power-on state (+85.0 C)
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;
    record.pack_voltage_v = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.0f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.7f, true, SENSOR_ERR_NONE};
    record.temps_c[0] = (SensorReadFloat){85.0f, true, SENSOR_ERR_NONE};

    flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_DS18B20_POWERON_85C, flags);
    TEST_ASSERT_FALSE(record.temps_c[0].ok);
}

void test_validator_ina226_conversion_ready(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.cell_count = 1;

    record.pack_voltage_v = (SensorReadFloat){0.0f, false, SENSOR_ERR_CONVERSION_NOT_READY};
    record.current_a = (SensorReadFloat){1.0f, true, SENSOR_ERR_NONE};

    uint32_t flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_INA226_NOT_READY, flags);
    TEST_ASSERT_FALSE(record.pack_voltage_v.ok);
}

void test_validator_cell_mismatches(void) {
    TelemetryValidator validator;
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);

    // Config expects 1 cell, report 4 cells
    record.cell_count = 4;
    record.pack_voltage_v = (SensorReadFloat){3.70f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){1.00f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.70f, true, SENSOR_ERR_NONE};

    uint32_t flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_CELL_COUNT_MISMATCH, flags);

    // Sum of cells vs pack voltage mismatch (|Vpack - Vcell| > 100 mV)
    record.cell_count = 1;
    record.pack_voltage_v = (SensorReadFloat){4.10f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[0] = (SensorReadFloat){3.70f, true, SENSOR_ERR_NONE}; // Delta = 400 mV > 100 mV

    flags = validator.validateRecord(&record, 1000);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_CELL_SUM_MISMATCH, flags);
    TEST_ASSERT_BITS_HIGH(QUALITY_FLAG_VOLTAGE_DIV_NOISE, flags);
}

// ============================================================================
// 2. BMS CRC & CHECKSUM TESTS
// ============================================================================

void test_bms_crc16_and_checksum(void) {
    // Test data buffer
    const uint8_t test_data[] = {0x03, 0x00, 0x00, 0x01};
    
    // Checksum calculation (JBD complement algorithm)
    uint16_t checksum = bmsCalculateChecksum(test_data, sizeof(test_data));
    uint32_t sum = 0x03 + 0x00 + 0x00 + 0x01;
    TEST_ASSERT_EQUAL_UINT16((uint16_t)((0x10000 - sum) & 0xFFFF), checksum);

    // CRC16 Modbus verification (non-zero deterministic value)
    uint16_t crc16 = bmsCalculateCrc16(test_data, sizeof(test_data));
    TEST_ASSERT_NOT_EQUAL(0, crc16);
}

void test_bms_frame_verification(void) {
    // Construct valid JBD frame: [0xDD, cmd=0xA5, status=0x00, len=0x02, d0=0x12, d1=0x34, ck_h, ck_l, 0x77]
    uint8_t payload[] = {0x00, 0x02, 0x12, 0x34};
    uint16_t ck = bmsCalculateChecksum(payload, sizeof(payload));

    uint8_t valid_frame[9] = {
        0xDD, 0xA5, 0x00, 0x02, 0x12, 0x34,
        (uint8_t)(ck >> 8), (uint8_t)(ck & 0xFF),
        0x77
    };
    TEST_ASSERT_TRUE(bmsVerifyFrame(valid_frame, sizeof(valid_frame)));

    // Corrupted checksum
    uint8_t corrupt_frame[9];
    memcpy(corrupt_frame, valid_frame, sizeof(valid_frame));
    corrupt_frame[6] ^= 0xFF; // Invert checksum byte
    TEST_ASSERT_FALSE(bmsVerifyFrame(corrupt_frame, sizeof(corrupt_frame)));

    // Truncated frame
    TEST_ASSERT_FALSE(bmsVerifyFrame(valid_frame, 5));
}

// ============================================================================
// 3. RECORD BUILDER (JSON SERIALIZATION) TESTS
// ============================================================================

void test_record_builder_valid_and_nulls(void) {
    TelemetryRecord_t record;
    memset(&record, 0, sizeof(record));

    record.seq = 42;
    strncpy(record.boot_id, "boot-deadbeef", sizeof(record.boot_id) - 1);
    strncpy(record.device_id, "ESP32-RIG-001", sizeof(record.device_id) - 1);
    strncpy(record.timestamp, "2026-10-07T12:00:00Z", sizeof(record.timestamp) - 1);
    strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
    record.uptime_ms = 42000;
    strncpy(record.ts_source, "boot_relative", sizeof(record.ts_source) - 1);
    strncpy(record.mode, "discharging", sizeof(record.mode) - 1);
    strncpy(record.calibration_version, "v1.0.0", sizeof(record.calibration_version) - 1);
    record.interlock_state = INTERLOCK_ARMED_ACTIVE;

    // Pack voltage valid, current INVALID (.ok = false)
    record.pack_voltage_v = (SensorReadFloat){3.725f, true, SENSOR_ERR_NONE};
    record.current_a = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};

    // Cell 0 valid, cells 1..3 invalid
    record.cell_voltages_v[0] = (SensorReadFloat){3.725f, true, SENSOR_ERR_NONE};
    record.cell_voltages_v[1] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};
    record.cell_voltages_v[2] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};
    record.cell_voltages_v[3] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};

    // Temperatures
    record.temps_c[0] = (SensorReadFloat){25.4f, true, SENSOR_ERR_NONE};
    record.temps_c[1] = (SensorReadFloat){26.1f, true, SENSOR_ERR_NONE};
    record.temps_c[2] = (SensorReadFloat){27.0f, true, SENSOR_ERR_NONE};
    record.temps_c[3] = (SensorReadFloat){24.0f, true, SENSOR_ERR_NONE};

    record.bms_soc_pct = (SensorReadFloat){85.0f, true, SENSOR_ERR_NONE};
    record.bms_soh_pct = (SensorReadFloat){98.0f, true, SENSOR_ERR_NONE};
    record.ah_discharged = (SensorReadFloat){0.5234f, true, SENSOR_ERR_NONE};
    record.quality_flags = QUALITY_FLAG_VALID;

    char json_buf[1024];
    size_t len = serializeRecordJson(&record, json_buf, sizeof(json_buf));
    TEST_ASSERT_TRUE(len > 0);

    // Verify key fields exist in output JSON
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"seq\": 42"));
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"boot_id\": \"boot-deadbeef\""));
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"battery_id\": \"RL-BAT-0001\""));
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"pack_voltage_v\": 3.725"));
    
    // Verify invalid sensor reading is serialized as null (NEVER fabricated 0)
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"current_a\": null"));
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"cell_voltages_v\": [3.725, null, null, null]"));

    // Verify quality flags array
    TEST_ASSERT_NOT_NULL(strstr(json_buf, "\"quality_flags\": [\"VALID\"]"));
}

void test_quality_flags_serialization(void) {
    char out_flags[256];

    // Case 1: Valid
    qualityFlagsToJsonArray(QUALITY_FLAG_VALID, out_flags, sizeof(out_flags));
    TEST_ASSERT_EQUAL_STRING("[\"VALID\"]", out_flags);

    // Case 2: Range violation + Stale
    uint32_t flags = QUALITY_FLAG_RANGE_VIOLATION | QUALITY_FLAG_STALE_DATA;
    qualityFlagsToJsonArray(flags, out_flags, sizeof(out_flags));
    TEST_ASSERT_NOT_NULL(strstr(out_flags, "\"RANGE_VIOLATION\""));
    TEST_ASSERT_NOT_NULL(strstr(out_flags, "\"STALE_DATA\""));
}

// ============================================================================
// 4. RING BUFFER & OVERFLOW POLICY TESTS
// ============================================================================

void test_ring_buffer_fifo_and_overflow(void) {
    TelemetryRingBuffer rb;
    TEST_ASSERT_TRUE(rb.isEmpty());
    TEST_ASSERT_FALSE(rb.isFull());
    TEST_ASSERT_EQUAL_UINT32(0, rb.size());

    // 1. Enqueue 5 records
    for (uint32_t i = 0; i < 5; i++) {
        TelemetryRecord_t r;
        memset(&r, 0, sizeof(r));
        r.seq = i;
        TEST_ASSERT_TRUE(rb.push(r));
    }
    TEST_ASSERT_EQUAL_UINT32(5, rb.size());
    TEST_ASSERT_FALSE(rb.hasOverflowed());

    // 2. Dequeue and verify FIFO ordering
    TelemetryRecord_t popped;
    TEST_ASSERT_TRUE(rb.pop(&popped));
    TEST_ASSERT_EQUAL_UINT32(0, popped.seq);
    TEST_ASSERT_EQUAL_UINT32(4, rb.size());

    // 3. Test Overflow Policy: fill to capacity (32 items) and push 10 more
    rb.clear();
    for (uint32_t i = 0; i < TELEMETRY_RING_BUFFER_CAPACITY + 10; i++) {
        TelemetryRecord_t r;
        memset(&r, 0, sizeof(r));
        r.seq = i;
        TEST_ASSERT_TRUE(rb.push(r)); // Never blocks!
    }

    // Size must remain at capacity (32)
    TEST_ASSERT_EQUAL_UINT32(TELEMETRY_RING_BUFFER_CAPACITY, rb.size());
    TEST_ASSERT_TRUE(rb.hasOverflowed());
    TEST_ASSERT_EQUAL_UINT32(10, rb.getOverflowCount());

    // Pop the oldest retained element: should be sequence 10 (0..9 were dropped)
    TEST_ASSERT_TRUE(rb.pop(&popped));
    TEST_ASSERT_EQUAL_UINT32(10, popped.seq);
}

void test_dump_10_records(void) {
    FILE* f = fopen("test_dump_10.jsonl", "w");
    if (!f) f = fopen("../test_dump_10.jsonl", "w");
    TEST_ASSERT_NOT_NULL(f);

    TelemetryValidator validator;
    const char boot_id[] = "boot-a1b2c3d4";

    for (uint32_t i = 0; i < 10; i++) {
        TelemetryRecord_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.seq = i;
        strncpy(rec.boot_id, boot_id, sizeof(rec.boot_id) - 1);
        strncpy(rec.device_id, "ESP32-RIG-001", sizeof(rec.device_id) - 1);
        strncpy(rec.battery_id, "RL-BAT-0001", sizeof(rec.battery_id) - 1);
        rec.uptime_ms = i * 1000;
        strncpy(rec.ts_source, "boot_relative", sizeof(rec.ts_source) - 1);
        strncpy(rec.mode, "discharging", sizeof(rec.mode) - 1);
        strncpy(rec.calibration_version, "v1.0.0", sizeof(rec.calibration_version) - 1);
        
        uint32_t total_sec = (rec.uptime_ms / 1000);
        uint32_t hours = (total_sec / 3600) % 24;
        uint32_t mins = (total_sec / 60) % 60;
        uint32_t secs = total_sec % 60;
        snprintf(rec.timestamp, sizeof(rec.timestamp), "2026-10-07T%02u:%02u:%02uZ", hours, mins, secs);

        rec.pack_voltage_v = (SensorReadFloat){3.720f, true, SENSOR_ERR_NONE};
        rec.current_a = (SensorReadFloat){1.000f, true, SENSOR_ERR_NONE};
        rec.cell_count = 1;
        rec.cell_voltages_v[0] = (SensorReadFloat){3.720f, true, SENSOR_ERR_NONE};
        rec.cell_voltages_v[1] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};
        rec.cell_voltages_v[2] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};
        rec.cell_voltages_v[3] = (SensorReadFloat){0.0f, false, SENSOR_ERR_DISCONNECTED};

        rec.temps_c[0] = (SensorReadFloat){25.4f + (float)i * 0.1f, true, SENSOR_ERR_NONE};
        rec.temps_c[1] = (SensorReadFloat){27.8f, true, SENSOR_ERR_NONE};
        rec.temps_c[2] = (SensorReadFloat){28.5f, true, SENSOR_ERR_NONE};
        rec.temps_c[3] = (SensorReadFloat){24.2f, true, SENSOR_ERR_NONE};

        rec.bms_soc_pct = (SensorReadFloat){85.0f - (float)i * 0.1f, true, SENSOR_ERR_NONE};
        rec.bms_soh_pct = (SensorReadFloat){98.0f, true, SENSOR_ERR_NONE};
        rec.ah_discharged = (SensorReadFloat){(float)i * 0.0003f, true, SENSOR_ERR_NONE};
        rec.interlock_state = INTERLOCK_ARMED_ACTIVE;

        rec.quality_flags = validator.validateRecord(&rec, rec.uptime_ms);

        char json_line[1024];
        size_t len = serializeRecordJson(&rec, json_line, sizeof(json_line));
        TEST_ASSERT_TRUE(len > 0);

        fprintf(f, "%s\n", json_line);
    }
    fclose(f);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // 1. Validator Tests
    RUN_TEST(test_validator_battery_id_format);
    RUN_TEST(test_validator_range_limits);
    RUN_TEST(test_validator_nan_detection);
    RUN_TEST(test_validator_stale_data);
    RUN_TEST(test_validator_ds18b20_disconnected_and_poweron_85c);
    RUN_TEST(test_validator_ina226_conversion_ready);
    RUN_TEST(test_validator_cell_mismatches);

    // 2. BMS CRC Tests
    RUN_TEST(test_bms_crc16_and_checksum);
    RUN_TEST(test_bms_frame_verification);

    // 3. Record Builder Tests
    RUN_TEST(test_record_builder_valid_and_nulls);
    RUN_TEST(test_quality_flags_serialization);

    // 4. Ring Buffer Tests
    RUN_TEST(test_ring_buffer_fifo_and_overflow);

    // 5. Serial Dump 10 Records Test
    RUN_TEST(test_dump_10_records);

    return UNITY_END();
}

