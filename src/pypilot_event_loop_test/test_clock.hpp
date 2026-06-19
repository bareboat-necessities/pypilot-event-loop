#pragma once

#include "pypilot_event_loop/clock.hpp"

namespace pypilot_event_loop {

class TestClock final : public IClock {
public:
    uint64_t micros() const override { return now_us_; }
    void set_us(uint64_t now_us) { now_us_ = now_us; }
    void advance_us(uint64_t delta_us) { now_us_ += delta_us; }

private:
    uint64_t now_us_ = 0;
};

} // namespace pypilot_event_loop
