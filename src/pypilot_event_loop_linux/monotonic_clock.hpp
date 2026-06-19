#pragma once

#include <stdint.h>
#include <time.h>
#include "pypilot_event_loop/clock.hpp"

namespace pypilot_event_loop {

class LinuxMonotonicClock final : public IClock {
public:
    uint64_t micros() const override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec / 1000ULL);
    }
};

} // namespace pypilot_event_loop
