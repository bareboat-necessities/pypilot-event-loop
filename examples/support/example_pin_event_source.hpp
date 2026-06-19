#pragma once

#include <stddef.h>

#include <pypilot_event_loop.hpp>

namespace pypilot_event_loop_examples {

template<size_t Capacity>
class ExamplePinEventSource final : public pypilot_event_loop::IPinEventSource {
public:
    bool valid() const override { return true; }

    bool push(pypilot_event_loop::PinEvent event) {
        if (count_ >= Capacity) {
            return false;
        }
        event.sequence = ++sequence_;
        events_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool read_event(pypilot_event_loop::PinEvent& event) override {
        if (count_ == 0) {
            return false;
        }
        event = events_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }

private:
    pypilot_event_loop::PinEvent events_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint32_t sequence_ = 0;
};

} // namespace pypilot_event_loop_examples
