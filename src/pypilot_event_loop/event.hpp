#pragma once

#include <stdint.h>
#include <stddef.h>

namespace pypilot_event_loop {

enum class EventType : uint8_t {
    None,
    Timer,
    StreamReadable,
    StreamWritable,
    StreamError,
    DatagramReadable,
    Shutdown,
    User
};

struct Event {
    EventType type = EventType::None;
    uint64_t time_us = 0;
    uint16_t source_id = 0;
    uint16_t size = 0;
    void* payload = nullptr;
};

} // namespace pypilot_event_loop
