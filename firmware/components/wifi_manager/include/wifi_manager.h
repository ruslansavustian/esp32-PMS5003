#pragma once

namespace wifi_manager {
struct Config {
    const char* ssid;
    const char* password;
};
// Call once at startup. Credentials are copied into the driver during this call.
void start(const Config& config);
// True only after a local IPv4 address has been obtained.
bool isConnected();
}
