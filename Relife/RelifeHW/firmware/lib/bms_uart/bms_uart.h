#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "relife_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BMS_SIM_FAULT_NONE = 0,
    BMS_SIM_FAULT_CRC,
    BMS_SIM_FAULT_TIMEOUT,
    BMS_SIM_FAULT_CELL_MISMATCH
} BmsSimFault_t;

/**
 * CRC and Checksum Calculation Functions (Native Testable)
 */
uint16_t bmsCalculateCrc16(const uint8_t* data, size_t len);
uint16_t bmsCalculateChecksum(const uint8_t* data, size_t len);
bool bmsVerifyFrame(const uint8_t* frame, size_t len);

/**
 * Top-level C API function returning {value, ok, error_code}
 */
SensorReadBmsStatus readBMSStatus(void);

/**
 * Simulation and Fault Injection Controls
 */
void setBmsSimFault(BmsSimFault_t fault);
BmsSimFault_t getBmsSimFault(void);

#ifdef __cplusplus
}
#endif

class BmsUart {
public:
    BmsUart();
    bool init(uint32_t baud_rate = 9600);
    bool poll();
    SensorReadBmsStatus readStatus() { return ::readBMSStatus(); }
    SensorReadFloat getSoc();
    SensorReadFloat getSoh();
    SensorReadFloat getBmsTemperature();
    bool isConnected();
};
