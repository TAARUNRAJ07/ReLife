#pragma once
#include "relife_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_FAULT_NONE = 0,
    SIM_FAULT_TEMP_DISCONNECTED,   // Returns -127.0 C (1-Wire open/disconnected)
    SIM_FAULT_TEMP_UNINIT_85C,     // Returns +85.0 C (DS18B20 power-on reset state)
    SIM_FAULT_INA226_NOT_READY,    // Conversion not ready / busy bit
    SIM_FAULT_VOLTAGE_OOR,         // Voltage out of range (e.g. 6.0V)
    SIM_FAULT_CURRENT_NAN,         // NaN current reading
    SIM_FAULT_CELL_SUM_MISMATCH,   // Pack voltage differs from cell sum by > 100 mV
    SIM_FAULT_CELL_COUNT_MISMATCH  // Reports mismatched cell count
} SimFaultPattern;

/**
 * Top-level C API functions returning {value, ok, error_code}
 */
SensorReadFloat readVoltage(void);
SensorReadFloat readCurrent(void);
SensorReadFloat readTemperature(uint8_t index);
SensorReadFloat readCellVoltage(uint8_t cell_index);

/**
 * Simulation fault injection control
 */
void setSimFault(SimFaultPattern fault);
SimFaultPattern getSimFault(void);

#ifdef __cplusplus
}
#endif

/**
 * C++ Sensor Manager Object
 */
class SensorManager {
public:
    SensorManager();
    bool init();
    SensorReadFloat readVoltage() { return ::readVoltage(); }
    SensorReadFloat readCurrent() { return ::readCurrent(); }
    SensorReadFloat readTemperature(uint8_t index) { return ::readTemperature(index); }
    SensorReadFloat readCellVoltage(uint8_t cell_index) { return ::readCellVoltage(cell_index); }
    
    // Backwards-compatible aliases
    SensorReadFloat readPackVoltage() { return readVoltage(); }
};
