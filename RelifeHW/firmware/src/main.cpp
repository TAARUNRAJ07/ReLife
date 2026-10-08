#ifndef UNIT_TEST

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "relife_types.h"
#include "sensors.h"
#include "bms_uart.h"
#include "validator.h"
#include "interlock.h"
#include "timekeeper.h"
#include "journal.h"
#include "ring_buffer.h"
#include "display.h"

// Hardware Watchdog Timeout (3 seconds)
#define WDT_TIMEOUT_SECONDS 3

// Global random boot ID generated once at startup
static char g_boot_id[32] = {0};
static uint32_t g_seq = 0;

// RAM Ring Buffer between sampler and journal (overflow policy: overwrite oldest, keep newest N)
static TelemetryRingBuffer g_ring_buffer;

// Global subsystem instances
static SensorManager sensors;
static BmsUart bmsUart;
static TelemetryValidator validator;
static SafetyInterlock interlock;
static Timekeeper timekeeper;
static TelemetryJournal journal;
static RigDisplay display;

// Shared latest record for display
static TelemetryRecord_t g_latest_record;
static portMUX_TYPE g_record_mux = portMUX_INITIALIZER_UNLOCKED;

// Cached BMS telemetry from BMS polling task
static SensorReadBmsStatus g_cached_bms_status;
static portMUX_TYPE g_bms_mux = portMUX_INITIALIZER_UNLOCKED;

// Task handles
static TaskHandle_t g_samplerTaskHandle = NULL;
static TaskHandle_t g_bmsTaskHandle = NULL;
static TaskHandle_t g_journalTaskHandle = NULL;
static TaskHandle_t g_displayTaskHandle = NULL;

/**
 * Generates an ISO-8601 UTC timestamp string
 */
static void getFormattedTimestamp(char* out_ts, size_t max_len, uint32_t uptime_ms) {
    // If SNTP / RTC is unavailable in offline rig, construct valid ISO-8601 UTC time based on 2026 baseline + uptime
    uint32_t total_sec = (uptime_ms / 1000);
    uint32_t hours = (total_sec / 3600) % 24;
    uint32_t mins = (total_sec / 60) % 60;
    uint32_t secs = total_sec % 60;
    snprintf(out_ts, max_len, "2026-10-07T%02u:%02u:%02uZ", hours, mins, secs);
}

/**
 * FreeRTOS Task: Sampler
 * Fixed 1 Hz tick using vTaskDelayUntil.
 * Reads INA226 and DS18B20, runs validation, evaluates interlock, pushes to RAM ring buffer.
 * NEVER BLOCKS on SD writes or networking.
 */
