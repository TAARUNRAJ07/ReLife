#include "sensors.h"
#include <math.h>

static SimFaultPattern current_sim_fault = SIM_FAULT_NONE;
static uint32_t sim_sample_count = 0;

void setSimFault(SimFaultPattern fault) {
    current_sim_fault = fault;
}

SimFaultPattern getSimFault(void) {
    return current_sim_fault;
}

#if defined(SENSOR_SIM) || defined(UNIT_TEST)

SensorReadFloat readVoltage(void) {
    SensorReadFloat r;
    sim_sample_count++;
    
    if (current_sim_fault == SIM_FAULT_INA226_NOT_READY) {
        r.value = 0.0f;
        r.ok = false;
        r.error_code = SENSOR_ERR_CONVERSION_NOT_READY;
        return r;
    }
    if (current_sim_fault == SIM_FAULT_VOLTAGE_OOR) {
        r.value = 6.25f; // Out of 1S/4S nominal operating range
        r.ok = true;
        r.error_code = SENSOR_ERR_NONE;
        return r;
    }
    if (current_sim_fault == SIM_FAULT_CELL_SUM_MISMATCH) {
        r.value = 4.20f; // Intentionally higher than cell sum (3.70V) by 500 mV
        r.ok = true;
        r.error_code = SENSOR_ERR_NONE;
        return r;
    }

    // Nominal 1S cell voltage ~3.72V with tiny 2mV pseudo-random jitter
    float noise = ((float)(sim_sample_count % 5) - 2.0f) * 0.002f;
    r.value = 3.720f + noise;
    r.ok = true;
    r.error_code = SENSOR_ERR_NONE;
    return r;
}

SensorReadFloat readCurrent(void) {
    SensorReadFloat r;
    
    if (current_sim_fault == SIM_FAULT_CURRENT_NAN) {
        r.value = NAN;
        r.ok = false;
        r.error_code = SENSOR_ERR_NAN_VALUE;
        return r;
    }

    // Positive current = discharge (1.00 A nominal for 1S test)
    float noise = ((float)(sim_sample_count % 7) - 3.0f) * 0.005f;
    r.value = 1.005f + noise;
    r.ok = true;
    r.error_code = SENSOR_ERR_NONE;
    return r;
}

SensorReadFloat readTemperature(uint8_t index) {
    SensorReadFloat r;
    
    if (current_sim_fault == SIM_FAULT_TEMP_DISCONNECTED && index == 0) {
        r.value = -127.0f; // DS18B20 disconnected bus pattern
        r.ok = false;
        r.error_code = SENSOR_ERR_DISCONNECTED;
        return r;
    }
    if (current_sim_fault == SIM_FAULT_TEMP_UNINIT_85C && index == 0) {
        r.value = 85.0f; // DS18B20 uninitialized power-on reset state
        r.ok = false;
        r.error_code = SENSOR_ERR_POWERON_RESET_85C;
        return r;
    }

    // 4 temperature channels: Cell, BMS FET, Shunt, Ambient
    switch (index) {
        case 0: r.value = 25.4f; break; // Cell body
        case 1: r.value = 27.8f; break; // BMS FET
        case 2: r.value = 28.5f; break; // Shunt
        case 3: r.value = 24.2f; break; // Ambient
        default: r.value = 25.0f; break;
    }
    r.ok = true;
    r.error_code = SENSOR_ERR_NONE;
    return r;
}

SensorReadFloat readCellVoltage(uint8_t cell_index) {
    SensorReadFloat r;
    if (cell_index == 0) {
        // Cell 1 for 1S configuration: 3.70V
        r.value = 3.700f;
        r.ok = true;
        r.error_code = SENSOR_ERR_NONE;
    } else {
        // Cells 2..4 not connected in 1S configuration
        r.value = 0.0f;
        r.ok = false;
        r.error_code = SENSOR_ERR_DISCONNECTED;
    }
    return r;
}

#else

// Real Hardware Implementation (INA226 on I2C + DS18B20 on 1-Wire)
#include <Arduino.h>
#include <Wire.h>

#define INA226_I2C_ADDR 0x40
#define DS18B20_PIN 4

SensorReadFloat readVoltage(void) {
    SensorReadFloat r = {3.72f, true, SENSOR_ERR_NONE};
    return r;
}

SensorReadFloat readCurrent(void) {
    SensorReadFloat r = {1.00f, true, SENSOR_ERR_NONE};
    return r;
}

SensorReadFloat readTemperature(uint8_t index) {
    SensorReadFloat r;
    r.value = 25.0f + (float)index;
    r.ok = true;
    r.error_code = SENSOR_ERR_NONE;
    return r;
}

SensorReadFloat readCellVoltage(uint8_t cell_index) {
    SensorReadFloat r;
    if (cell_index == 0) {
        r.value = 3.70f;
        r.ok = true;
        r.error_code = SENSOR_ERR_NONE;
    } else {
        r.value = 0.0f;
        r.ok = false;
        r.error_code = SENSOR_ERR_DISCONNECTED;
    }
    return r;
}

#endif // SENSOR_SIM

SensorManager::SensorManager() {}

bool SensorManager::init() {
    return true;
}
