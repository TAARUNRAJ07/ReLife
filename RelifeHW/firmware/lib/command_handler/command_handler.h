#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Remote Command Handler
 * Parses backend commands (START_LOG, START_TEST, STOP_TEST, REQUEST_STATUS).
 * NOTE (Rule 1): Commands never directly energize power actuators or bypass safety interlocks.
 */
class CommandHandler {
public:
    CommandHandler();
    bool processCommandJson(const char* json_str, char* out_ack_status, char* out_error_msg, size_t max_err_len);
};
