#pragma once

namespace measurement_source {
struct Reading {
    float pm1;
    float pm25;
    float pm10;
    const char* source;
};

// Replace the mock implementation with UART parsing when PMS5003 is wired.
Reading read();
}
