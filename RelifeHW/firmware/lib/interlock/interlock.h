#pragma once
#include "relife_types.h"

/**
 * @brief Safety Interlock Decision Engine (Rule 1)
 * All power actuators (relays, load switches) are strictly governed by this local function.
 * Evaluates cell voltages, temperatures, overcurrent, e-stop, sensor health, and test state.
 */
class SafetyInterlock {
public:
    SafetyInterlock();
    bool init(uint8_t relay_pin, uint8_t estop_sense_pin);
    
    /**
     * @brief The ONLY function authorized to drive power actuation output pins.
     * Evaluates local safety envelope. If any trip condition occurs, power is immediately cut.
     */
    InterlockState evaluateAndDrive(const TelemetryRecord_t* record, bool test_active);
    
    InterlockState getInterlockState();
    bool isArmed();
    void emergencyTrip(InterlockState reason);
    void resetInterlock();
};
