#include "delivery.h"

TelemetryDelivery::TelemetryDelivery() {}

bool TelemetryDelivery::init(const char* host, uint16_t port, const char* device_id, const char* device_key) {
    (void)host;
    (void)port;
    (void)device_id;
    (void)device_key;
    return true;
}

bool TelemetryDelivery::sendBatch(const TelemetryRecord_t* records, size_t count, uint32_t* out_ack_seq) {
    (void)records;
    (void)count;
    if (out_ack_seq) *out_ack_seq = 0;
    return true;
}

bool TelemetryDelivery::sendHeartbeat(bool interlock_ok, const char* active_test_id) {
    (void)interlock_ok;
    (void)active_test_id;
    return true;
}

bool TelemetryDelivery::pollCommands() {
    return true;
}
