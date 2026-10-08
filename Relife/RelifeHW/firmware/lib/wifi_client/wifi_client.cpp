#include "wifi_client.h"

WifiClientManager::WifiClientManager() {}

bool WifiClientManager::init() {
    return true;
}

bool WifiClientManager::connect() {
    return true;
}

bool WifiClientManager::isConnected() {
    return false;
}

int8_t WifiClientManager::getRssi() {
    return -60;
}

void WifiClientManager::disconnect() {}
