#pragma once
#include <cstdint>

namespace measurement_source {
struct Reading {
    float pm1;
    float pm25;
    float pm10;
    const char* source;
    int64_t receivedAtUs;
};

struct Config { int rx_gpio; };
enum class Status { Waiting, WarmingUp, Ready, Stale };

// Start once from app_main; UART is consumed independently of the network task.
bool start(const Config& config);
// Returns false during warmup, silence or failure; never synthesizes zero PM.
bool read(Reading& out);
Status status();
const char* statusName(Status status);
}
