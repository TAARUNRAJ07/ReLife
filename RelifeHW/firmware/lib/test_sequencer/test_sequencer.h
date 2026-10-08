#pragma once
#include "relife_types.h"

typedef enum {
    TEST_IDLE = 0,
    TEST_RUNNING_CAPACITY = 1,
    TEST_RUNNING_PULSE = 2,
    TEST_COMPLETED = 3,
    TEST_ABORTED = 4
} TestState;

/**
 * @brief Automated Test Profile Sequencer
 * Manages test execution state machine (capacity discharge, internal resistance pulse).
 */
class TestSequencer {
public:
    TestSequencer();
    bool startCapacityTest(const char* battery_id, float cutoff_v, uint32_t max_duration_s);
    bool startPulseTest(const char* battery_id, float pulse_current_a, uint32_t pulse_duration_s);
    void stopTest(const char* reason);
    void tick(uint32_t delta_ms, const TelemetryRecord_t* current_sample);
    TestState getState();
    const char* getActiveTestId();
};
