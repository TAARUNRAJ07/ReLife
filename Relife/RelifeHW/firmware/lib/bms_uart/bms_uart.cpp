#include "bms_uart.h"
#include <string.h>

static BmsSimFault_t current_bms_fault = BMS_SIM_FAULT_NONE;

void setBmsSimFault(BmsSimFault_t fault) {
    current_bms_fault = fault;
}

BmsSimFault_t getBmsSimFault(void) {
    return current_bms_fault;
}

uint16_t bmsCalculateCrc16(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint16_t bmsCalculateChecksum(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint16_t)((0x10000 - (sum & 0xFFFF)) & 0xFFFF);
}

bool bmsVerifyFrame(const uint8_t* frame, size_t len) {
    // Minimum JBD frame: [0xDD, cmd, status, data_len, ck_h, ck_l, 0x77] = 7 bytes
    if (!frame || len < 7) return false;
    if (frame[0] != 0xDD || frame[len - 1] != 0x77) return false;
    
    uint8_t data_len = frame[3];
    if (len != (size_t)(data_len + 7)) return false;

    // Checksum is calculated over status, length, and payload (bytes 2 to 3 + data_len)
    uint16_t expected_ck = (uint16_t)((frame[len - 3] << 8) | frame[len - 2]);
    uint16_t calculated_ck = bmsCalculateChecksum(&frame[2], data_len + 2);

    return (expected_ck == calculated_ck);
}

SensorReadBmsStatus readBMSStatus(void) {
    SensorReadBmsStatus result;
    memset(&result, 0, sizeof(result));

    if (current_bms_fault == BMS_SIM_FAULT_CRC) {
        result.ok = false;
        result.error_code = SENSOR_ERR_CRC_MISMATCH;
        return result;
    }

    if (current_bms_fault == BMS_SIM_FAULT_TIMEOUT) {
        result.ok = false;
        result.error_code = SENSOR_ERR_TIMEOUT;
        return result;
    }

    // Default valid simulated BMS telemetry for 1S 3.7V cell
    result.ok = true;
    result.error_code = SENSOR_ERR_NONE;
    result.value.pack_voltage_v = 3.700f;
    result.value.current_a = 1.000f; // 1.0A discharge
    result.value.cell_voltages_v[0] = 3.700f;
    result.value.cell_voltages_v[1] = 0.0f;
    result.value.cell_voltages_v[2] = 0.0f;
    result.value.cell_voltages_v[3] = 0.0f;
    result.value.cell_count = (current_bms_fault == BMS_SIM_FAULT_CELL_MISMATCH) ? 4 : 1;
    result.value.soc_pct = 85.0f;
    result.value.soh_pct = 98.0f;
    result.value.bms_temp_c = 27.5f;
    result.value.alarm_flags = 0;

    return result;
}

BmsUart::BmsUart() {}

bool BmsUart::init(uint32_t baud_rate) {
    (void)baud_rate;
    return true;
}

bool BmsUart::poll() {
    SensorReadBmsStatus s = readBMSStatus();
    return s.ok;
}

SensorReadFloat BmsUart::getSoc() {
    SensorReadBmsStatus s = readBMSStatus();
    SensorReadFloat r = {s.value.soc_pct, s.ok, s.error_code};
    return r;
}

SensorReadFloat BmsUart::getSoh() {
    SensorReadBmsStatus s = readBMSStatus();
    SensorReadFloat r = {s.value.soh_pct, s.ok, s.error_code};
    return r;
}

SensorReadFloat BmsUart::getBmsTemperature() {
    SensorReadBmsStatus s = readBMSStatus();
    SensorReadFloat r = {s.value.bms_temp_c, s.ok, s.error_code};
    return r;
}

bool BmsUart::isConnected() {
    SensorReadBmsStatus s = readBMSStatus();
    return s.ok;
}
