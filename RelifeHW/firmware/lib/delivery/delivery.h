#pragma once
#include "relife_types.h"

/**
 * @brief Telemetry Batch Delivery Client
 * Formats JSON payloads according to contracts/v1 and delivers them via HTTPS POST.
 * Parses server ACK and updates journal.
 */
class TelemetryDelivery {
public:
    TelemetryDelivery();
    bool init(const char* host, uint16_t port, const char* device_id, const char* device_key);
    bool sendBatch(const TelemetryRecord_t* records, size_t count, uint32_t* out_ack_seq);
    bool sendHeartbeat(bool interlock_ok, const char* active_test_id);
    bool pollCommands();
};
