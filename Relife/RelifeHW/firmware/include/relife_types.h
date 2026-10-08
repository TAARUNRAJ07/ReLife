#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Standard Sensor Reading Pattern (Rule 5)
 * Every sensor read returns {value, ok, error_code}.
 * Invalid readings must never be fabricated or set to 0 without quality flags.
 */
typedef struct {
    float value;
    bool ok;
    uint32_t error_code;
} SensorReadFloat;

typedef struct {
    int32_t value;
    bool ok;
    uint32_t error_code;
} SensorReadInt;

/**
 * Sensor Error Codes
 */
#define SENSOR_ERR_NONE                  0
#define SENSOR_ERR_DISCONNECTED          1
#define SENSOR_ERR_CONVERSION_NOT_READY  2
#define SENSOR_ERR_OUT_OF_RANGE          3
#define SENSOR_ERR_NAN_VALUE             4
#define SENSOR_ERR_STALE                 5
#define SENSOR_ERR_CRC_MISMATCH          6
#define SENSOR_ERR_TIMEOUT               7
#define SENSOR_ERR_POWERON_RESET_85C     8
#define SENSOR_ERR_CELL_SUM_MISMATCH     9
#define SENSOR_ERR_CELL_COUNT_MISMATCH   10

/**
 * BMS Parsed Status Structure
 */
typedef struct {
    float pack_voltage_v;
    float current_a;              // Positive = discharge, negative = charge
    float cell_voltages_v[4];
    uint8_t cell_count;
    float soc_pct;
    float soh_pct;
    float bms_temp_c;
    uint32_t alarm_flags;
} BmsStatusData_t;

typedef struct {
    BmsStatusData_t value;
    bool ok;
    uint32_t error_code;
} SensorReadBmsStatus;

/**
 * Quality Flag Bitmasks (Section 4 & Validation Rules)
 */
#define QUALITY_FLAG_VALID                 0x00000000
#define QUALITY_FLAG_SENSOR_FAULT          0x00000001
#define QUALITY_FLAG_CLOCK_UNSYNCED        0x00000002
#define QUALITY_FLAG_VOLTAGE_DIV_NOISE     0x00000004
#define QUALITY_FLAG_ADC_SATURATED         0x00000008
#define QUALITY_FLAG_BMS_TIMEOUT           0x00000010
#define QUALITY_FLAG_INTERLOCK_TRIP        0x00000020
#define QUALITY_FLAG_RANGE_VIOLATION       0x00000040
#define QUALITY_FLAG_NAN_READING           0x00000080
#define QUALITY_FLAG_STALE_DATA            0x00000100
#define QUALITY_FLAG_DS18B20_DISCONNECT    0x00000200  // -127 C
#define QUALITY_FLAG_DS18B20_POWERON_85C   0x00000400  // 85.0 C uninitialized reset
#define QUALITY_FLAG_INA226_NOT_READY      0x00000800
#define QUALITY_FLAG_BMS_CRC_FAIL          0x00001000
#define QUALITY_FLAG_CELL_COUNT_MISMATCH   0x00002000
#define QUALITY_FLAG_CELL_SUM_MISMATCH     0x00004000  // |Vpack - sum(Vcell)| > 100 mV
#define QUALITY_FLAG_SD_OVERFLOW           0x00008000

/**
 * Interlock States
 */
typedef enum {
    INTERLOCK_SAFE_DISARMED = 0,
    INTERLOCK_ARMED_ACTIVE = 1,
    INTERLOCK_TRIP_OVERVOLTAGE = 2,
    INTERLOCK_TRIP_UNDERVOLTAGE = 3,
    INTERLOCK_TRIP_OVERTEMP = 4,
    INTERLOCK_TRIP_OVERCURRENT = 5,
    INTERLOCK_TRIP_ESTOP = 6,
    INTERLOCK_TRIP_SENSOR_FAULT = 7
} InterlockState;

/**
 * Operating Modes
 */
typedef enum {
    MODE_IDLE = 0,
    MODE_CHARGING,
    MODE_DISCHARGING,
    MODE_CAPACITY_TEST,
    MODE_PULSE_TEST,
    MODE_ERROR_ABORT
} OperatingMode;

/**
 * Timestamp Synchronization Source
 */
typedef enum {
    TS_SOURCE_SNTP = 0,
    TS_SOURCE_RTC,
    TS_SOURCE_BOOT_RELATIVE,
    TS_SOURCE_SIMULATED
} TimestampSource;

/**
 * Full Telemetry Record matching contracts/v1/telemetry.schema.json
 */
typedef struct {
    uint32_t seq;
    char boot_id[32];              // Random alphanumeric string per boot
    char device_id[32];            // Rig device ID e.g. "ESP32-RIG-001"
    char timestamp[32];            // ISO-8601 UTC string
    char battery_id[16];           // ^RL-BAT-[0-9]{4}$
    uint32_t uptime_ms;
    char ts_source[16];            // "sntp", "rtc", "boot_relative", "simulated"
    char mode[16];                 // "idle", "charging", "discharging", etc.
    SensorReadFloat pack_voltage_v;
    SensorReadFloat current_a;     // Positive = discharge, negative = charge
    SensorReadFloat cell_voltages_v[4];
    uint8_t cell_count;            // 1 for 1S, 4 for 4S
    SensorReadFloat temps_c[4];    // Cell avg, BMS FET, Shunt, Ambient
    SensorReadFloat bms_soc_pct;
    SensorReadFloat bms_soh_pct;
    SensorReadFloat ah_discharged;
    InterlockState interlock_state;
    uint32_t quality_flags;
    char calibration_version[12];  // e.g. "v1.0.0"
} TelemetryRecord_t;

/**
 * String conversion helpers
 */
static inline const char* interlockStateToString(InterlockState state) {
    switch (state) {
        case INTERLOCK_SAFE_DISARMED: return "SAFE_DISARMED";
        case INTERLOCK_ARMED_ACTIVE: return "ARMED_ACTIVE";
        case INTERLOCK_TRIP_OVERVOLTAGE: return "TRIPPED_OVERVOLTAGE";
        case INTERLOCK_TRIP_UNDERVOLTAGE: return "TRIPPED_UNDERVOLTAGE";
        case INTERLOCK_TRIP_OVERTEMP: return "TRIPPED_OVERTEMP";
        case INTERLOCK_TRIP_OVERCURRENT: return "TRIPPED_OVERCURRENT";
        case INTERLOCK_TRIP_ESTOP: return "TRIPPED_ESTOP";
        case INTERLOCK_TRIP_SENSOR_FAULT: return "TRIPPED_SENSOR_FAULT";
        default: return "SAFE_DISARMED";
    }
}

static inline const char* modeToString(OperatingMode mode) {
    switch (mode) {
        case MODE_IDLE: return "idle";
        case MODE_CHARGING: return "charging";
        case MODE_DISCHARGING: return "discharging";
        case MODE_CAPACITY_TEST: return "capacity_test";
        case MODE_PULSE_TEST: return "pulse_test";
        case MODE_ERROR_ABORT: return "error_abort";
        default: return "idle";
    }
}

static inline const char* tsSourceToString(TimestampSource src) {
    switch (src) {
        case TS_SOURCE_SNTP: return "sntp";
        case TS_SOURCE_RTC: return "rtc";
        case TS_SOURCE_BOOT_RELATIVE: return "boot_relative";
        case TS_SOURCE_SIMULATED: return "simulated";
        default: return "boot_relative";
    }
}

#ifdef __cplusplus
}
#endif