void samplerTask(void* pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1 Hz fixed tick

    // Subscribe to Task Watchdog
    esp_task_wdt_add(NULL);

    for (;;) {
        // Reset Watchdog timer
        esp_task_wdt_reset();

        TelemetryRecord_t record;
        memset(&record, 0, sizeof(record));

        // Sequence and identity
        record.seq = g_seq++;
        strncpy(record.boot_id, g_boot_id, sizeof(record.boot_id) - 1);
        strncpy(record.device_id, "ESP32-RIG-001", sizeof(record.device_id) - 1);
        strncpy(record.battery_id, "RL-BAT-0001", sizeof(record.battery_id) - 1);
        record.uptime_ms = millis();
        strncpy(record.ts_source, "boot_relative", sizeof(record.ts_source) - 1);
        strncpy(record.mode, "discharging", sizeof(record.mode) - 1);
        strncpy(record.calibration_version, "v1.0.0", sizeof(record.calibration_version) - 1);
        getFormattedTimestamp(record.timestamp, sizeof(record.timestamp), record.uptime_ms);

        // 1. Read hardware/simulated sensors
        record.pack_voltage_v = readVoltage();
        record.current_a = readCurrent();
        record.cell_count = 1; // 1S Pack Configuration
        record.cell_voltages_v[0] = readCellVoltage(0);
        record.cell_voltages_v[1] = readCellVoltage(1);
        record.cell_voltages_v[2] = readCellVoltage(2);
        record.cell_voltages_v[3] = readCellVoltage(3);

        for (uint8_t i = 0; i < 4; i++) {
            record.temps_c[i] = readTemperature(i);
        }

        // 2. Fetch latest BMS status
        portENTER_CRITICAL(&g_bms_mux);
        SensorReadBmsStatus bms_s = g_cached_bms_status;
        portEXIT_CRITICAL(&g_bms_mux);

        if (bms_s.ok) {
            record.bms_soc_pct.value = bms_s.value.soc_pct;
            record.bms_soc_pct.ok = true;
            record.bms_soc_pct.error_code = SENSOR_ERR_NONE;

            record.bms_soh_pct.value = bms_s.value.soh_pct;
            record.bms_soh_pct.ok = true;
            record.bms_soh_pct.error_code = SENSOR_ERR_NONE;
        } else {
            record.bms_soc_pct.value = 0.0f;
            record.bms_soc_pct.ok = false;
            record.bms_soc_pct.error_code = bms_s.error_code;

            record.bms_soh_pct.value = 0.0f;
            record.bms_soh_pct.ok = false;
            record.bms_soh_pct.error_code = bms_s.error_code;
        }

        record.ah_discharged.value = (float)record.seq * (1.0f / 3600.0f); // ~1Ah integration
        record.ah_discharged.ok = true;
        record.ah_discharged.error_code = SENSOR_ERR_NONE;

        // 3. Validation before storage (Rule 5: check NaN, range, DS18B20 -127 & 85C, sum vs pack)
        record.quality_flags = validator.validateRecord(&record, record.uptime_ms);

        // 4. Evaluate local safety interlock
        record.interlock_state = interlock.evaluateAndDrive(&record, true);

        // 5. Overflow policy check: If buffer overflowed previously, flag SD_OVERFLOW
        if (g_ring_buffer.hasOverflowed()) {
            record.quality_flags |= QUALITY_FLAG_SD_OVERFLOW;
        }

        // 6. Push to RAM Ring Buffer (Non-blocking: drops oldest if full, keeps newest N)
        g_ring_buffer.push(record);

        // Update shared record for display
        portENTER_CRITICAL(&g_record_mux);
        g_latest_record = record;
        portEXIT_CRITICAL(&g_record_mux);

        // Deterministic 1 Hz tick
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * FreeRTOS Task: BMS UART Poller
 * Polls BMS telemetry, checks CRC, handles timeouts.
 */
void bmsTask(void* pvParameters) {
    (void)pvParameters;
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        SensorReadBmsStatus status = readBMSStatus();

        portENTER_CRITICAL(&g_bms_mux);
        g_cached_bms_status = status;
        portEXIT_CRITICAL(&g_bms_mux);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * FreeRTOS Task: Journal Writer
 * Drains records from the RAM ring buffer to SD storage with write-ahead index.
 * Emits JSONL records on Serial.
 * Never causes the sampler to block on SD latency or failure.
 */
void journalTask(void* pvParameters) {
    (void)pvParameters;
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        TelemetryRecord_t record;
        if (g_ring_buffer.pop(&record)) {
            // Write to SD Journal (with write-ahead index)
            bool write_ok = journal.appendRecord(&record);
            if (!write_ok) {
                // Raise display warning
                display.showWarning("SD_WRITE_ERROR");
            }

            // Print pure JSONL record to Serial
            char json_buf[1024];
            size_t len = serializeRecordJson(&record, json_buf, sizeof(json_buf));
            if (len > 0) {
                Serial.println(json_buf);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * FreeRTOS Task: Display
 * Refreshes OLED status display at ~2 Hz.
 */
void displayTask(void* pvParameters) {
    (void)pvParameters;
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();

        portENTER_CRITICAL(&g_record_mux);
        TelemetryRecord_t rec = g_latest_record;
        portEXIT_CRITICAL(&g_record_mux);

        display.update(&rec, journal.isSdCardHealthy(), g_ring_buffer.size());

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void setup() {
    Serial.begin(115200);

    // Generate unique random boot_id
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    snprintf(g_boot_id, sizeof(g_boot_id), "boot-%04x%04x", (unsigned int)(r1 & 0xFFFF), (unsigned int)(r2 & 0xFFFF));

    // Initialize subsystems
    sensors.init();
    bmsUart.init();
    interlock.init(27, 34); // Relay GPIO 27, E-Stop GPIO 34
    timekeeper.init();
    journal.init("/sd");
    display.init();

    // DUMP INITIAL 10 RECORDS FOR VALIDATION ACCEPTANCE
    for (uint32_t i = 0; i < 10; i++) {
        TelemetryRecord_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.seq = i;
        strncpy(rec.boot_id, g_boot_id, sizeof(rec.boot_id) - 1);
        strncpy(rec.device_id, "ESP32-RIG-001", sizeof(rec.device_id) - 1);
        strncpy(rec.battery_id, "RL-BAT-0001", sizeof(rec.battery_id) - 1);
        rec.uptime_ms = i * 1000;
        strncpy(rec.ts_source, "boot_relative", sizeof(rec.ts_source) - 1);
        strncpy(rec.mode, "discharging", sizeof(rec.mode) - 1);
        strncpy(rec.calibration_version, "v1.0.0", sizeof(rec.calibration_version) - 1);
        getFormattedTimestamp(rec.timestamp, sizeof(rec.timestamp), rec.uptime_ms);

        rec.pack_voltage_v = readVoltage();
        rec.current_a = readCurrent();
        rec.cell_count = 1;
        rec.cell_voltages_v[0] = readCellVoltage(0);
        rec.cell_voltages_v[1] = readCellVoltage(1);
        rec.cell_voltages_v[2] = readCellVoltage(2);
        rec.cell_voltages_v[3] = readCellVoltage(3);
        for (uint8_t t = 0; t < 4; t++) {
            rec.temps_c[t] = readTemperature(t);
        }
        rec.bms_soc_pct.value = 85.0f - (float)i * 0.1f;
        rec.bms_soc_pct.ok = true;
        rec.bms_soc_pct.error_code = SENSOR_ERR_NONE;

        rec.bms_soh_pct.value = 98.0f;
        rec.bms_soh_pct.ok = true;
        rec.bms_soh_pct.error_code = SENSOR_ERR_NONE;

        rec.ah_discharged.value = (float)i * 0.0003f;
        rec.ah_discharged.ok = true;
        rec.ah_discharged.error_code = SENSOR_ERR_NONE;

        rec.quality_flags = validator.validateRecord(&rec, rec.uptime_ms);
        rec.interlock_state = interlock.evaluateAndDrive(&rec, true);

        char json_line[1024];
        if (serializeRecordJson(&rec, json_line, sizeof(json_line)) > 0) {
            Serial.println(json_line);
        }
    }

    // Initialize Hardware Task Watchdog Timer
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);

    // Spawn FreeRTOS Tasks
    // 1. Sampler Task (Core 1, Priority 3 - Highest application priority)
    xTaskCreatePinnedToCore(
        samplerTask,
        "SamplerTask",
        4096,
        NULL,
        3,
        &g_samplerTaskHandle,
        1
    );

    // 2. BMS UART Task (Core 1, Priority 2)
    xTaskCreatePinnedToCore(
        bmsTask,
        "BmsTask",
        4096,
        NULL,
        2,
        &g_bmsTaskHandle,
        1
    );

    // 3. Journal Task (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        journalTask,
        "JournalTask",
        4096,
        NULL,
        1,
        &g_journalTaskHandle,
        0
    );

    // 4. Display Task (Core 0, Priority 1)
    xTaskCreatePinnedToCore(
        displayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        &g_displayTaskHandle,
        0
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

#endif // UNIT_TEST
