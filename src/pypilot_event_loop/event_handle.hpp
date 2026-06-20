#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

/**
 * Stable handle for an event-loop callback slot.
 *
 * The slot identifies storage inside EventLoop. The generation prevents stale
 * handles from controlling a later callback after the slot has been removed and
 * reused.
 */
struct EventHandle {
    static constexpr uint16_t invalid_slot = 0xffffu;

    constexpr EventHandle() = default;
    constexpr EventHandle(uint16_t slot_value, uint16_t generation_value)
        : slot(slot_value), generation(generation_value) {}

    uint16_t slot = invalid_slot;
    uint16_t generation = 0;

    bool assigned() const { return slot != invalid_slot; }
    explicit operator bool() const { return assigned(); }
};

} // namespace pypilot_event_loop
