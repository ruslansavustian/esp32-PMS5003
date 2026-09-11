#include "pms5003_parser.h"
#include <cstring>

namespace {
uint16_t word(const uint8_t* bytes)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
}
}

namespace pms5003 {
void Parser::discard_first()
{
    --size_;
    std::memmove(buffer_, buffer_ + 1, size_);
}

bool Parser::push(uint8_t byte, Sample& sample)
{
    buffer_[size_++] = byte;
    // Retain a possible header inside a damaged candidate instead of dropping it.
    while (size_ > 0) {
        if (buffer_[0] != 0x42) { discard_first(); continue; }
        if (size_ < 2) return false;
        if (buffer_[1] != 0x4d) { discard_first(); continue; }
        if (size_ < 4) return false;
        if (word(buffer_ + 2) != 28) {
            ++bad_lengths_;
            discard_first();
            continue;
        }
        if (size_ < sizeof(buffer_)) return false;
        uint16_t checksum = 0;
        for (size_t i = 0; i < 30; ++i) checksum += buffer_[i];
        if (checksum != word(buffer_ + 30)) {
            ++bad_checksums_;
            discard_first();
            continue;
        }
        // Plantower V2.3: Data4/5/6 (atmospheric), not Data1/2/3 (CF=1).
        // Bytes 28/29 are reserved in this manual, do not interpret them as PM.
        sample = {word(buffer_ + 10), word(buffer_ + 12), word(buffer_ + 14)};
        size_ = 0;
        return true;
    }
    return false;
}
}
