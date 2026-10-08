#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Wi-Fi Connection Manager & TLS Transport
 * Handles network connection lifecycle, reconnection with exponential backoff and jitter.
 */
class WifiClientManager {
public:
    WifiClientManager();
    bool init();
    bool connect();
    bool isConnected();
    int8_t getRssi();
    void disconnect();
};
