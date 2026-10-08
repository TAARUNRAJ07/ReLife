#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "relife_types.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Serializes a TelemetryRecord_t into a single JSONL line string matching
 * contracts/v1/telemetry.schema.json without any heap allocation or Arduino String.
 * Invalid sensor values (.ok == false) are rendered as 'null'.
 */
size_t serializeRecordJson(const TelemetryRecord_t* record, char* out_buf, size_t max_len);

/**
 * Converts quality flag bitmasks to a valid JSON string array representation.
 */
void qualityFlagsToJsonArray(uint32_t flags, char* out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

/**
 * SD Card Telemetry Journal with Write-Ahead Indexing & RAM Ring Buffer
 */
class TelemetryJournal {
public:
    TelemetryJournal();
    bool init(const char* mount_path = "/sd");
    
    // Non-blocking handoff: pushes to internal ring buffer
    bool enqueueRecord(const TelemetryRecord_t* record);
    
    // Process queued records and flush to SD storage
    bool processQueue();
    
    // Direct append to non-volatile SD storage (returns false on SD failure)
    bool appendRecord(const TelemetryRecord_t* record);
    
    // SD and Buffer Status
    bool isSdCardHealthy() const { return sd_healthy_; }
    bool hasBufferOverflowed() const { return ring_buffer_.hasOverflowed(); }
    void clearBufferOverflow() { ring_buffer_.clearOverflow(); }
    size_t getQueueSize() const { return ring_buffer_.size(); }
    uint32_t getDroppedCount() const { return ring_buffer_.getOverflowCount(); }
    
    // Query / Ack API
    size_t readUnacknowledgedBatch(TelemetryRecord_t* out_records, size_t max_count);
    bool pruneAcknowledged(uint32_t ack_up_to_seq);
    size_t getPendingCount();
    uint64_t getFreeBytes();

    TelemetryRingBuffer& getRingBuffer() { return ring_buffer_; }

private:
    char mount_path_[32];
    bool sd_healthy_;
    uint32_t write_ahead_seq_;
    uint32_t last_flushed_seq_;
    TelemetryRingBuffer ring_buffer_;
};
