#pragma once

#include <stddef.h>
#include "event.hpp"

namespace pypilot_event_loop {

template<size_t Capacity>
class EventQueue {
public:
    bool push(const Event& event) {
        if (full()) {
            return false;
        }
        events_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    bool pop(Event& event) {
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

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

private:
    Event events_[Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

} // namespace pypilot_event_loop
