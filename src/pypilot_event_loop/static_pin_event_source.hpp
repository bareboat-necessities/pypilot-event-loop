#pragma once

#include <stddef.h>

#include "pin_event.hpp"

namespace pypilot_event_loop {

template<size_t Capacity = 8>
class StaticPinEventSource final : public IPinEventSource {
public:
    bool valid() const override { return true; }

    bool push(PinEvent event) {
        if (full()) {
            ++dropped_;
            return false;
        }
        event.sequence = ++sequence_;
        events_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool read_event(PinEvent& event) override {
        if (empty()) {
            return false;
        }
        event = events_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }

    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= Capacity; }
    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }
    uint32_t dropped() const { return dropped_; }

private:
    PinEvent events_[Capacity == 0 ? 1 : Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint32_t sequence_ = 0;
    uint32_t dropped_ = 0;
};

} // namespace pypilot_event_loop
