#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief SNTP Client & ISO-8601 UTC Timestamp Synchronizer
 * Adheres to Rule 6: SNTP synchronization must be valid before initiating TLS telemetry requests.
 */
class Timekeeper {
public:
    Timekeeper();
    bool init();
    bool syncNtp(const char* ntp_server, uint32_t timeout_ms);
    bool isSynced();
    bool getIso8601Utc(char* buffer, size_t max_len);
    uint64_t getEpochMs();
};
