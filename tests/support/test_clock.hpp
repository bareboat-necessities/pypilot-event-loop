#pragma once

#include "async_event_loop/clock.hpp"

namespace async_event_loop_test {

class TestClock final : public async_event_loop::IClock {
public:
    uint64_t micros() const override { return now_us_; }
    void set_us(uint64_t now_us) { now_us_ = now_us; }
    void advance_us(uint64_t delta_us) { now_us_ += delta_us; }

private:
    uint64_t now_us_ = 0;
};

} // namespace async_event_loop_test
