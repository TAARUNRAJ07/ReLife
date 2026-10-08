#include "timekeeper.h"
#include <string.h>
#include <stdio.h>

Timekeeper::Timekeeper() {}

bool Timekeeper::init() {
    return true;
}

bool Timekeeper::syncNtp(const char* ntp_server, uint32_t timeout_ms) {
    (void)ntp_server;
    (void)timeout_ms;
    return true;
}

bool Timekeeper::isSynced() {
    return false;
}

bool Timekeeper::getIso8601Utc(char* buffer, size_t max_len) {
    if (!buffer || max_len < 25) return false;
    snprintf(buffer, max_len, "2026-10-07T00:00:00.000Z");
    return true;
}

uint64_t Timekeeper::getEpochMs() {
    return 0;
}
