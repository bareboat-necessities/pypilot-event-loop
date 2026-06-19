#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

/**
 * Stable handle for an event-loop callback slot.
 *
 * The slot identifies storage inside EventLoop. The generation prevents stale
 * handles from controlling a later callback if slot reuse is introduced. Current
 * scheduler backends do not support true unscheduling yet, so remove() disables
 * and clears the callback but does not reclaim the slot for reuse.
 */
struct EventHandle {
    uint16_t slot = 0xffffu;
    uint16_t generation = 0;

    bool assigned() const { return slot != 0xffffu; }
};

} // namespace pypilot_event_loop
