#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "relife_types.h"

#define TELEMETRY_RING_BUFFER_CAPACITY 32

/**
 * @brief Thread-safe (or atomic lockless single-producer single-consumer) RAM Ring Buffer
 * Policy on overflow: Overwrite oldest record, retain newest N records in RAM.
 * Never blocks the producer (sampler).
 */
class TelemetryRingBuffer {
public:
    TelemetryRingBuffer() 
        : head_(0), tail_(0), count_(0), overflow_count_(0), has_overflowed_(false) {}

    bool push(const TelemetryRecord_t& record) {
        buffer_[head_] = record;
        head_ = (head_ + 1) % TELEMETRY_RING_BUFFER_CAPACITY;

        if (count_ < TELEMETRY_RING_BUFFER_CAPACITY) {
            count_++;
        } else {
            // Buffer full: overwrite oldest record by advancing tail
            tail_ = (tail_ + 1) % TELEMETRY_RING_BUFFER_CAPACITY;
            overflow_count_++;
            has_overflowed_ = true;
        }
        return true;
    }

    bool pop(TelemetryRecord_t* out_record) {
        if (isEmpty()) {
            return false;
        }
        if (out_record) {
            *out_record = buffer_[tail_];
        }
        tail_ = (tail_ + 1) % TELEMETRY_RING_BUFFER_CAPACITY;
        count_--;
        return true;
    }

    bool peek(TelemetryRecord_t* out_record) const {
        if (isEmpty()) return false;
        if (out_record) {
            *out_record = buffer_[tail_];
        }
        return true;
    }

    size_t size() const { return count_; }
    size_t capacity() const { return TELEMETRY_RING_BUFFER_CAPACITY; }
    bool isEmpty() const { return count_ == 0; }
    bool isFull() const { return count_ == TELEMETRY_RING_BUFFER_CAPACITY; }
    
    bool hasOverflowed() const { return has_overflowed_; }
    void clearOverflow() { has_overflowed_ = false; }
    uint32_t getOverflowCount() const { return overflow_count_; }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        has_overflowed_ = false;
    }

private:
    TelemetryRecord_t buffer_[TELEMETRY_RING_BUFFER_CAPACITY];
    size_t head_;
    size_t tail_;
    size_t count_;
    uint32_t overflow_count_;
    bool has_overflowed_;
};
