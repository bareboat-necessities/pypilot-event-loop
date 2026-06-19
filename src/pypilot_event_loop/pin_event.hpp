#pragma once

#include <stdint.h>

namespace pypilot_event_loop {

/** Digital pin event modes. */
enum class PinEventType : uint8_t {
    RisingEdge,
    FallingEdge,
    Change,
    HighLevel,
    LowLevel
};

/** Normalized pin event record produced by IPinEventSource. */
struct PinEvent {
    PinEventType type = PinEventType::Change;
    uint64_t timestamp_us = 0;
    uint32_t source_id = 0;
    uint32_t sequence = 0;
    bool level = false;
};

/**
 * Abstract edge/state event source for GPIO-like inputs.
 *
 * Linux gpiod sources return a native fd and are integrated with libevent.
 * Arduino interrupt/cooperative sources return -1 and are checked from tick().
 * read_event() drains one pending event and returns true when one was available.
 */
class IPinEventSource {
public:
    virtual ~IPinEventSource() = default;
    virtual bool valid() const = 0;
    virtual int native_fd() const { return -1; }
    virtual bool read_event(PinEvent& event) = 0;
};

} // namespace pypilot_event_loop
