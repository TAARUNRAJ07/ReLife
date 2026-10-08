#include "test_sequencer.h"

TestSequencer::TestSequencer() {}

bool TestSequencer::startCapacityTest(const char* battery_id, float cutoff_v, uint32_t max_duration_s) {
    (void)battery_id;
    (void)cutoff_v;
    (void)max_duration_s;
    return true;
}

bool TestSequencer::startPulseTest(const char* battery_id, float pulse_current_a, uint32_t pulse_duration_s) {
    (void)battery_id;
    (void)pulse_current_a;
    (void)pulse_duration_s;
    return true;
}

void TestSequencer::stopTest(const char* reason) {
    (void)reason;
}

void TestSequencer::tick(uint32_t delta_ms, const TelemetryRecord_t* current_sample) {
    (void)delta_ms;
    (void)current_sample;
}

TestState TestSequencer::getState() {
    return TEST_IDLE;
}

const char* TestSequencer::getActiveTestId() {
    return "";
}
