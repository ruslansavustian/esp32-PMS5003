#pragma once
#include "measurement_source.h"
#include "pms5003_parser.h"

namespace pms5003 {
// Caller provides synchronization. Timing logic is host-testable.
class SampleStore {
  public:
    static constexpr int64_t kWarmupUs = 30'000'000;
    static constexpr int64_t kStaleUs = 10'000'000;

    void accept(const Sample &sample, int64_t now) {
        if (!has_sample_ || now - received_at_ >= kStaleUs) {
            stable_since_ = now;
        }
        sample_ = sample;
        received_at_ = now;
        has_sample_ = true;
    }
    void invalidate() {
        has_sample_ = false;
    }
    measurement_source::Status status(int64_t now) const {
        using measurement_source::Status;
        if (!has_sample_) {
            return Status::Waiting;
        }
        if (now - received_at_ >= kStaleUs) {
            return Status::Stale;
        }
        if (now - stable_since_ < kWarmupUs) {
            return Status::WarmingUp;
        }
        return Status::Ready;
    }
    bool read(int64_t now, measurement_source::Reading &out) const {
        if (status(now) != measurement_source::Status::Ready) {
            return false;
        }
        out = {static_cast<float>(sample_.pm1),
               static_cast<float>(sample_.pm25),
               static_cast<float>(sample_.pm10),
               "pms5003",
               received_at_};
        return true;
    }

  private:
    Sample sample_{};
    bool has_sample_ = false;
    int64_t received_at_ = 0;
    int64_t stable_since_ = 0;
};
} // namespace pms5003
