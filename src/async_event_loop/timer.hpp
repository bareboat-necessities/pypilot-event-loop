#pragma once

#include <stdint.h>

namespace async_event_loop {

class PeriodicTimer {
public:
    explicit PeriodicTimer(uint64_t period_us = 0) : period_us_(period_us) {}

    void set_period_us(uint64_t period_us) { period_us_ = period_us; }
    uint64_t period_us() const { return period_us_; }

    void reset(uint64_t now_us = 0) {
        next_due_us_ = now_us;
        armed_ = true;
    }

    bool ready(uint64_t now_us) {
        if (!armed_) {
            reset(now_us);
        }
        if (period_us_ == 0 || now_us < next_due_us_) {
            return false;
        }
        do {
            next_due_us_ += period_us_;
        } while (now_us >= next_due_us_);
        return true;
    }

    uint64_t next_due_us() const { return next_due_us_; }

private:
    uint64_t period_us_ = 0;
    uint64_t next_due_us_ = 0;
    bool armed_ = false;
};

class OneShotTimer {
public:
    void arm(uint64_t due_us) {
        due_us_ = due_us;
        armed_ = true;
        fired_ = false;
    }

    void disarm() { armed_ = false; }
    bool armed() const { return armed_; }
    bool fired() const { return fired_; }
    uint64_t due_us() const { return due_us_; }

    bool ready(uint64_t now_us) {
        if (!armed_ || fired_ || now_us < due_us_) {
            return false;
        }
        fired_ = true;
        armed_ = false;
        return true;
    }

private:
    uint64_t due_us_ = 0;
    bool armed_ = false;
    bool fired_ = false;
};

} // namespace async_event_loop
