#pragma once

#include <stddef.h>
#include <stdint.h>

#include "event.hpp"

namespace pypilot_event_loop {

/** Overflow behavior for BoundedEventQueue. */
enum class EventQueueOverflowPolicy : uint8_t {
    /** Reject the incoming event and return false. */
    RejectNew,
    /** Drop the oldest queued event, then accept the incoming event. */
    DropOldest,
    /** Drop the incoming event and return false. */
    DropNewest,
    /** Replace an existing event with matching type+source_id; reject if none exists. */
    CoalesceBySource
};

/** Queue counters useful for diagnostics and production health reporting. */
struct EventQueueStats {
    uint32_t pushed = 0;
    uint32_t popped = 0;
    uint32_t dropped = 0;
    uint32_t coalesced = 0;
    uint32_t overflow_count = 0;
    uint32_t next_sequence = 1;
};

/**
 * Fixed-capacity runtime event queue with explicit overflow policy.
 *
 * This queue is intended for normal event-loop/runtime use. It is not an ISR
 * queue and is not a lock-free cross-thread queue. Use IsrEventQueue for
 * interrupt handoff on Arduino or any other single-producer/single-consumer
 * interrupt path.
 */
template<size_t Capacity,
         EventQueueOverflowPolicy Policy = EventQueueOverflowPolicy::RejectNew>
class BoundedEventQueue {
public:
    bool push(Event event) {
        if (Capacity == 0) {
            stats_.dropped++;
            stats_.overflow_count++;
            return false;
        }

        event.sequence = stats_.next_sequence++;

        if (!full()) {
            append(event);
            return true;
        }

        stats_.overflow_count++;

        if (Policy == EventQueueOverflowPolicy::DropOldest) {
            Event ignored;
            pop_internal(ignored, false);
            stats_.dropped++;
            event.flags = static_cast<uint16_t>(event.flags | EventFlagOverflow);
            append(event);
            return true;
        }

        if (Policy == EventQueueOverflowPolicy::CoalesceBySource) {
            if (coalesce(event)) {
                stats_.coalesced++;
                return true;
            }
            stats_.dropped++;
            return false;
        }

        stats_.dropped++;
        return false;
    }

    bool pop(Event& event) {
        return pop_internal(event, true);
    }

    bool peek(Event& event) const {
        if (empty()) {
            return false;
        }
        event = events_[head_];
        return true;
    }

    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= Capacity; }
    size_t size() const { return count_; }
    size_t capacity() const { return Capacity; }

    const EventQueueStats& stats() const { return stats_; }
    uint32_t dropped() const { return stats_.dropped; }
    uint32_t overflow_count() const { return stats_.overflow_count; }
    bool overflowed() const { return stats_.overflow_count != 0; }

    void clear() {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
    }

    void reset_stats() { stats_ = EventQueueStats{}; }

private:
    void append(const Event& event) {
        events_[tail_] = event;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        ++stats_.pushed;
    }

    bool pop_internal(Event& event, bool count_pop) {
        if (empty()) {
            return false;
        }
        event = events_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        if (count_pop) {
            ++stats_.popped;
        }
        return true;
    }

    bool coalesce(const Event& event) {
        for (size_t i = 0; i < count_; ++i) {
            const size_t index = (head_ + i) % Capacity;
            if (events_[index].source_id == event.source_id && events_[index].type == event.type) {
                Event replacement = event;
                replacement.flags = static_cast<uint16_t>(replacement.flags | EventFlagCoalesced);
                events_[index] = replacement;
                return true;
            }
        }
        return false;
    }

    Event events_[Capacity == 0 ? 1 : Capacity]{};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    EventQueueStats stats_;
};

/** Backwards-compatible default event queue name. */
template<size_t Capacity>
using EventQueue = BoundedEventQueue<Capacity, EventQueueOverflowPolicy::RejectNew>;

} // namespace pypilot_event_loop
