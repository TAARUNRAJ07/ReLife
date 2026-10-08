#include "interlock.h"

SafetyInterlock::SafetyInterlock() {}

bool SafetyInterlock::init(uint8_t relay_pin, uint8_t estop_sense_pin) {
    (void)relay_pin;
    (void)estop_sense_pin;
    return true;
}

InterlockState SafetyInterlock::evaluateAndDrive(const TelemetryRecord_t* record, bool test_active) {
    (void)record;
    (void)test_active;
    return INTERLOCK_SAFE_DISARMED;
}

InterlockState SafetyInterlock::getInterlockState() {
    return INTERLOCK_SAFE_DISARMED;
}

bool SafetyInterlock::isArmed() {
    return false;
}

void SafetyInterlock::emergencyTrip(InterlockState reason) {
    (void)reason;
}

void SafetyInterlock::resetInterlock() {}
