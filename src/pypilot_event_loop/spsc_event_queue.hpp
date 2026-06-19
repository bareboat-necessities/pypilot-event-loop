#pragma once

#include <stddef.h>
#include <stdint.h>

#include "event.hpp"

namespace pypilot_event_loop {

/**
 * Single-producer/single-consumer event queue for interrupt handoff.
 *
 * The producer should call push_from_producer(); the normal event loop should
 * call pop(). The queue stores Capacity events and uses one internal sentinel
 * slot to distinguish full from empty without a shared count variable.
 */
template<size_t Capacity>
class SpscEventQueue {
public:
    bool push_from_producer(Event event) {
        const size_t next_tail = increment(tail_);
        if (next_tail == head_) {
            ++dropped_;
            return false;
        }
        event.sequence = next_sequence_++;
        event.flags = static_cast<uint16_t>(event.flags | EventFlagFromIsr);
        events_[tail_] = event;
        tail_ = next_tail;
        return true;
    }

    bool pop(Event& event) {
        if (empty()) {
            return false;
        }
        event = events_[head_];
        head_ = increment(head_);
        return true;
    }

    bool empty() const { return head_ == tail_; }
    bool full() const { return increment(tail_) == head_; }
    size_t capacity() const { return Capacity; }

    size_t size() const {
        const size_t h = head_;
        const size_t t = tail_;
        if (t >= h) {
            return t - h;
        }
        return storage_capacity() - h + t;
    }

    uint32_t dropped() const { return dropped_; }

    void clear() {
        head_ = 0;
        tail_ = 0;
    }

private:
    static constexpr size_t storage_capacity() { return Capacity + 1; }

    static size_t increment(size_t value) {
        return (value + 1) % storage_capacity();
    }

    Event events_[Capacity + 1]{};
    volatile size_t head_ = 0;
    volatile size_t tail_ = 0;
    volatile uint32_t next_sequence_ = 1;
    volatile uint32_t dropped_ = 0;
};

} // namespace pypilot_event_loop
