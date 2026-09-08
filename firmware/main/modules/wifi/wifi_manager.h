#pragma once

namespace wifi_manager {
// Call once at startup. Connection continues through ESP-IDF events.
void start();
// True only after a local IPv4 address has been obtained.
bool isConnected();
}
