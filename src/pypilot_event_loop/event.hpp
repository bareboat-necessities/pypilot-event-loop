#pragma once

#include <stdint.h>
#include <stddef.h>

namespace pypilot_event_loop {

/** Runtime event category used by bounded queues and backend dispatch. */
enum class EventKind : uint8_t {
    None,
    Timer,
    FdReadable,
    FdWritable,
    StreamReadable,
    StreamWritable,
    StreamError,
    DatagramReadable,
    PinEdge,
    I2cReady,
    Shutdown,
    User
};

using EventType = EventKind;

/** Optional bit flags carried with RuntimeEvent/Event. */
enum EventFlags : uint16_t {
    EventFlagNone = 0,
    EventFlagOverflow = 1u << 0,
    EventFlagCoalesced = 1u << 1,
    EventFlagFromIsr = 1u << 2
};

/**
 * Small POD event record.
 *
 * This type is intentionally copyable and allocation-free so it can be stored
 * in bounded queues on Linux and Arduino. The queue assigns sequence numbers
 * when the event is accepted.
 */
struct RuntimeEvent {
    EventKind type = EventKind::None;
    uint64_t time_us = 0;
    uint32_t sequence = 0;
    uint16_t source_id = 0;
    uint16_t flags = EventFlagNone;
    uint16_t size = 0;
    void* payload = nullptr;
};

using Event = RuntimeEvent;

} // namespace pypilot_event_loop
