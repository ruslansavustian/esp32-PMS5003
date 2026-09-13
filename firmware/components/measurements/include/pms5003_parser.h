#pragma once
#include <cstddef>
#include <cstdint>

namespace pms5003 {
struct Sample {
    uint16_t pm1;
    uint16_t pm25;
    uint16_t pm10;
};

// Byte-stream parser, independent of ESP-IDF. No allocation, at most 32 bytes.
class Parser {
public:
    bool push(uint8_t byte, Sample& sample);
    void reset() { size_ = 0; }
    uint32_t bad_lengths() const { return bad_lengths_; }
    uint32_t bad_checksums() const { return bad_checksums_; }
private:
    void discard_first();
    uint8_t buffer_[32]{};
    size_t size_ = 0;
    uint32_t bad_lengths_ = 0;
    uint32_t bad_checksums_ = 0;
};
}
