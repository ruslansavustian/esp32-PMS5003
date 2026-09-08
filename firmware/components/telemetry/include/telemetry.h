#pragma once

namespace telemetry {
struct Config {
    const char* endpoint;
    const char* deviceId;
    const char* deviceToken;
    unsigned intervalSeconds;
    bool allowInsecureHttp;
};
// Call once. Strings must remain valid and unchanged for the application's lifetime.
// The Config itself is copied. Invalid configuration leaves telemetry disabled.
void start(const Config& config);
}
