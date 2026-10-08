#include "command_handler.h"
#include <string.h>

CommandHandler::CommandHandler() {}

bool CommandHandler::processCommandJson(const char* json_str, char* out_ack_status, char* out_error_msg, size_t max_err_len) {
    (void)json_str;
    if (out_ack_status) strcpy(out_ack_status, "EXECUTED");
    if (out_error_msg && max_err_len > 0) out_error_msg[0] = '\0';
    return true;
}
